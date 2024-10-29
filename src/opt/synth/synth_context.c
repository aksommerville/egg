#include "synth_internal.h"

/* Delete.
 */
 
static void synth_res_cleanup(struct synth_res *res) {
  if (res->pcm) synth_pcm_del(res->pcm);
}
 
void synth_del(struct synth *synth) {
  if (!synth) return;
  if (synth->qbuf) free(synth->qbuf);
  if (synth->sine) free(synth->sine);
  while (synth->voicec-->0) synth_voice_del(synth->voicev[synth->voicec]);
  if (synth->printerv) {
    while (synth->printerc-->0) synth_printer_del(synth->printerv[synth->printerc]);
    free(synth->printerv);
  }
  if (synth->resv) {
    while (synth->resc-->0) synth_res_cleanup(synth->resv+synth->resc);
    free(synth->resv);
  }
  while (synth->channelc-->0) synth_channel_cleanup(synth->channelv+synth->channelc);
  if (synth->songown) free(synth->songown);
  free(synth);
}

/* Generate rates table.
 */
 
static void synth_generate_rates(struct synth *synth) {
  // Abstractly: hz=440.0f*powf(2.0f,(noteid-69.0f)/12.0f)
  // But we'll cheat it a little. Generate the octave starting at A4 (0x45, 440 Hz),
  // then half and double it the rest of the way.
  // We could multiply our way up that first octave too, but I think the risk of rounding error is too high.
  
  float *ref=synth->rate_by_noteid+0x45;
  *ref=440.0f/(float)synth->rate;
  int i=1;
  for (;i<12;i++) ref[i]=ref[0]*powf(2.0f,(float)i/12.0f);
  
  float *p=ref+12;
  for (i=0x45+12;i<128;i++,p++) *p=p[-12]*2.0f;
  for (i=0x45,p=ref-1;i-->0;p--) *p=p[12]*0.5f;
}

/* New.
 */
 
struct synth *synth_new(int rate,int chanc) {
  if ((rate<SYNTH_RATE_MIN)||(rate>SYNTH_RATE_MAX)) return 0;
  if ((chanc<SYNTH_CHANC_MIN)||(chanc>SYNTH_CHANC_MAX)) return 0;
  struct synth *synth=calloc(1,sizeof(struct synth));
  if (!synth) return 0;
  
  synth->rate=rate;
  synth->chanc=chanc;
  synth->framesperms=(double)rate/1000.0;
  synth_generate_rates(synth);
  
  return synth;
}

/* End current song.
 */
 
static void synth_end_song(struct synth *synth) {
  int i=synth->voicec;
  while (i-->0) {
    struct synth_voice *voice=synth->voicev[i];
    if (!voice->song) continue;
    voice->release(voice);
  }
  struct synth_channel *channel=synth->channelv;
  for (i=SYNTH_CHANNEL_COUNT;i-->0;channel++) {
    synth_channel_cleanup(channel);
  }
  synth->songp=0;
  synth->songc=0;
  synth->song=0;
  synth->songid=0;
  synth->songrepeat=0;
  synth->songdelay=0;
  synth->playhead=0;
  if (synth->songown) {
    free(synth->songown);
    synth->songown=0;
  }
}

/* Report error and end song.
 */
 
static void synth_song_error(struct synth *synth) {
  fprintf(stderr,"egg:ERROR: Terminating song:%d due to error around %d/%d in events.\n",synth->songid,synth->songp,synth->songc);
  synth_end_song(synth);
}

/* Apply any pending song events and return duration in frames available to synthesize.
 * (limit) in frames and must be >=1.
 * Returns 1..limit.
 */
 
