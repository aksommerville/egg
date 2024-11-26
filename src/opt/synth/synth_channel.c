#include "synth_internal.h"

/* Cleanup.
 */
 
void synth_channel_cleanup(struct synth_channel *channel) {
  if (channel->drumv) {
    struct synth_drum *drum=channel->drumv;
    int i=128;
    for (;i-->0;drum++) {
      if (drum->pcm) synth_pcm_del(drum->pcm);
    }
    free(channel->drumv);
  }
  synth_wave_del(channel->wave);
  memset(channel,0,sizeof(struct synth_channel));
}

/* DRUM.
 */
 
static int synth_channel_decode_DRUM(struct synth_channel *channel,struct synth *synth,const uint8_t *src,int srcc) {
  if (!(channel->drumv=calloc(sizeof(struct synth_drum),128))) return -1;
  int srcp=0;
  while (srcp<srcc) {
    if (srcp>srcc-5) return -1;
    uint8_t noteid=src[srcp++];
    uint8_t trimlo=src[srcp++];
    uint8_t trimhi=src[srcp++];
    int len=(src[srcp]<<8)|src[srcp+1];
    srcp+=2;
    if (srcp>srcc-len) return -1;
    if (noteid<0x80) {
      struct synth_drum *drum=channel->drumv+noteid;
      if (drum->src) return -1;
      drum->trimlo=trimlo/255.0f;
      drum->trimhi=trimhi/255.0f;
      drum->src=src+srcp;
      drum->srcc=len;
    }
    srcp+=len;
  }
  return 0;
}

static struct synth_voice *synth_channel_play_note_DRUM(struct synth_channel *channel,struct synth *synth,uint8_t noteid,float velocity) {
  struct synth_drum *drum=channel->drumv+noteid;
  if (!drum->srcc) {
    if (!drum->warned) {
      drum->warned=1;
      fprintf(stderr,"WARNING: Channel %d tried to play undefined drum 0x%02x.\n",channel->chid,noteid);
    }
    return 0;
  }
  if (!drum->pcm) {
    if (!(drum->pcm=synth_init_pcm(synth,drum->src,drum->srcc))) {
      if (!drum->warned) {
        drum->warned=1;
        fprintf(stderr,"WARNING: Channel %d, drum 0x%02x, failed to decode %d bytes.\n",channel->chid,noteid,drum->srcc);
      }
      return 0;
    }
  }
  float trim=drum->trimlo*(1.0f-velocity)+drum->trimhi*velocity;
  trim*=channel->trim;
  return synth_voice_pcm_new(synth,drum->pcm,trim);
}

/* WAVE.
 */
 
static int synth_channel_decode_WAVE(struct synth_channel *channel,struct synth *synth,const uint8_t *src,int srcc) {
  int srcp=0,err;
  
  if ((err=synth_env_config_decode(&channel->levelenv,src+srcp,srcc-srcp,synth->rate))<0) return err;
  srcp+=err;
  synth_env_config_scale(&channel->levelenv,channel->trim);
  
  if (!(channel->wave=synth_wave_new())) return -1;
  if ((err=synth_wave_decode(channel->wave,synth,src+srcp,srcc-srcp))<0) return err;
  srcp+=err;
  
  if (srcp<srcc) {
    if ((err=synth_env_config_decode(&channel->pitchenv,src+srcp,srcc-srcp,synth->rate))<0) return err;
    srcp+=err;
    synth_env_config_bias(&channel->pitchenv,-0.5f);
    synth_env_config_scale(&channel->pitchenv,32768.0f);
  }
  
  return 0;
}

static struct synth_voice *synth_channel_play_note_WAVE(struct synth_channel *channel,struct synth *synth,float rate_norm,float velocity,int durframes) {
  return synth_voice_wave_new(synth,channel->wave,rate_norm,velocity,durframes,&channel->levelenv,&channel->pitchenv);
}

/* FM.
 */
 
static int synth_channel_decode_FM(struct synth_channel *channel,struct synth *synth,const uint8_t *src,int srcc) {
  int srcp=0,err;
  
  if ((err=synth_env_config_decode(&channel->levelenv,src+srcp,srcc-srcp,synth->rate))<0) return err;
  srcp+=err;
  synth_env_config_scale(&channel->levelenv,channel->trim);
  
  if (srcp>=srcc) {
    channel->modrate=1.0f;
  } else if (srcp>srcc-2) {
    return -1;
  } else {
    channel->modrate=src[srcp]+src[srcp+1]/256.0f;
    srcp+=2;
  }
  
  float rangepeak=1.0f;
  if (srcp>=srcc) {
  } else if (srcp>srcc-2) {
    return -1;
  } else {
    rangepeak=src[srcp]+src[srcp+1]/256.0f;
    srcp+=2;
  }
  
  if (srcp<srcc) {
    if ((err=synth_env_config_decode(&channel->pitchenv,src+srcp,srcc-srcp,synth->rate))<0) return err;
    srcp+=err;
    synth_env_config_bias(&channel->pitchenv,-0.5f);
    synth_env_config_scale(&channel->pitchenv,32768.0f);
  }
  
  if (srcp<srcc) {
    if ((err=synth_env_config_decode(&channel->rangeenv,src+srcp,srcc-srcp,synth->rate))<0) return err;
    srcp+=err;
    synth_env_config_scale(&channel->rangeenv,rangepeak);
  } else {
    channel->rangeenv.inivlo=rangepeak;
    channel->rangeenv.inivhi=rangepeak;
  }
  
  return 0;
}

