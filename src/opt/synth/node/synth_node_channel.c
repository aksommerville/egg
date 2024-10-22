#include "../synth_internal.h"

/* Instance definition.
 */
 
struct synth_node_channel {
  struct synth_node hdr;
  uint8_t chid;
  uint8_t mode; // EGS_MODE_*
  float triml,trimr; // master,pan. Equal if chanc==1.
  struct synth_node *post;
  float *buffer;
  
  int wheelrange; // Cents, in mode WAVE or FM.
  struct synth_env_config level; // WAVE,FM,SUB
  struct synth_env_config pitchenv; // WAVE,FM
  struct synth_env_config rangeenv; // FM
  
// mode==EGS_MODE_DRUM:
  struct synth_drum {
    int rid; // absolute
    float pan; // -1..1
    float trimlo;
    float trimhi;
  } *drumv; // null or 128 entries. Unused entries are straight zeroes.
  
// mode==EGS_MODE_WAVE:
  float *wave; // SYNTH_WAVE_SIZE_SAMPLES
  
// mode==EGS_MODE_FM:
  float modrate; // relative to carrier
  float modrange;
  
// mode==EGS_MODE_SUB:
  int subwidth; // hz
};

#define NODE ((struct synth_node_channel*)node)

/* Cleanup.
 */
 
static void _channel_del(struct synth_node *node) {
  if (NODE->buffer) free(NODE->buffer);
  if (NODE->drumv) free(NODE->drumv);
  if (NODE->wave) free(NODE->wave);
  synth_node_del(NODE->post);
}

/* Init.
 */
 
static int _channel_init(struct synth_node *node) {
  return 0;
}

/* Update.
 */
 
static void _channel_update_noop(float *v,int framec,struct synth_node *node) {
}
 
static void _channel_update_straight(float *v,int framec,struct synth_node *node) {
  int i=node->childc;
  while (i-->0) {
    struct synth_node *voice=node->childv[i];
    voice->update(v,framec,voice);
    if (voice->finished) {
      voice->parent=0;
      node->childc--;
      memmove(node->childv+i,node->childv+i+1,sizeof(void*)*(node->childc-i));
      synth_node_del(voice);
    }
  }
}
 
static void _channel_update_post(float *v,int framec,struct synth_node *node) {
  int samplec=framec*node->chanc;
  int bufsize=sizeof(float)*samplec;
  memset(NODE->buffer,0,bufsize);
  _channel_update_straight(NODE->buffer,framec,node);
  if (NODE->post) NODE->post->update(NODE->buffer,framec,NODE->post);
  synth_signal_mlts(v,samplec,SYNTH_GLOBAL_TRIM);
  synth_signal_add(v,NODE->buffer,samplec);
}

/* Ready.
 */
 
static int _channel_ready(struct synth_node *node) {
  node->update=_channel_update_noop;
  switch (NODE->mode) {
    case EGS_MODE_NOOP: return 0;
    
    case EGS_MODE_DRUM: {
        if (!NODE->drumv) return -1;
      } break;
      
    case EGS_MODE_WAVE: {
        if (!NODE->wave) return -1;
      } break;
      
    case EGS_MODE_FM: {
      } break;
      
    case EGS_MODE_SUB: {
      } break;
      
    default: return -1;
  }
  // Creating the buffer was going to be optional (and use _channel_update_straight directly when not).
  // But after some consideration, I want the global trim after everything (eg waveshaper clipping).
  // Since we're buffering anyway, why not run the voices mono and apply the channel's master and pan generically?
  // ...because then we'd also have to buffer per-voice. Forget that.
  if (NODE->post) if (synth_node_ready(NODE->post)<0) return -1;
  if (!NODE->buffer&&!(NODE->buffer=malloc(sizeof(float)*SYNTH_UPDATE_LIMIT_SAMPLES))) return -1;
  node->update=_channel_update_post;
  return 0;
}

/* Type definition.
 */
 