static int synth_update_song(struct synth *synth,int limit) {

  // No song? Take the entire buffer.
  if (!synth->songc) return limit;
  
  // Delay already pending?
  if (synth->songdelay>0) {
   _apply_delay_:
    int updc=synth->songdelay;
    if (updc>limit) updc=limit;
    synth->songdelay-=updc;
    synth->playhead+=updc;
    return updc;
  }
  
  // Process real events.
  // All real (non-delay) events have a set high bit.
  while ((synth->songp<synth->songc)&&(synth->song[synth->songp]&0x80)) {
    uint8_t lead=synth->song[synth->songp++];
    switch (lead&0xf0) {
      case 0x80:
      case 0x90:
      case 0xa0: {
          if (synth->songp>synth->songc-2) {
            synth_song_error(synth);
            break;
          }
          uint8_t a=synth->song[synth->songp++];
          uint8_t b=synth->song[synth->songp++];
          uint8_t chid=lead&0x0f;
          uint8_t noteid=a>>1;
          uint8_t velocity=((a<<6)&0x40)|(b>>2);
          int durms;
          switch (lead&0xf0) {
            case 0x80: durms=(b&0x03)<<4; break;
            case 0x90: velocity&=0x78; velocity|=velocity>>4; durms=((b&0x1f)+1)<<6; break;
            case 0xa0: velocity&=0x78; velocity|=velocity>>4; durms=((b&0x1f)+1)<<9; break;
          }
          synth_event(synth,chid,0x98,noteid,velocity,durms);
        } break;
      default: {
          synth_song_error(synth);
          break;
        }
    }
  }
  
  // Acquire next delay if there is any.
  while ((synth->songp<synth->songc)&&!(synth->song[synth->songp]&0x80)) {
    int delay=synth->song[synth->songp++];
    if (!delay) { // Oops, explicit EOF, not a delay.
      synth->songc=synth->songp-1;
      break;
    }
    if (delay&0x40) delay=((delay&0x3f)+1)*64;
    synth->songdelay+=delay;
  }
  if (synth->songdelay>0) {
    synth->songdelay=lround(synth->songdelay*synth->framesperms);
    goto _apply_delay_;
  }
  
  // If we're at the end, either repeat or terminate.
  if (synth->songp>=synth->songc) {
    if (synth->songrepeat) {
      synth->songp=0;
      synth->playhead=0;
      return 1; // Single frame update when we repeat, just in case there's no delay events.
    }
    synth_end_song(synth);
  }
  
  return limit;
}

/* Run all printers and reap defunct ones.
 */
 
static void synth_update_printers(struct synth *synth,int c) {
  int i=synth->printerc;
  while (i-->0) {
    struct synth_printer *printer=synth->printerv[i];
    if (synth_printer_update(printer,c)<=0) {
    
      // There can be a substantial amount of silence at the end. Trim it.
      struct synth_pcm *pcm=printer->pcm;
      double pe=0.000001;
      double ne=-pe;
      while (pcm->c&&(pcm->v[pcm->c-1]>ne)&&(pcm->v[pcm->c-1]<pe)) pcm->c--;
  
      synth->printerc--;
      memmove(synth->printerv+i,synth->printerv+i+1,sizeof(void*)*(synth->printerc-i));
      synth_printer_del(printer);
    }
  }
}

/* Update, float, single pass.
 * (v) must be zeroed first.
 */
 
static void synth_update_signal(float *v,int c,struct synth *synth) {
  int i=synth->voicec;
  while (i-->0) {
    struct synth_voice *voice=synth->voicev[i];
    voice->update(v,c,voice);
    if (voice->finished) {
      synth->voicec--;
      memmove(synth->voicev+i,synth->voicev+i+1,sizeof(void*)*(synth->voicec-i));
      synth_voice_del(voice);
    }
  }
}

/* Update, float, unbound, single channel.
 * We overwrite (v) blindly.
 */
 
static void synth_updatef_mono(float *v,int c,struct synth *synth) {
  synth_update_printers(synth,c);
  synth->preprintc=c;
  memset(v,0,sizeof(float)*c);
  while (c>0) {
    int updc=synth_update_song(synth,c);
    synth_update_signal(v,updc,synth);
    v+=updc;
    c-=updc;
  }
  synth->preprintc=0;
}

/* Update, float, unbound, multi-channel.
 */

void synth_updatef(float *v,int c,struct synth *synth) {
  if (synth->chanc>1) {
    int framec=c/synth->chanc;
    synth_updatef_mono(v,framec,synth);
    const float *src=v+framec;
    float *dst=v+c;
    int i=framec;
    while (i-->0) {
      src--;
      int ii=synth->chanc;
      while (ii-->0) *(--dst)=*src;
    }
  } else {
    synth_updatef_mono(v,c,synth);
  }
}

/* Float to int.
 */
 
static void synth_quantize(int16_t *dst,const float *src,int c) {
  for (;c-->0;dst++,src++) {
    if (*src<=-1.0f) *dst=-32768;
    else if (*src>=1.0f) *dst=32767;
    else *dst=(int16_t)(*src);
  }
}

/* Update, int, unbound, multi-channel.
 */
 
