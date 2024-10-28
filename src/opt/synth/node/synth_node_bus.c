#include "../synth_internal.h"

#define SYNTH_BUS_FADE_OUT_TIME 1.0f
#define SYNTH_BUS_FADE_IN_TIME 1.0f

/* Instance definition.
 */
 
struct synth_node_bus {
  struct synth_node hdr;
  int rid;
  int repeat;
  int fading; // >0=out, <0=in, 0=none
  float fade_level;
  float dfade_level; // sign must agree with (fading)
  int tempo; // ms/qnote
  const uint8_t *src; // EGS song, events only. Borrowed.
  int srcc;
  int srcp;
  int playhead; // Frames since start of song.
  int pending_delay; // Frames
  double framesperms;
  float buffer[SYNTH_UPDATE_LIMIT_SAMPLES];
  struct synth_node *channel_by_chid[16]; // WEAK, they live for real in (childv).
};

#define NODE ((struct synth_node_bus*)node)

/* Cleanup.
 */
 
static void _bus_del(struct synth_node *node) {
}

/* Init.
 */
 
static int _bus_init(struct synth_node *node) {
  NODE->framesperms=(double)node->synth->rate/1000.0;
  return 0;
}

/* Something invalid detected in song events during playback.
 * Mark the bus for termination.
 */
 
static void bus_invalid_song(struct synth_node *node) {
  fprintf(stderr,"song:%d: Error around %d/%d in event stream. Terminating bus.\n",NODE->rid,NODE->srcp,NODE->srcc);
  node->finished=1; // Stop cold after this update.
  NODE->srcp=0;
  NODE->srcc=0;
}

/* Forward note and wheel events to the appropriate channel, or drop them.
 */
 
static void bus_begin_note(struct synth_node *node,uint8_t chid,uint8_t noteid,uint8_t velocity,int durms) {
  if (chid>=0x10) return;
  struct synth_node *channel=NODE->channel_by_chid[chid];
  if (!channel) return;
  synth_node_channel_begin_note(channel,noteid,velocity,durms);
}
 
static void bus_set_wheel(struct synth_node *node,uint8_t chid,uint8_t v) {
  if (chid>=0x10) return;
  struct synth_node *channel=NODE->channel_by_chid[chid];
  if (!channel) return;
  synth_node_channel_set_wheel(channel,v);
}

/* Apply any song events at the playhead time, then advance playhead by up to (framec).
 * Always returns in 1..framec.
 */
 
static int bus_update_song(struct synth_node *node,int framec) {
  #define FAIL { bus_invalid_song(node); return framec; }

  /* If there's any delay held over, we need to pay that out first.
   */
  if (NODE->pending_delay>0) {
    int updc=NODE->pending_delay;
    if (updc>framec) updc=framec;
    NODE->pending_delay-=updc;
    NODE->playhead+=updc;
    return updc;
  }
  
  /* Process real events -- stop at implicit EOF, explicit EOF, or delay.
   * Everything we're interested in has its high bit set, and is distinguished by the four high bits.
   */
  while ((NODE->srcp<NODE->srcc)&&(NODE->src[NODE->srcp]&0x80)) {
    uint8_t lead=NODE->src[NODE->srcp++];
    switch (lead&0xf0) {
      case 0x80: { // Short Note.
          if (NODE->srcp>NODE->srcc-2) FAIL
          uint8_t a=NODE->src[NODE->srcp++];
          uint8_t b=NODE->src[NODE->srcp++];
          uint8_t chid=lead&0x0f;
          uint8_t noteid=a>>1;
          uint8_t velocity=((a&0x01)<<6)|(b>>2); // 7 bit
          int durms=(b&0x03)*16;
          bus_begin_note(node,chid,noteid,velocity,durms);
        } break;
      case 0x90: { // Medium Note.
          if (NODE->srcp>NODE->srcc-2) FAIL
          uint8_t a=NODE->src[NODE->srcp++];
          uint8_t b=NODE->src[NODE->srcp++];
          uint8_t chid=lead&0x0f;
          uint8_t noteid=a>>1;
          uint8_t velocity=((a&0x01)<<6)|((b&0xe0)>>2); // 4 bit
          velocity|=velocity>>4;
          int durms=((b&0x1f)+1)<<6;
          bus_begin_note(node,chid,noteid,velocity,durms);
        } break;
      case 0xa0: { // Long Note.
          if (NODE->srcp>NODE->srcc-2) FAIL
          uint8_t a=NODE->src[NODE->srcp++];
          uint8_t b=NODE->src[NODE->srcp++];
          uint8_t chid=lead&0x0f;
          uint8_t noteid=a>>1;
          uint8_t velocity=((a&0x01)<<6)|((b&0xe0)>>2); // 4 bit
          velocity|=velocity>>4;
          int durms=((b&0x1f)+1)<<9;
          bus_begin_note(node,chid,noteid,velocity,durms);
        } break;
      case 0xb0: { // Wheel.
          if (NODE->srcp>NODE->srcc-1) FAIL
          uint8_t v=NODE->src[NODE->srcp++];
          uint8_t chid=lead&0x0f;
          bus_set_wheel(node,chid,v);
        } break;
      default: FAIL // All else is reserved and illegal.
    }
  }
  
  /* Collect delays.
   * It's perfectly normal for there to be multiple delays in a row, and we improve throughput by coalescing them.
   * The usual case is there's one Long Delay (64..4096ms) followed by one Short Delay (1..63ms).
   */
  int delay=0;
  for (;;) {
  
    /* EOF (implicit or explicit).
     */
    if ((NODE->srcp>=NODE->srcc)||!NODE->src[NODE->srcp]) {
      if (delay) break;
      if (NODE->repeat) {
        // 1 frame delay at the turnover.
        // But if the song is empty (ie srcp==0), report a long fake delay instead.
        int fake_delay=(NODE->srcp?1:framec);
        NODE->srcp=0;
        NODE->playhead=0;
        return fake_delay;
      } else {
        NODE->srcp=NODE->srcc;
        synth_node_bus_terminate(node);
        return framec;
      }
    }
    
    /* Non-delay event. Stop processing.
     */
    if (NODE->src[NODE->srcp]&0x80) break;
    
    uint8_t event=NODE->src[NODE->srcp++];
    int ms=event&0x3f;
    if (event&0x40) ms=(ms+1)<<6;
    delay+=ms;
  }
  
  /* Finally, convert this delay to frames, force positive, and apply it.
   */
  NODE->pending_delay=(int)(delay*NODE->framesperms);
  if (NODE->pending_delay<1) NODE->pending_delay=1;
  int updc=NODE->pending_delay;
  if (updc>framec) updc=framec;
  NODE->pending_delay-=updc;
  NODE->playhead+=updc;
  #undef FAIL
  return updc;
}

