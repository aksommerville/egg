#include "../synth_internal.h"

// Nominal master of 1.0 actually comes out as this.
#define SYNTH_GLOBAL_TRIM 0.25f

// Sanity limit, refuse to add more voices than this.
// Must be less than INT_MAX/sizeof(void*), but realistically should be *way* less than that :)
#define SYNTH_NODE_CHANNEL_VOICE_LIMIT 32

// Must be as long as the longest static command.
static const uint8_t synth_node_channel_spare_zeroes[32]={0};

// Encoded envelope, used if no 'level' field present in config.
static const uint8_t synth_node_channel_default_level[]={
  0x01, // sustain at second point
  0x00,0x00,0x00,0x00, // start at zero
  0x00,0x30, 0x00,0x20, 0x40,0x00, 0xff,0xff, // attack
  0x00,0x30, 0x00,0x30, 0x10,0x00, 0x30,0x00, // decay/sustain
  0x01,0x00, 0x02,0x00, 0x00,0x00, 0x00,0x00, // release
};

/* Instance definition.
 */
 
struct synth_node_channel {
  struct synth_node hdr;
  uint8_t chid;
  struct synth_node *bus; // WEAK, REQUIRED
  struct synth_node **voicev;
  int voicec,voicea;
  struct synth_node *post; // STRONG, OPTIONAL
  float *buffer;
  int bufferframec;
  float master,pan; // From Channel Header. They apply in mysterious ways.
  int wheelrange; // cents
  int wheelraw; // Most recent value as encoded: 0..16383.
  int wheelcents; // '' scaled by (wheelrange).
  float wheelrel; // '' as a pitch multiplier.
  float *pitchlfo_buffer; // SYNTH_UPDATE_LIMIT_FRAMES if not null
  float *fmlfo_buffer; // ''
  int sounds_constraint;
  
// Voice factory:
  const struct synth_node_type *voicetype; // pcm,sub,wave,fm
  int drumsrid;
  int drumsbias;
  float drumstrimlo;
  float drumstrimhi;
  float subwidth; // norm
  float fmrate;
  float fmrange;
  struct synth_env *level,*pitchenv,*fmenv;
  struct synth_wave wave;
  struct synth_osc pitchlfo,fmlfo;
};

#define NODE ((struct synth_node_channel*)node)

/* Delete.
 */
 
static void _channel_del(struct synth_node *node) {
  if (NODE->voicev) {
    while (NODE->voicec-->0) synth_node_del(NODE->voicev[NODE->voicec]);
    free(NODE->voicev);
  }
  synth_node_del(NODE->post);
  if (NODE->buffer) free(NODE->buffer);
  if (NODE->pitchlfo_buffer) free(NODE->pitchlfo_buffer);
  if (NODE->fmlfo_buffer) free(NODE->fmlfo_buffer);
  if (NODE->level) free(NODE->level);
  if (NODE->pitchenv) free(NODE->pitchenv);
  if (NODE->fmenv) free(NODE->fmenv);
}

/* Update into provided buffer, no private buffer.
 */
 
static void _channel_update_direct(float *v,int c,struct synth_node *node) {

  if (NODE->pitchlfo_buffer) {
    synth_osc_update(NODE->pitchlfo_buffer,c,&NODE->pitchlfo);
  }
  if (NODE->fmlfo_buffer) {
    synth_osc_update(NODE->fmlfo_buffer,c,&NODE->fmlfo);
  }

  int i=NODE->voicec;
  while (i-->0) {
    struct synth_node *voice=NODE->voicev[i];
    voice->update(v,c,voice);
    if (voice->finished) {
      NODE->voicec--;
      memmove(NODE->voicev+i,NODE->voicev+i+1,sizeof(void*)*(NODE->voicec-i));
    }
  }
  if (NODE->post) NODE->post->update(v,c,NODE->post);
}

/* Update into private buffer, then add to provided.
 */
 
