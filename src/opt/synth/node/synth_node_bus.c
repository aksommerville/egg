#include "../synth_internal.h"

// To avoid rounding error, fade-out must advance by at least 1/100k each frame.
// This forces a maximum fade-out time of a little over 2 seconds, at typical output rates.
#define BUS_DFADE_MIN 0.00001f

/* Object definition.
 */
 
struct synth_node_bus {
  struct synth_node hdr;
  int songid;
  int repeat;
  struct synth_node *channel_by_chid[SYNTH_CHANNEL_COUNT]; // WEAK, SPARSE. Addressable channels.
  struct synth_node **channelv;
  int channelc,channela;
  double framesperms;
  int songdelay; // Frame count to next song event.
  int harddelay; // If >0, skip so many frames hard. Don't update song, don't generate a signal.
  int fadeenable; // -1,0,1=cancelling,none,fading
  float fade;
  float dfade; // Per frame.
  const uint8_t *evv; // Encoded song, events only.
  int evc;
  int evp;
};

#define NODE ((struct synth_node_bus*)node)

/* Delete.
 */
 
static void _bus_del(struct synth_node *node) {
  if (NODE->channelv) {
    while (NODE->channelc-->0) synth_node_del(NODE->channelv[NODE->channelc]);
    free(NODE->channelv);
  }
}

/* Remove all WEAK references to a channel, presumably has just been deleted and removed from (NODE->channelv).
 */
 
static void synth_node_bus_unlist_channel(struct synth_node *node,struct synth_node *channel) {
  int i=SYNTH_CHANNEL_COUNT;
  while (i-->0) {
    if (NODE->channel_by_chid[i]==channel) {
      NODE->channel_by_chid[i]=0;
    }
  }
}

/* Apply fade.
 * Call only if (NODE->fadeenable) nonzero.
 * Set (node->finished) if we strike zero.
 */
 
static void synth_node_bus_update_fade(float *v,int framec,struct synth_node *node) {
  if (node->chanc==1) {
    if (NODE->fadeenable>0) { // Cancelling, mono.
      for (;framec-->0;v++) {
        NODE->fade+=NODE->dfade;
        if (NODE->fade>=1.0f) {
          NODE->fade=1.0f;
          NODE->fadeenable=0;
          return;
        }
        (*v)*=NODE->fade;
      }
    } else if (NODE->fadeenable<0) { // Fading, mono.
      for (;framec-->0;v++) {
        NODE->fade+=NODE->dfade;
        if (NODE->fade<=0.0f) {
          NODE->fade=0.0f;
          NODE->fadeenable=0;
          node->finished=1;
          memset(v,0,sizeof(float)*(framec+1));
          return;
        }
        (*v)*=NODE->fade;
      }
    }
  } else {
    if (NODE->fadeenable>0) { // Cancelling, stereo.
      for (;framec-->0;v+=2) {
        NODE->fade+=NODE->dfade;
        if (NODE->fade>=1.0f) {
          NODE->fade=1.0f;
          NODE->fadeenable=0;
          return;
        }
        v[0]*=NODE->fade;
        v[1]*=NODE->fade;
      }
    } else if (NODE->fadeenable<0) { // Fading, stereo.
      for (;framec-->0;v+=2) {
        NODE->fade+=NODE->dfade;
        if (NODE->fade<=0.0f) {
          NODE->fade=0.0f;
          NODE->fadeenable=0;
          node->finished=1;
          memset(v,0,sizeof(float)*2*(framec+1));
          return;
        }
        v[0]*=NODE->fade;
        v[1]*=NODE->fade;
      }
    }
  }
}

/* Update song.
 * Called repeatedly during update, whether a song exists or not.
 * Must return in (1..limit), how many signal frames to update.
 * Song player state will have updated by that count.
 */
 