const struct synth_node_type synth_node_type_channel={
  .name="channel",
  .objlen=sizeof(struct synth_node_channel),
  .del=_channel_del,
  .init=_channel_init,
  .ready=_channel_ready,
};

/* NOOP mode.
 */
 
static void channel_configure_NOOP(struct synth_node *node) {
  node->update=_channel_update_noop;
  NODE->triml=NODE->trimr=0.0f;
}

/* DRUM mode.
 */
 
static int channel_configure_DRUM(struct synth_node *node,const uint8_t *src,int srcc) {
  if (srcc<4) return -1;
  if (NODE->drumv) return -1;
  if (!(NODE->drumv=calloc(sizeof(struct synth_drum),128))) return -1;
  int ridbase=(src[0]<<8)|src[1];
  int noteid0=src[2];
  int notec=src[3];
  if (noteid0+notec>0x80) return -1;
  if (4+notec*4>srcc) return -1;
  int srcp=4;
  struct synth_drum *dst=NODE->drumv+noteid0;
  for (;notec-->0;dst++) {
    if (!src[srcp+2]&&!src[srcp+3]) { // Both trims zero, the note is unused. Ensure it stays fully zero.
      srcp+=4;
    } else {
      dst->rid=ridbase+src[srcp++];
      dst->pan=((src[srcp++]-0x80)/128.0f);
      dst->trimlo=src[srcp++]/255.0f;
      dst->trimhi=src[srcp++]/255.0f;
    }
  }
  return 0;
}

/* WAVE mode.
 */
 
static int channel_configure_WAVE(struct synth_node *node,const uint8_t *src,int srcc) {
  int srcp=0,err;
  
  if (srcp>srcc-2) return -1;
  NODE->wheelrange=(src[srcp]<<8)|src[srcp+1];
  srcp+=2;
  
  if ((err=synth_env_decode_level(&NODE->level,src+srcp,srcc-srcp))<0) return err;
  srcp+=err;
  
  if ((err=synth_env_decode_aux_u16(&NODE->pitchenv,&NODE->level,src+srcp,srcc-srcp,0x8000))<0) return err;
  srcp+=err;
  synth_env_config_bias(&NODE->pitchenv,-32768.0f);
  synth_env_config_scale(&NODE->pitchenv,1.0f/1200.0f);
  
  if (srcp>=srcc) return -1;
  int shape=src[srcp++];
  
  if (srcp>=srcc) return -1;
  int coefc=src[srcp++];
  if (shape&&coefc) {
    fprintf(stderr,"WAVE channel shape and coefc can't both be set. Have shape=%d and coefc=%d for channel %d.\n",shape,coefc,NODE->chid);
    return -1;
  }
  
  if (shape) {
    if (!(NODE->wave=synth_wave_generate_primitive(node->synth,shape))) return -1;
  } else {
    if (srcp>srcc-2*coefc) return -1;
    if (!(NODE->wave=synth_wave_generate_harmonics(node->synth,src+srcp,coefc))) return -1;
  }
  
  return 0;
}

/* FM mode.
 */
 
static int channel_configure_FM(struct synth_node *node,const uint8_t *src,int srcc) {
  int srcp=0,err;
  
  if (srcp>srcc-2) return -1;
  NODE->wheelrange=(src[srcp]<<8)|src[srcp+1];
  srcp+=2;
  
  if ((err=synth_env_decode_level(&NODE->level,src+srcp,srcc-srcp))<0) return err;
  srcp+=err;
  
  if ((err=synth_env_decode_aux_u16(&NODE->pitchenv,&NODE->level,src+srcp,srcc-srcp,0x8000))<0) return err;
  srcp+=err;
  synth_env_config_bias(&NODE->pitchenv,-32768.0f);
  synth_env_config_scale(&NODE->pitchenv,1.0f/1200.0f);
  
  if (srcp>srcc-4) return -1;
  NODE->modrate=src[srcp]+(src[srcp+1]/256.0f);
  NODE->modrange=src[srcp+2]+(src[srcp+3]/256.0f);
  srcp+=4;
  
  if ((err=synth_env_decode_aux(&NODE->rangeenv,&NODE->level,src+srcp,srcc-srcp,0xffff))<0) return err;
  srcp+=err;
  synth_env_config_scale(&NODE->rangeenv,NODE->modrange);
  
  return 0;
}