/* Read song and update channels.
 */
 
static void bus_generate_signal(float *v,int framec,struct synth_node *node) {
  while (framec>0) {
    int updc=bus_update_song(node,framec);
    int i=node->childc;
    while (i-->0) {
      struct synth_node *channel=node->childv[i];
      channel->update(v,updc,channel);
    }
    v+=updc*node->chanc;
    framec-=updc;
  }
}

/* Fade out or back in.
 */
 
static void bus_apply_fade_mono(float *v,int framec,struct synth_node *node) {
  int i=framec;
  for (;i-->0;v++) {
    (*v)*=NODE->fade_level;
    NODE->fade_level+=NODE->dfade_level;
    if (NODE->fade_level<=0.0f) { // End of fade-out.
      NODE->fade_level=0;
      node->finished=1;
      memset(v+1,0,sizeof(float)*i);
      return;
    } else if (NODE->fade_level>=1.0f) { // End of fade-out cancel.
      NODE->fade_level=1.0;
      NODE->fading=0;
      return;
    }
  }
}
 
static void bus_apply_fade_stereo(float *v,int framec,struct synth_node *node) {
  int i=framec;
  for (;i-->0;v+=2) {
    v[0]*=NODE->fade_level;
    v[1]*=NODE->fade_level;
    NODE->fade_level+=NODE->dfade_level;
    if (NODE->fade_level<=0.0f) { // End of fade-out.
      NODE->fade_level=0;
      node->finished=1;
      memset(v+2,0,sizeof(float)*i*2);
      return;
    } else if (NODE->fade_level>=1.0f) { // End of fade-out cancel.
      NODE->fade_level=1.0;
      NODE->fading=0;
      return;
    }
  }
}

/* Update.
 */
 
static void _bus_update(float *v,int framec,struct synth_node *node) {

  /* If we're fading, we have to update into a buffer, apply the fade, then add that to output.
   */
  if (NODE->fading) {
    int samplec=framec*node->chanc;
    memset(NODE->buffer,0,sizeof(float)*samplec);
    bus_generate_signal(NODE->buffer,framec,node);
    if (node->chanc==1) bus_apply_fade_mono(NODE->buffer,framec,node);
    else bus_apply_fade_stereo(NODE->buffer,framec,node);
    synth_signal_add(v,NODE->buffer,samplec);
  
  /* No fade in play, we can run direct against the main.
   */
  } else {
    bus_generate_signal(v,framec,node);
  }
}

/* Ready.
 */
 
static int _bus_ready(struct synth_node *node) {
  node->update=_bus_update;
  int i;
  for (i=node->childc;i-->0;) {
    struct synth_node *channel=node->childv[i];
    int chid=synth_node_channel_get_chid(channel);
    if ((chid<0)||(chid>=0x10)) {
      synth_node_remove_child(node,channel);
      continue;
    }
    NODE->channel_by_chid[chid]=channel;
    if (synth_node_ready(channel)<0) return -1;
  }
  return 0;
}

/* Type definition.
 */
 
const struct synth_node_type synth_node_type_bus={
  .name="bus",
  .objlen=sizeof(struct synth_node_bus),
  .del=_bus_del,
  .init=_bus_init,
  .ready=_bus_ready,
};