static void _channel_update_buffer(float *v,int c,struct synth_node *node) {
  if (NODE->bufferframec<1) return;
  while (c>0) {
    int updc=NODE->bufferframec;
    if (updc>c) updc=c;
    int updsamplec=updc*node->chanc;
    memset(NODE->buffer,0,sizeof(float)*updsamplec);
    _channel_update_direct(NODE->buffer,updc,node);
    const float *src=NODE->buffer;
    int i=updsamplec;
    for (;i-->0;src++,v++) (*v)+=(*src);
    c-=updsamplec;
  }
}

/* Init.
 */
 
static int _channel_init(struct synth_node *node) {
  NODE->chid=0xff;
  NODE->wheelraw=0x2000;
  NODE->wheelrel=1.0f;
  return 0;
}

/* Ready.
 */
 
static int _channel_ready(struct synth_node *node) {
  if (!NODE->bus) return -1;
  if (!NODE->voicetype) return -1;
  
  if (NODE->sounds_constraint) {
    if (NODE->voicetype==&synth_node_type_pcm) {
      fprintf(stderr,"ERROR: Sound effect attempted to use a drums channel.\n");
      return -1;
    }
  }
  
  /* Decide whether to use a private buffer.
   * If a post is in play, definitely yes.
   * Otherwise... never?
   */
  if (NODE->post) {
    int bufa=1024;
    if (!(NODE->buffer=malloc(sizeof(float)*bufa*node->chanc))) return -1;
    NODE->bufferframec=bufa;
    node->update=_channel_update_buffer;

    // When there's a buffer, all voices use level 1. Add a gain node at the end of post to apply the master level.
    uint8_t gainserial[4]={0,0,0xff,0x00};
    int gaini=(int)(NODE->master*256.0f);
    if (gaini>=0xffff) gainserial[0]=gainserial[1]=0xff;
    else if (gaini<=0) gainserial[0]=gainserial[1]=0x00;
    else {
      gainserial[0]=gaini>>8;
      gainserial[1]=gaini;
    }
    if (synth_node_pipe_add_op(NODE->post,0x80,gainserial,sizeof(gainserial))<0) return -1;

  } else {
    node->update=_channel_update_direct;
  }
  
  if (NODE->post&&(synth_node_ready(NODE->post)<0)) return -1;
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

/* Configure bits.
 */

static int nonzero(const uint8_t *v,int c) {
  for (;c-->0;v++) if (*v) return 1;
  return 0;
}

#define ZEROPAD(reqc) \
  uint8_t scratch[reqc]; \
  if (c<sizeof(scratch)) { \
    memset(scratch,0,sizeof(scratch)); \
    memcpy(scratch,v,c); \
    v=scratch; \
    c=sizeof(scratch); \
  }

static void synth_node_channel_configure_drums(struct synth_node *node,const uint8_t *v,int c) {
  ZEROPAD(5)
  NODE->drumsrid=(v[0]<<8)|v[1];
  NODE->drumsbias=v[2];
  NODE->drumstrimlo=v[3]/255.0f;
  if (v[4]) NODE->drumstrimhi=v[4]/255.0f;
  else NODE->drumstrimhi=1.0f;
}

static void synth_node_channel_configure_sub(struct synth_node *node,const uint8_t *v,int c) {
  ZEROPAD(2)
  NODE->subwidth=(float)((v[0]<<8)|v[1])/(float)node->synth->rate;
  if (NODE->subwidth<0.000001) NODE->subwidth=0.000001;
  else if (NODE->subwidth>0.5) NODE->subwidth=0.5;
}

static void synth_node_channel_configure_fm(struct synth_node *node,const uint8_t *v,int c) {
  ZEROPAD(4)
  NODE->fmrate=((v[0]<<8)|v[1])/256.0f;
  NODE->fmrange=((v[2]<<8)|v[3])/256.0f;
}

#undef ZEROPAD

/* Configure.
 */
 
void synth_node_channel_setup(struct synth_node *node,uint8_t chid,struct synth_node *bus) {
  if (!node||(node->type!=&synth_node_type_channel)||node->ready) return;
  NODE->chid=chid;
  NODE->bus=bus;
}

int synth_node_channel_configure(struct synth_node *node,const void *src,int srcc) {
  if (!node||(node->type!=&synth_node_type_channel)||node->ready) return -1;
  #if 0 /*TODO*/
  // Capture the fields <128, which may only appear once, and apply globally.
  // Fields >=128 will be delivered directly to post, in the order they appear.
  struct fld {
    const uint8_t *v;
    int c;
  } fldv[128]={0};
  struct synth_song_channel_reader reader={.src=src,.srcc=srcc};
  int opcode,fldc;
  const uint8_t *fld;
  while ((fldc=synth_song_channel_reader_next(&opcode,&fld,&reader))>=0) {
    if (opcode<128) {
      if (fldv[opcode].v) return -1;
      if (opcode>=0x60) {
        // 0x60..0x7f are reserved for new critical fields -- not allowed to ignore. None currently defined.
        return -1;
      }
      fldv[opcode].v=fld;
      fldv[opcode].c=fldc;
    } else {
      if (!NODE->post) {
        if (!(NODE->post=synth_node_new(node->synth,&synth_node_type_pipe,node->chanc))) return -1;
      }
      if (synth_node_pipe_add_op(NODE->post,opcode,fld,fldc)<0) return -1;
    }
  }
  
  /* Establish voice type.
   */
  if (nonzero(fldv[0x03].v,fldv[0x03].c)) {
    NODE->voicetype=&synth_node_type_pcm;
    synth_node_channel_configure_drums(node,fldv[0x03].v,fldv[0x03].c);
  } else if (nonzero(fldv[0x05].v,fldv[0x05].c)) {
    NODE->voicetype=&synth_node_type_sub;
    synth_node_channel_configure_sub(node,fldv[0x05].v,fldv[0x05].c);
  } else if (nonzero(fldv[0x08].v,fldv[0x08].c)) {
    NODE->voicetype=&synth_node_type_fm;
    synth_node_channel_configure_fm(node,fldv[0x08].v,fldv[0x08].c);
  } else {
    NODE->voicetype=&synth_node_type_wave;
  }
  
  /* Read or default master and pan.
   */
  NODE->master=1.0f;
  NODE->pan=0.0f;
  if (fldv[0x01].c>=1) {
    NODE->master=fldv[0x01].v[0]/255.0f;
  }
  if (fldv[0x02].c>=1) {
    if (fldv[0x02].v[0]==0x00) {
      // Natural zero means force mono. Not sure if that's a thing we can do.
      NODE->pan=0.0f;
    } else {
      NODE->pan=(fldv[0x02].v[0]-0x80)/127.0f;
    }
  }
  //NODE->master*=SYNTH_GLOBAL_TRIM;
  
  // Wheel range is zero for drums, or provided in cents, or defaults to 200.
  if (NODE->voicetype==&synth_node_type_pcm) {
    NODE->wheelrange=0;
  } else if (fldv[0x04].c) {
    NODE->wheelrange=fldv[0x04].v[0]<<8;
    if (fldv[0x04].c>=2) NODE->wheelrange|=fldv[0x04].v[1];
  } else {
    NODE->wheelrange=200;
  }
  
  /* All voice types except pcm require a level envelope.
   */
  if (NODE->voicetype!=&synth_node_type_pcm) {
    if (fldv[0x0d].c) {
      if (!(NODE->level=synth_env_decode(fldv[0x0d].v,fldv[0x0d].c,node->synth->rate))) return -1;
    } else {
      if (!(NODE->level=synth_env_decode(synth_node_channel_default_level,sizeof(synth_node_channel_default_level),node->synth->rate))) return -1;
    }
  }
  
  /* Wave and FM take additional configuration.
   */
  if (
    (NODE->voicetype==&synth_node_type_fm)||
    (NODE->voicetype==&synth_node_type_wave)
  ) {
    if (fldv[0x0a].c) {
      if (synth_osc_decode(&NODE->fmlfo,node->synth,fldv[0x0a].v,fldv[0x0a].c)<0) return -1;
      NODE->fmlfo.scale*=0.5f;
      NODE->fmlfo.bias=0.5f;
      if (!(NODE->fmlfo_buffer=malloc(sizeof(float)*SYNTH_UPDATE_LIMIT_FRAMES))) return -1;
    }
    if (fldv[0x0c].c) {
      if (synth_osc_decode(&NODE->pitchlfo,node->synth,fldv[0x0c].v,fldv[0x0c].c)<0) return -1;
      NODE->pitchlfo.scale*=65536.0f;
      if (!(NODE->pitchlfo_buffer=malloc(sizeof(float)*SYNTH_UPDATE_LIMIT_FRAMES))) return -1;
    }
    if (fldv[0x09].c&&!(NODE->fmenv=synth_env_decode(fldv[0x09].v,fldv[0x09].c,node->synth->rate))) return -1;
    if (fldv[0x0b].c) {
      if (!(NODE->pitchenv=synth_env_decode(fldv[0x0b].v,fldv[0x0b].c,node->synth->rate))) return -1;
      synth_env_adjust_values(NODE->pitchenv,-0.5f,32768.0f);
    }
    int shape=0;
    if (fldv[0x06].c>=1) shape=fldv[0x06].v[0];
    synth_wave_synthesize(&NODE->wave,node->synth,shape,fldv[0x07].v,fldv[0x07].c);
  }
  #endif
  return 0;
}

/* Initialize new voice.
 * It's OK to fail, rather than initializing a dummy.
 */
 
static int synth_node_channel_init_voice(
  struct synth_node *node,
  struct synth_node *voice,
  uint8_t noteid,float velocity,int durms
) {
  int durframes=0;
  if (durms) {
    durframes=(durms*node->synth->rate)/1000;
    if (durframes<0) durframes=0;
  }
  if (voice->type==&synth_node_type_pcm) {
    int sndid=noteid+NODE->drumsbias;
    int p=synth_soundv_search(node->synth,NODE->drumsrid);
    if (p<0) {
      // This is definitely not an error and not worth logging. But keeping it loggable for troubleshooting.
      fprintf(stderr,"%s: Drum %d:%d (note 0x%02x) not found.\n",__func__,NODE->drumsrid,sndid,noteid);
      return -1;
    }
    if (synth_sound_require(node->synth,node->synth->soundv+p)<0) return -1;
    struct synth_pcm *pcm=node->synth->soundv[p].pcm;
    float level;
    if (velocity<=0.0f) level=NODE->drumstrimlo;
    else if (velocity>=1.0f) level=NODE->drumstrimhi;
    else level=(1.0f-velocity)*NODE->drumstrimlo+velocity*NODE->drumstrimhi;
    if (!NODE->bufferframec) level*=NODE->master;
    float pan=node->synth->soundv[p].pan+NODE->pan;
    if (pan<-1.0f) pan=-1.0f; else if (pan>1.0f) pan=1.0f;
    if (synth_node_pcm_setup(voice,pcm,level,pan)<0) return -1;
    
  } else if (voice->type==&synth_node_type_wave) {
    float rate=node->synth->ratefv[noteid];
    float level=NODE->bufferframec?1.0f:NODE->master;
    if (synth_node_wave_setup(voice,&NODE->wave,rate,velocity,durframes,NODE->level,level,NODE->pan)<0) return -1;
    if (NODE->pitchenv||NODE->pitchlfo_buffer) {
      synth_node_wave_set_pitch_adjustment(voice,NODE->pitchenv,NODE->pitchlfo_buffer);
    }
    synth_node_wave_adjust_rate(voice,NODE->wheelrel);
    
  } else if (voice->type==&synth_node_type_fm) {
    float rate=node->synth->ratefv[noteid];
    float level=NODE->bufferframec?1.0f:NODE->master;
    if (synth_node_fm_setup(voice,&NODE->wave,NODE->fmrate,NODE->fmrange,rate,velocity,durframes,NODE->level,level,NODE->pan)<0) return -1;
    if (NODE->pitchenv||NODE->pitchlfo_buffer) {
      synth_node_fm_set_pitch_adjustment(voice,NODE->pitchenv,NODE->pitchlfo_buffer);
    }
    if (NODE->fmenv||NODE->fmlfo_buffer) {
      synth_node_fm_set_modulation_adjustment(voice,NODE->fmenv,NODE->fmlfo_buffer);
    }
    synth_node_fm_adjust_rate(voice,NODE->wheelrel);
    
  } else if (voice->type==&synth_node_type_sub) {
    float rate=node->synth->ratefv[noteid];
    float level=NODE->bufferframec?1.0f:NODE->master;
    if (synth_node_sub_setup(voice,NODE->subwidth,rate,velocity,durframes,NODE->level,level,NODE->pan)<0) return -1;
    synth_node_sub_adjust_rate(voice,NODE->wheelrel);
    
  } else {
    return -1;
  }
  return synth_node_ready(voice);
}

/* Update voice rates, after wheel change.
 */
 
static void synth_node_channel_update_voice_rates(struct synth_node *node) {
  struct synth_node **p=NODE->voicev;
  int i=NODE->voicec;
  for (;i-->0;p++) {
    struct synth_node *voice=*p;
    if (voice->type==&synth_node_type_wave) synth_node_wave_adjust_rate(voice,NODE->wheelrel);
    else if (voice->type==&synth_node_type_fm) synth_node_fm_adjust_rate(voice,NODE->wheelrel);
    else if (voice->type==&synth_node_type_sub) synth_node_sub_adjust_rate(voice,NODE->wheelrel);
  }
}

/* Receive event.
 */
 
void synth_node_channel_event(struct synth_node *node,uint8_t chid,uint8_t opcode,uint8_t a,uint8_t b,int durms) {
  if (!node||(node->type!=&synth_node_type_channel)||!node->ready) return;
  //fprintf(stderr,"%s node=%p chid=%d opcode=0x%02x a=0x%02x b=0x%02x durms=%d\n",__func__,node,chid,opcode,a,b,durms);
  switch (opcode) {
    /* TODO We probably ought to support regular Note On and Note Off, but I'm not sure how that will work.
    case 0x80: ...
    case 0x90: ...
    */
    
    case 0x98: { // Note On with predetermined Off time.
        if (a&0x80) return;
        if (NODE->voicec>=SYNTH_NODE_CHANNEL_VOICE_LIMIT) {
          fprintf(stderr,"%s:WARNING: Too many voices, discarding note 0x%02x.\n",__func__,a);
          return;
        }
        if (NODE->voicec>=NODE->voicea) {
          int na=NODE->voicea+8;
          void *nv=realloc(NODE->voicev,sizeof(void*)*na);
          if (!nv) return;
          NODE->voicev=nv;
          NODE->voicea=na;
        }
        struct synth_node *voice=synth_node_new(node->synth,NODE->voicetype,node->chanc);
        if (!voice) return;
        if (b>0x7f) b=0x7f;
        if (synth_node_channel_init_voice(node,voice,a,(float)b/127.0f,durms)<0) {
          synth_node_del(voice);
          return;
        }
        NODE->voicev[NODE->voicec++]=voice;
      } break;
      
    case 0xe0: { // Pitch Wheel.
        if (NODE->wheelrange<1) return;
        int raw=a|(b<<7);
        if (raw==NODE->wheelraw) return;
        NODE->wheelraw=raw;
        int cents=((raw-0x2000)*NODE->wheelrange)/0x2000;
        if (cents==NODE->wheelcents) return;
        NODE->wheelcents=cents;
        NODE->wheelrel=synth_multiplier_from_cents(cents);
        synth_node_channel_update_voice_rates(node);
      } break;
  }
}

/* Add "sounds" constraint.
 */
 
int synth_node_channel_constrain_for_sounds(struct synth_node *node) {
  if (!node||(node->type!=&synth_node_type_channel)||node->ready) return -1;
  NODE->sounds_constraint=1;
  return 0;
}

/* Default pan.
 */

float synth_node_channel_get_default_pan(const struct synth_node *node) {
  if (!node||(node->type!=&synth_node_type_channel)) return 0.0f;
  return NODE->pan;
}