static int synth_node_bus_update_song(struct synth_node *node,int limit) {
  //fprintf(stderr,"%s limit=%d songdelay=%d evp=%d/%d\n",__func__,limit,NODE->songdelay,NODE->evp,NODE->evc);
  if (limit<1) return 1;
  
  // If we have some delay pending, pay it out.
  if (NODE->songdelay>0) {
    int framec=NODE->songdelay;
    if (framec>limit) framec=limit;
    NODE->songdelay-=framec;
    return framec;
  }
  
  // Read next song event, repeat until we acquire a delay or song ends.
  for (;;) {
    
    // End of song?
    if ((NODE->evp>=NODE->evc)||!NODE->evv[NODE->evp]) {
      fprintf(stderr,"%s: End of song. repeat=%d\n",__func__,NODE->repeat);
      if (NODE->repeat) {
        // Report a minimal delay at each repeat, as a safety valve against zero-delay songs.
        // If there really is zero delay and repeat, this is still going to be a catastrophe.
        // TODO Determine how bad it could get, and mitigate harder if need be.
        NODE->evp=0;
        return 1;
      }
      // We're not actually able to turn the song off.
      // So instead, just delay for 2 gigaframes, and we'll see you again in a month.
      NODE->songdelay=INT_MAX;
      return limit;
    }
    
    /* Read the next event. Natural zeroes are already handled.
     * 00000000                   : End of Song.
     * 00tttttt                   : Fine Delay, (t) ms. (t) nonzero.
     * 01tttttt                   : Coarse Delay, ((t+1)*64) ms.
     * 1000cccc nnnnnnnv vvvvvvxx : Fire and Forget. (c) chid, (n) noteid, (v) velocity.
     * 1001cccc nnnnnnnv vvvttttt : Short Note. (c) chid, (n) noteid, (v) velocity 4-bit, (t) hold time (t+1)*16ms
     * 1010cccc nnnnnnnv vvvttttt : Long Note. (c) chid, (n) noteid, (v) velocity 4-bit, (t) hold time (t+1)*128ms
     * 1011cccc wwwwwwww          : Pitch Wheel. (c) chid, (w) wheel position. 0x80 is neutral.
     * 11xxxxxx                   : Reserved, decoder must fail.
     */
    uint8_t lead=NODE->evv[NODE->evp++];
    
    // Delay? Coalesce adjacent delay events. They're all single bytes. But watch out for zeroes.
    // A delay event interrupts event processing no matter what.
    if (!(lead&0x80)) {
      int ms;
      if (lead&0x40) { // Coarse Delay.
        ms=((lead&0x3f)+1)<<6;
      } else { // Fine Delay.
        ms=lead;
      }
      while (NODE->evp<NODE->evc) {
        lead=NODE->evv[NODE->evp];
        if (!lead) break;
        if (lead&0x80) break;
        if (lead&0x40) ms+=((lead&0x3f)+1)<<6;
        else ms+=lead;
        NODE->evp++;
      }
      NODE->songdelay=(int)((double)ms*NODE->framesperms);
      if (NODE->songdelay<1) NODE->songdelay=1;
      int framec=NODE->songdelay;
      if (framec>limit) framec=limit;
      NODE->songdelay-=framec;
      return framec;
    }
    
    // All other events are identified by the top 4 bits.
    #define ILLEGAL_EVENT { \
      fprintf(stderr,"%s: Invalid song, stopping playback.\n",__func__); \
      NODE->evp=0; \
      NODE->evc=0; \
      NODE->songdelay=INT_MAX; \
      return limit; \
    }
    switch (lead&0xf0) {
      case 0x80:
      case 0x90:
      case 0xa0: {
          if (NODE->evp>NODE->evc-2) ILLEGAL_EVENT
          uint8_t s1=NODE->evv[NODE->evp++];
          uint8_t s2=NODE->evv[NODE->evp++];
          uint8_t noteid=s1>>1;
          uint8_t velocity=((s1&1)<<6)|(s2>>2);
          int durms=0;
          switch (lead&0xf0) {
            case 0x90: {
                velocity&=0x78;
                velocity|=velocity>>4;
                durms=((s2&0x1f)+1)<<4;
              } break;
            case 0xa0: {
                velocity&=0x78;
                velocity|=velocity>>4;
                durms=((s2&0x1f)+1)<<7;
              } break;
          }
          synth_node_bus_event(node,lead&0x0f,0x98,noteid,velocity,durms);
        } break;
      case 0xb0: {
          if (NODE->evp>NODE->evc-1) ILLEGAL_EVENT
          uint8_t s1=NODE->evv[NODE->evp++];
          uint8_t b=s1>>1;
          uint8_t a=((s1&1)?0x40:0)|(s1&0x3f);
          synth_node_bus_event(node,lead&0x0f,0xe0,a,b,0);
        } break;
      default: ILLEGAL_EVENT
    }
    #undef ILLEGAL_EVENT
  }
  
  return limit;
}

/* Main update.
 */
 
static void synth_node_bus_update_signal(float *v,int framec,struct synth_node *node) {
  int i=NODE->channelc;
  while (i-->0) {
    struct synth_node *channel=NODE->channelv[i];
    channel->update(v,framec,channel);
    if (channel->finished) {
      NODE->channelc--;
      memmove(NODE->channelv+i,NODE->channelv+i+1,sizeof(void*)*(NODE->channelc-i));
      synth_node_del(channel);
      synth_node_bus_unlist_channel(node,channel);
    }
  }
  if (NODE->fadeenable) synth_node_bus_update_fade(v,framec,node);
}

static void _bus_update(float *v,int framec,struct synth_node *node) {
  if (node->finished) return;

  // Hard delay.
  if (NODE->harddelay>0) {
    int skipc=NODE->harddelay;
    if (skipc>framec) skipc=framec;
    v+=skipc*node->chanc;
    framec-=skipc;
    NODE->harddelay-=skipc;
  }
  
  // Interleave song updates and signal updates until we run out of buffer.
  while (framec>0) {
    int updc=synth_node_bus_update_song(node,framec);
    synth_node_bus_update_signal(v,updc,node);
    v+=updc*node->chanc;
    framec-=updc;
  }
}

/* Init.
 */
 