/* SUB mode.
 */
 
static int channel_configure_SUB(struct synth_node *node,const uint8_t *src,int srcc) {
  int srcp;
  
  if ((srcp=synth_env_decode_level(&NODE->level,src,srcc))<0) return srcp;
  
  if (srcp>srcc-2) return -1;
  NODE->subwidth=(src[srcp]<<8)|src[srcp+1];
  srcp+=2;
  
  return 0;
}

/* Build the post pipe.
 * This is not called in NOOP mode, or when the input is empty.
 */
 
static int channel_configure_post(struct synth_node *node,const uint8_t *src,int srcc) {
  if (NODE->post) return -1;
  if (!(NODE->post=synth_node_new(&synth_node_type_pipe,node->chanc,node->synth))) return -1;
  if (synth_node_pipe_configure(NODE->post,src,srcc)<0) return -1;
  return 0;
}

/* Configure.
 */
 
int synth_node_channel_configure(struct synth_node *node,const void *src,int srcc) {
  if (!node||(node->type!=&synth_node_type_channel)||node->ready) return -1;
  if (srcc<6) return -1;
  const uint8_t *SRC=src;
  int srcp=0;
  
  /* Starts with a 4-byte fixed header establishing chid, mode, and master trims.
   */
  NODE->chid=SRC[srcp++];
  uint8_t master=SRC[srcp++];
  uint8_t pan=SRC[srcp++];
  NODE->mode=SRC[srcp++];
  if ((node->chanc==1)||(NODE->mode==EGS_MODE_DRUM)) {
    // Mono and DRUM take a flat master, no pan. In DRUM mode, each noteid has its own pan.
    NODE->triml=NODE->trimr=master/255.0f;
  } else {
    if (pan<0x80) { NODE->triml=1.0f; NODE->trimr=pan/127.0f; }
    else if (pan>0x80) { NODE->triml=(0xff-pan)/127.0f; NODE->trimr=1.0f; }
    else NODE->triml=NODE->trimr=1.0f;
    float mtrim=master/255.0f;
    NODE->triml*=mtrim;
    NODE->trimr*=mtrim;
  }
  
  /* Mode config.
   */
  if (srcp>srcc-2) return -1;
  int len=(SRC[srcp]<<8)|SRC[srcp+1];
  srcp+=2;
  if (srcp>srcc-len) return -1;
  switch (NODE->mode) {
    case EGS_MODE_NOOP: channel_configure_NOOP(node); break;
    case EGS_MODE_DRUM: if (channel_configure_DRUM(node,SRC+srcp,len)<0) return -1; break;
    case EGS_MODE_WAVE: if (channel_configure_WAVE(node,SRC+srcp,len)<0) return -1; break;
    case EGS_MODE_FM: if (channel_configure_FM(node,SRC+srcp,len)<0) return -1; break;
    case EGS_MODE_SUB: if (channel_configure_SUB(node,SRC+srcp,len)<0) return -1; break;
    default: {
        fprintf(stderr,"WARNING: Unknown channel mode 0x%02x for synth channel %d. Disabling channel.\n",NODE->mode,NODE->chid);
        NODE->mode=EGS_MODE_NOOP;
        channel_configure_NOOP(node);
      }
  }
  srcp+=len;
  
  /* Post.
   */
  if (srcp>srcc-2) return -1;
  len=(SRC[srcp]<<8)|SRC[srcp+1];
  srcp+=2;
  if (srcp>srcc-len) return -1;
  if (len&&(NODE->mode!=EGS_MODE_NOOP)) {
    if (channel_configure_post(node,SRC+srcp,len)<0) return -1;
  }
  srcp+=len;
  
  return 0;
}