void synth_updatei(int16_t *v,int c,struct synth *synth) {

  /* Acquire qbuf if we don't have it yet.
   * Arbitrary size limit here, we can change to whatever but it must be a multiple of (chanc).
   */
  if (!synth->qbufa) {
    int framec=1024;
    int samplec=framec*synth->chanc;
    if (!(synth->qbuf=malloc(sizeof(float)*samplec))) {
      memset(v,0,c<<1);
      return;
    }
    synth->qbufa=samplec;
  }
  
  /* Update into (qbuf), then quantize into (v).
   */
  while (c>0) {
    int updc=synth->qbufa;
    if (updc>c) updc=c;
    synth_updatef(synth->qbuf,updc,synth);
    synth_quantize(v,synth->qbuf,updc);
    v+=updc;
    c-=updc;
  }
}

/* Resource list primitives.
 */
 
static int synth_resv_search(const struct synth *synth,int tid,int rid) {
  if (synth->resc) { // "Absent and belonging at the back" comes up often during init and would otherwise be worst-case for the search.
    const struct synth_res *q=synth->resv+synth->resc-1;
    if (tid>q->tid) return -synth->resc-1;
    if ((tid==q->tid)&&(rid>q->rid)) return -synth->resc-1;
  }
  int lo=0,hi=synth->resc;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
    const struct synth_res *q=synth->resv+ck;
         if (tid<q->tid) hi=ck;
    else if (tid>q->tid) lo=ck+1;
    else if (rid<q->rid) hi=ck;
    else if (rid>q->rid) lo=ck+1;
    else return ck;
  }
  return -lo-1;
}

static struct synth_res *synth_resv_insert(struct synth *synth,int p,int tid,int rid,const void *src,int srcc) {
  if ((p<0)||(p>synth->resc)) return 0;
  if (synth->resc>=synth->resa) {
    int na=synth->resa+32;
    if (na>INT_MAX/sizeof(struct synth_res)) return 0;
    void *nv=realloc(synth->resv,sizeof(struct synth_res)*na);
    if (!nv) return 0;
    synth->resv=nv;
    synth->resa=na;
  }
  struct synth_res *res=synth->resv+p;
  memmove(res+1,res,sizeof(struct synth_res)*(synth->resc-p));
  synth->resc++;
  memset(res,0,sizeof(struct synth_res));
  res->tid=tid;
  res->rid=rid;
  res->src=src;
  res->srcc=srcc;
  return res;
}

/* Install sound or song, public.
 */

int synth_install_sound(struct synth *synth,int rid,const void *src,int srcc) {
  int p=synth_resv_search(synth,EGG_TID_sound,rid);
  if (p>=0) return -1;
  p=-p-1;
  if (!synth_resv_insert(synth,p,EGG_TID_sound,rid,src,srcc)) return -1;
  return 0;
}

int synth_install_song(struct synth *synth,int rid,const void *src,int srcc) {
  int p=synth_resv_search(synth,EGG_TID_song,rid);
  if (p>=0) return -1;
  p=-p-1;
  if (!synth_resv_insert(synth,p,EGG_TID_song,rid,src,srcc)) return -1;
  return 0;
}

/* Create a printer, start running it, and return a WEAK reference.
 */
 
static struct synth_printer *synth_begin_print(struct synth *synth,const void *src,int srcc) {
  if (synth->printerc>=synth->printera) {
    int na=synth->printera+16;
    if (na>INT_MAX/sizeof(void*)) return 0;
    void *nv=realloc(synth->printerv,sizeof(void*)*na);
    if (!nv) return 0;
    synth->printerv=nv;
    synth->printera=na;
  }
  struct synth_printer *printer=synth_printer_new(src,srcc,synth->rate);
  if (!printer) return 0;
  if (synth->preprintc>0) synth_printer_update(printer,synth->preprintc);
  return printer;
}

/* Decode PCM synchronously, or install a printer to do it.
 */
 
struct synth_pcm *synth_init_pcm(struct synth *synth,const void *src,int srcc) {
  
  if (
    ((srcc>=4)&&!memcmp(src,"RIFF",4))||
  0) {
    return synth_pcm_decode(src,srcc,synth->rate);
  }
  
  if (
    ((srcc>=4)&&!memcmp(src,"\0EGS",4))||
  0) {
    struct synth_printer *printer=synth_begin_print(synth,src,srcc);
    if (!printer) return synth_pcm_new(1);
    if (synth_pcm_ref(printer->pcm)<0) return synth_pcm_new(1);
    return printer->pcm;
  }
  
  return synth_pcm_new(1);
}

/* Play sound, public.
 */
 