static int _bus_init(struct synth_node *node) {
  node->update=_bus_update;
  NODE->fade=1.0f;
  NODE->framesperms=(double)node->synth->rate/1000.0;
  return 0;
}

/* Ready.
 */
 
static int _bus_ready(struct synth_node *node) {
  int i,err;
  
  //TODO Verify configuration.
  
  // Ready all channels.
  struct synth_node **pp=NODE->channelv;
  for (i=NODE->channelc;i-->0;pp++) {
    if ((err=synth_node_ready(*pp))<0) return err;
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

/* Apply channel config.
 */
 
static int synth_node_bus_configure_channel(struct synth_node *node,int chid,const uint8_t *src,int srcc) {
  if ((chid<0)||(chid>=SYNTH_CHANNEL_COUNT)||NODE->channel_by_chid[chid]) return -1;
  if (NODE->channelc>=NODE->channela) {
    int na=NODE->channela+8;
    if (na>INT_MAX/sizeof(void*)) return -1;
    void *nv=realloc(NODE->channelv,sizeof(void*)*na);
    if (!nv) return -1;
    NODE->channelv=nv;
    NODE->channela=na;
  }
  struct synth_node *channel=synth_node_new(node->synth,&synth_node_type_channel,node->chanc);
  if (!channel) return -1;
  NODE->channelv[NODE->channelc++]=channel;
  NODE->channel_by_chid[chid]=channel;
  synth_node_channel_setup(channel,chid,node);
  if (synth_node_channel_configure(channel,src,srcc)<0) return -1;
  return 0;
}

/* Receive encoded config.
 */
 
int synth_node_bus_configure(struct synth_node *node,const void *src,int srcc) {
  if (!node||(node->type!=&synth_node_type_bus)||node->ready) return -1;
  int err;
  struct synth_song_parts parts={0};
  if (synth_song_split(&parts,src,srcc)<0) return -1;
  int chid=0; for (;chid<SYNTH_CHANNEL_COUNT;chid++) {
    const uint8_t *chsrc=parts.channels[chid].v;
    int chsrcc=parts.channels[chid].c;
    while ((chsrcc>=2)&&(chsrc[0]==0x00)) { // Skip leading noops.
      int skipc=2+chsrc[1];
      chsrc+=skipc;
      chsrcc-=skipc;
    }
    if (chsrcc<1) continue;
    if ((err=synth_node_bus_configure_channel(node,chid,chsrc,chsrcc))<0) return err;
  }
  NODE->evv=parts.events;
  NODE->evc=parts.eventsc;
  NODE->evp=0;
  return 0;
}

/* Set songid and repeat.
 */

void synth_node_bus_set_songid(struct synth_node *node,int songid,int repeat) {
  if (!node||(node->type!=&synth_node_type_bus)||node->ready) return;
  NODE->songid=songid;
  NODE->repeat=repeat;
}

/* Get songid.
 */
 
int synth_node_bus_get_songid(struct synth_node *node) {
  if (!node||(node->type!=&synth_node_type_bus)||node->ready) return 0;
  return NODE->songid;
}

/* Prepare fade-out.
 */

void synth_node_bus_fade_out(struct synth_node *node,int framec,int force) {
  if (!node||(node->type!=&synth_node_type_bus)||node->ready) return;
  if ((framec<1)||NODE->harddelay) { // Immediate.
    NODE->fadeenable=1;
    NODE->dfade=-1.0f;
    NODE->fade=0.0f;
    node->finished=1;
  } else if (force||(NODE->fadeenable<1)) {
    NODE->fadeenable=1;
    NODE->dfade=-1.0f/(float)framec;
    if (NODE->dfade>-BUS_DFADE_MIN) NODE->dfade=-BUS_DFADE_MIN;
  }
}

/* Cancel fade-out.
 */
 
void synth_node_bus_cancel_fade(struct synth_node *node) {
  if (!node||(node->type!=&synth_node_type_bus)||node->ready||node->finished) return;
  if (NODE->fadeenable>0) {
    NODE->fadeenable=-1;
    NODE->dfade=-NODE->dfade;
  }
}

/* Delay signal.
 */
 
void synth_node_bus_wait(struct synth_node *node,int framec) {
  if (!node||(node->type!=&synth_node_type_bus)||node->ready) return;
  if (framec<0) framec=0;
  NODE->harddelay=framec;
}

/* Event from outside.
 */

void synth_node_bus_event(struct synth_node *node,uint8_t chid,uint8_t opcode,uint8_t a,uint8_t b,int durms) {
  if (!node||(node->type!=&synth_node_type_bus)||!node->ready) return;
  //fprintf(stderr,"%s node=%p chid=%d opcode=0x%02x a=0x%02x b=0x%02x durms=%d\n",__func__,node,chid,opcode,a,b,durms);
  if (chid>=SYNTH_CHANNEL_COUNT) return;
  struct synth_node *channel=NODE->channel_by_chid[chid];
  if (!channel) return;
  synth_node_channel_event(channel,chid,opcode,a,b,durms);
}