/* Measure channel header.
 * Caller must check for the terminator 0xff and not call us with those.
 */
 
static int bus_measure_chhdr(const uint8_t *src,int srcc) {
  if (srcc<6) return -1;
  int srcp=6;
  int modelen=(src[4]<<8)|src[5];
  if (srcp>srcc-modelen) return -1;
  srcp+=modelen;
  if (srcp>srcc-2) return -1;
  int postlen=(src[srcp]<<8)|src[srcp+1];
  srcp+=2;
  if (srcp>srcc-postlen) return -1;
  srcp+=postlen;
  return srcp;
}

/* Configure.
 */
 
int synth_node_bus_configure(struct synth_node *node,const void *src,int srcc,int repeat,int rid) {
  if (!node||(node->type!=&synth_node_type_bus)||node->ready) return -1;
  if (node->childc) return -1; // Already configured!
  if (!src||(srcc<6)||memcmp(src,"\0EGS",4)) return -1;
  NODE->repeat=repeat;
  NODE->rid=rid;
  const uint8_t *SRC=src;
  NODE->tempo=(SRC[4]<<8)|SRC[5];
  int srcp=6;
  
  /* Channel headers.
   */
  while (srcp<srcc) {
    if (SRC[srcp]==0xff) { srcp++; break; }
    int len=bus_measure_chhdr(SRC+srcp,srcc-srcp);
    if (len<1) return -1;
    if (!SRC[srcp+1]||!SRC[srcp+3]) { // Master or Mode zero, noop channel. Skip it.
      srcp+=len;
      continue;
    }
    struct synth_node *channel=synth_node_spawn(node,&synth_node_type_channel,0);
    if (!channel) return -1;
    if (rid<0) synth_node_channel_enable_global_trim(channel,0);
    if (
      (synth_node_channel_configure(channel,SRC+srcp,len)<0)||
      (synth_node_ready(channel)<0)
    ) {
      synth_node_remove_child(node,channel);
      return -1;
    }
    srcp+=len;
  }
  
  /* Remainder of (src) is events. Note them.
   */
  NODE->src=SRC+srcp;
  NODE->srcc=srcc-srcp;
  
  return 0;
}

/* Trivial accessors.
 */

int synth_node_bus_get_rid(const struct synth_node *node) {
  if (!node||(node->type!=&synth_node_type_bus)) return 0;
  return NODE->rid;
}

double synth_node_bus_get_playhead(const struct synth_node *node) {
  if (!node||(node->type!=&synth_node_type_bus)) return 0.0;
  return (double)NODE->playhead/(double)node->synth->rate;
}

int synth_node_bus_is_terminating(const struct synth_node *node) {
  if (!node||(node->type!=&synth_node_type_bus)) return 1;
  return (NODE->fading>0);
}

int synth_node_bus_set_repeat(struct synth_node *node,int repeat) {
  if (!node||(node->type!=&synth_node_type_bus)) return -1;
  NODE->repeat=repeat;
  return 0;
}

/* Set playhead.
 */

void synth_node_bus_set_playhead(struct synth_node *node,double s) {
  if (!node||(node->type!=&synth_node_type_bus)) return;
  //TODO
}

/* Terminate.
 */
 
int synth_node_bus_terminate(struct synth_node *node) {
  if (!node||(node->type!=&synth_node_type_bus)) return -1;
  if (NODE->fading>0) return 0;
  if (!NODE->fading) NODE->fade_level=1.0f; // but if already fading in, keep the existing level
  NODE->fading=1;
  NODE->dfade_level=-1.0f/(node->synth->rate*SYNTH_BUS_FADE_OUT_TIME);
  return 0;
}

/* Unterminate.
 */
 
int synth_node_bus_unterminate(struct synth_node *node) {
  if (!node||(node->type!=&synth_node_type_bus)) return -1;
  if (NODE->fading<=0) return 0;
  NODE->fading=-1;
  NODE->dfade_level=1.0f/(node->synth->rate*SYNTH_BUS_FADE_IN_TIME);
  return 0;
}

/* Plain MIDI event.
 */
 
void synth_node_bus_midi_event(struct synth_node *node,uint8_t chid,uint8_t opcode,uint8_t a,uint8_t b,int durms) {
  if (!node||(node->type!=&synth_node_type_bus)||!node->ready) return;
  //TODO
}

/* Get song duration in ms.
 */
 
int synth_node_bus_get_duration(const struct synth_node *node) {
  if (!node||(node->type!=&synth_node_type_bus)) return 0;
  int dur=0,srcp=0;
  while (srcp<NODE->srcc) {
    uint8_t lead=NODE->src[srcp++];
    if (!lead) break;
    if (lead&0x80) switch (lead&0xf0) {
      case 0x80: srcp+=2; break;
      case 0x90: srcp+=2; break;
      case 0xa0: srcp+=2; break;
      case 0xb0: srcp+=1; break;
      default: return -1;
    } else {
      int delay=lead&0x3f;
      if (lead&0x40) delay=(delay+1)<<6;
      dur+=delay;
    }
  }
  return dur;
}