/* Trivial accessors.
 */
 
int synth_node_channel_get_chid(const struct synth_node *node) {
  if (!node||(node->type!=&synth_node_type_channel)) return -1;
  return NODE->chid;
}

/* Begin drum note.
 */
 
static void channel_begin_DRUM(struct synth_node *node,uint8_t noteid,float velocity) {
  fprintf(stderr,"%s TODO 0x%02x %f\n",__func__,noteid,velocity);
}

/* Begin wave note.
 */
 
static void channel_begin_WAVE(struct synth_node *node,float freq,float velocity,int durms) {
  struct synth_node *voice=synth_node_spawn(node,&synth_node_type_wave,0);
  if (!voice) return;
  synth_env_run(synth_node_wave_get_levelenv(voice),&NODE->level,velocity,durms,node->synth->rate);
  if (!NODE->pitchenv.noop) {
    synth_env_run(synth_node_wave_get_pitchenv(voice),&NODE->pitchenv,velocity,durms,node->synth->rate);
  }
  if (
    (synth_node_wave_configure(voice,NODE->wave,freq,NODE->triml,NODE->trimr)<0)||
    (synth_node_ready(voice)<0)
  ) {
    synth_node_remove_child(node,voice);
    return;
  }
}

/* Begin FM note.
 */
 
static void channel_begin_FM(struct synth_node *node,float freq,float velocity,int durms) {
  struct synth_node *voice=synth_node_spawn(node,&synth_node_type_fm,0);
  if (!voice) return;
  synth_env_run(synth_node_fm_get_levelenv(voice),&NODE->level,velocity,durms,node->synth->rate);
  if (!NODE->pitchenv.noop) {
    synth_env_run(synth_node_fm_get_pitchenv(voice),&NODE->pitchenv,velocity,durms,node->synth->rate);
  }
  if (!NODE->rangeenv.noop) {
    synth_env_run(synth_node_fm_get_rangeenv(voice),&NODE->rangeenv,velocity,durms,node->synth->rate);
  }
  if (
    (synth_node_fm_configure(voice,NODE->modrate,NODE->modrange,freq,NODE->triml,NODE->trimr)<0)||
    (synth_node_ready(voice)<0)
  ) {
    synth_node_remove_child(node,voice);
    return;
  }
}

/* Begin subtractive note.
 */
 
static void channel_begin_SUB(struct synth_node *node,float freq,float velocity,int durms) {
  fprintf(stderr,"%s TODO freq=%f vel=%f durms=%d\n",__func__,freq,velocity,durms);
}

/* Begin note, dispatch on mode.
 */
 
void synth_node_channel_begin_note(struct synth_node *node,uint8_t noteid,uint8_t velocity,int durms) {
  if (!node||(node->type!=&synth_node_type_channel)||!node->ready) return;
  if (noteid>=0x80) return;
  float fvel=velocity/127.0f;
  if (fvel>1.0f) fvel=1.0f;
  float freq=node->synth->freq_by_noteid[noteid];
  //TODO Apply wheel.
  switch (NODE->mode) {
    case EGS_MODE_DRUM: channel_begin_DRUM(node,freq,fvel); break;
    case EGS_MODE_WAVE: channel_begin_WAVE(node,freq,fvel,durms); break;
    case EGS_MODE_FM: channel_begin_FM(node,freq,fvel,durms); break;
    case EGS_MODE_SUB: channel_begin_SUB(node,freq,fvel,durms); break;
  }
}

/* Move pitch wheel.
 */
 
void synth_node_channel_set_wheel(struct synth_node *node,uint8_t v) {
  if (!node||(node->type!=&synth_node_type_channel)||!node->ready) return;
  fprintf(stderr,"%s 0x%02x\n",__func__,v);//TODO
}
