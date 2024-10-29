#include "synth_internal.h"

/* Instance.
 */
 
struct synth_voice_fm {
  struct synth_voice hdr;
  float carrate;
  float carp;
  float modp;
  float modrate;
  struct synth_env_runner levelenv;
  struct synth_env_runner pitchenv;
  struct synth_env_runner rangeenv;
};

#define VOICE ((struct synth_voice_fm*)voice)

/* Delete.
 */
 
static void _fm_del(struct synth_voice *voice) {
}

/* Update.
 */
 
static void _fm_update_flat(float *v,int c,struct synth_voice *voice) {
  for (;c-->0;v++) {
    float level=synth_env_next(VOICE->levelenv);
    float sample=level*sinf(VOICE->carp);
    (*v)+=sample;
    
    float rate=VOICE->carrate;
    
    float mod=sinf(VOICE->modp);
    VOICE->modp+=rate*VOICE->modrate;
    while (VOICE->modp>=M_PI) VOICE->modp-=M_PI*2.0f;
    
    VOICE->carp+=rate+rate*mod*synth_env_next(VOICE->rangeenv);
    while (VOICE->carp>=M_PI) VOICE->carp-=M_PI*2.0f;
  }
  if (VOICE->levelenv.finished) voice->finished=1;
}
 
static void _fm_update_pitchenv(float *v,int c,struct synth_voice *voice) {
  for (;c-->0;v++) {
    float level=synth_env_next(VOICE->levelenv);
    float sample=level*sinf(VOICE->carp);
    (*v)+=sample;
    
    float rate=synth_env_next(VOICE->pitchenv);
    rate=VOICE->carrate*powf(2.0f,rate/1200.0f);
    
    float mod=sinf(VOICE->modp);
    VOICE->modp+=rate*VOICE->modrate;
    while (VOICE->modp>=M_PI) VOICE->modp-=M_PI*2.0f;
    
    VOICE->carp+=rate+rate*mod*synth_env_next(VOICE->rangeenv);
    while (VOICE->carp>=M_PI) VOICE->carp-=M_PI*2.0f;
  }
  if (VOICE->levelenv.finished) voice->finished=1;
}

/* Release.
 */
 
static void _fm_release(struct synth_voice *voice) {
  synth_env_release(&VOICE->levelenv);
  synth_env_release(&VOICE->pitchenv);
  synth_env_release(&VOICE->rangeenv);
}

/* New.
 */
 
struct synth_voice *synth_voice_fm_new(
  struct synth *synth,
  float modrate,
  float rate_norm,
  float velocity,
  int durframes,
  const struct synth_env_config *levelenv,
  const struct synth_env_config *pitchenv,
  const struct synth_env_config *rangeenv
) {
  struct synth_voice *voice=synth_voice_new(synth,sizeof(struct synth_voice_fm));
  if (!voice) return 0;
  voice->del=_fm_del;
  voice->release=_fm_release;
  synth_env_init(&VOICE->levelenv,levelenv,velocity,durframes);
  synth_env_init(&VOICE->rangeenv,rangeenv,velocity,durframes);
  if (pitchenv->pointc>0) {
    synth_env_init(&VOICE->pitchenv,pitchenv,velocity,durframes);
    voice->update=_fm_update_pitchenv;
    VOICE->carrate=rate_norm;
  } else {
    float cents;
    if (pitchenv->flags&SYNTH_ENV_FLAG_VELOCITY) {
      cents=pitchenv->inivlo*(1.0f-velocity)+pitchenv->inivhi*velocity;
    } else {
      cents=pitchenv->inivlo;
    }
    VOICE->carrate=rate_norm*powf(2.0f,cents/1200.0f);
    voice->update=_fm_update_flat;
  }
  VOICE->carrate*=M_PI*2.0f;
  return voice;
}