static struct synth_voice *synth_channel_play_note_FM(struct synth_channel *channel,struct synth *synth,float rate_norm,float velocity,int durframes) {
  return synth_voice_fm_new(synth,channel->modrate,rate_norm,velocity,durframes,&channel->levelenv,&channel->pitchenv,&channel->rangeenv);
}

/* SUB.
 */
 
static int synth_channel_decode_SUB(struct synth_channel *channel,struct synth *synth,const uint8_t *src,int srcc) {
  int srcp=0,err;
  
  if ((err=synth_env_config_decode(&channel->levelenv,src+srcp,srcc-srcp,synth->rate))<0) return err;
  srcp+=err;
  synth_env_config_scale(&channel->levelenv,channel->trim);
  
  if (srcp>=srcc) {
    channel->subwidth=100.0f;
  } else if (srcp>srcc-2) {
    return -1;
  } else {
    channel->subwidth=(float)((src[srcp]<<8)|src[srcp+1]);
    srcp+=2;
  }
  channel->subwidth/=(float)synth->rate;

  return 0;
}

static struct synth_voice *synth_channel_play_note_SUB(struct synth_channel *channel,struct synth *synth,float rate_norm,float velocity,int durframes) {
  return synth_voice_sub_new(synth,rate_norm,channel->subwidth,velocity,durframes,&channel->levelenv);
}

/* Decode.
 */

int synth_channel_decode(struct synth_channel *channel,struct synth *synth,const void *src,int srcc,float global_trim) {
  if (!channel||channel->drumv||channel->wave) return -1;
  const uint8_t *SRC=src;
  if (srcc<6) return -1;
  channel->chid=SRC[0];
  channel->trim=SRC[1]/255.0f;
  channel->trim*=global_trim;
  channel->mode=SRC[2];
  int bodylen=(SRC[3]<<16)|(SRC[4]<<8)|SRC[5];
  if (6>srcc-bodylen) return -1;
  switch (channel->mode) {
    case SYNTH_CHANNEL_MODE_DRUM: if (synth_channel_decode_DRUM(channel,synth,SRC+6,bodylen)<0) return -1; break;
    case SYNTH_CHANNEL_MODE_WAVE: if (synth_channel_decode_WAVE(channel,synth,SRC+6,bodylen)<0) return -1; break;
    case SYNTH_CHANNEL_MODE_FM: if (synth_channel_decode_FM(channel,synth,SRC+6,bodylen)<0) return -1; break;
    case SYNTH_CHANNEL_MODE_SUB: if (synth_channel_decode_SUB(channel,synth,SRC+6,bodylen)<0) return -1; break;
    default: {
        channel->mode=SYNTH_CHANNEL_MODE_NOOP;
        channel->trim=0.0f;
      }
  }
  return 6+bodylen;
}

/* Play note.
 */

struct synth_voice *synth_channel_play_note(
  struct synth_channel *channel,
  struct synth *synth,
  uint8_t noteid, // 0..127
  uint8_t velocity, // 0..127
  int durms
) {

  // Get out quick for NOOP or invalid noteid.
  if (!channel||(channel->mode==SYNTH_CHANNEL_MODE_NOOP)) return 0;
  if (noteid>=0x80) return 0;
  
  // All real modes require floating-point velocity.
  float fvel=(velocity<0x80)?(velocity/127.0f):1.0f;
  
  // DRUM takes noteid au naturelle and doesn't need duration.
  if (channel->mode==SYNTH_CHANNEL_MODE_DRUM) {
    return synth_channel_play_note_DRUM(channel,synth,noteid,fvel);
  }
  
  // All others (WAVE,FM,SUB) take a normalized note frequency, and duration in frames.
  float rate=synth->rate_by_noteid[noteid];
  int durframes=(int)(synth->framesperms*durms);
  switch (channel->mode) {
    case SYNTH_CHANNEL_MODE_WAVE: return synth_channel_play_note_WAVE(channel,synth,rate,fvel,durframes);
    case SYNTH_CHANNEL_MODE_FM: return synth_channel_play_note_FM(channel,synth,rate,fvel,durframes);
    case SYNTH_CHANNEL_MODE_SUB: return synth_channel_play_note_SUB(channel,synth,rate,fvel,durframes);
  }
  return 0;
}