void synth_play_sound(struct synth *synth,int rid) {
  if (synth->voicec>=SYNTH_VOICE_LIMIT) return;
  int p=synth_resv_search(synth,EGG_TID_sound,rid);
  if (p<0) return;
  struct synth_res *res=synth->resv+p;
  if (!res->pcm) {
    if (!(res->pcm=synth_init_pcm(synth,res->src,res->srcc))) return;
  }
  struct synth_voice *voice=synth_voice_pcm_new(synth,res->pcm,1.0f);
  if (!voice) return;
  synth->voicev[synth->voicec++]=voice;
}

/* Play song, private.
 */
 
static void synth_play_song_internal(struct synth *synth,const void *src,int srcc,int rid,int force,int repeat) {
  if (!force) {
    if (rid==synth->songid) return;
  }
  synth_end_song(synth);
  synth->song=0;
  synth->songc=0;
  synth->songid=0; // Becomes (rid) only if decode succeeds.
  synth->songrepeat=repeat;
  synth->songp=0;
  synth->playhead=0;
  synth->songdelay=0;
  
  if ((srcc>=4)&&!memcmp(src,"\0EGS",4)) {
    const uint8_t *SRC=src;
    int srcp=4;
    
    while (srcp<srcc) {
      uint8_t chid=SRC[srcp];
      if (chid==0xff) break; // Events introducer.
      if (chid>=SYNTH_CHANNEL_COUNT) {
        fprintf(stderr,"song:%d: Invalid channel id %d\n",rid,chid);
        return;
      }
      struct synth_channel *channel=synth->channelv+chid;
      if (channel->mode) {
        fprintf(stderr,"song:%d: Illegal reinitialization of channel %d\n",rid,chid);
        return;
      }
      int err=synth_channel_decode(channel,synth,SRC+srcp,srcc-srcp);
      if (err<1) {
        fprintf(stderr,"song:%d: Error decoding channel %d\n",rid,chid);
        return;
      }
      srcp+=err;
    }
    
    if (srcp<srcc) {
      if (SRC[srcp++]!=0xff) return;
      synth->song=SRC+srcp;
      synth->songc=srcc-srcp;
    }
    if (!synth->songc) return;
    
    synth->songid=rid;
  }
}

/* Play song, three public options.
 */

void synth_play_song(struct synth *synth,int rid,int force,int repeat) {
  int p=synth_resv_search(synth,EGG_TID_song,rid);
  if (p<0) {
    // Missing resource plays silence as rid zero -- per spec, we must not report the faulty rid as playing.
    synth_play_song_internal(synth,0,0,0,0,0);
    return;
  }
  const struct synth_res *res=synth->resv+p;
  synth_play_song_internal(synth,res->src,res->srcc,rid,force,repeat);
}

void synth_play_song_handoff(struct synth *synth,void *src,int srcc,int repeat) {
  if ((srcc<0)||(srcc&&!src)) { src=0; srcc=0; }
  synth_play_song_internal(synth,src,srcc,0x10000,1,repeat);
  synth->songown=src;
}

void synth_play_song_borrow(struct synth *synth,const void *src,int srcc,int repeat) {
  synth_play_song_internal(synth,src,srcc,0x10000,1,repeat);
}

/* Event from client or our own song player.
 */
 
void synth_event(struct synth *synth,uint8_t chid,uint8_t opcode,uint8_t a,uint8_t b,int durms) {
  if (chid>=SYNTH_CHANNEL_COUNT) return;
  switch (opcode) {
    case 0x98: {
        if (synth->voicec>=SYNTH_VOICE_LIMIT) return;
        struct synth_channel *channel=synth->channelv+chid;
        struct synth_voice *voice=synth_channel_play_note(channel,synth,a,b,durms);
        if (!voice) return;
        if (synth->preprintc) voice->song=1; // A cheap hack: If we're inside an update, the voice belongs to song.
        synth->voicev[synth->voicec++]=voice;
      } break;
    //TODO We should be able to manage 0x90 and 0x80 too, if there's a need for live input.
  }
}

/* Playhead.
 */
 
int synth_get_song(const struct synth *synth) {
  return synth->songid;
}

double synth_get_playhead(struct synth *synth) {
  if (!synth->songc) return 0.0;
  double ph=(double)synth->playhead/(double)synth->rate;
  if (ph<=0.0) ph=0.001; // Never report exactly zero if a song is playing.
  return ph;
}

void synth_set_playhead(struct synth *synth,double s) {
  fprintf(stderr,"TODO: %s s=%f\n",__func__,s);//TODO
}
