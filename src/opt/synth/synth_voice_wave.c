#include "synth_internal.h"

/* Instance.
 */
 
struct synth_voice_wave {
  struct synth_voice hdr;
  struct synth_wave *wave;
  uint32_t p;
  uint32_t dp;
  float baserate;
  struct synth_env_runner levelenv;
  struct synth_env_runner pitchenv;
};

#define VOICE ((struct synth_voice_wave*)voice)

/* Delete.
 */
 
static void _wave_del(struct synth_voice *voice) {
  synth_wave_del(VOICE->wave);
}

/* Update.
 */
 
static void _wave_update_flat(float *v,int c,struct synth_voice *voice) {
  for (;c-->0;v++) {
    float level=synth_env_next(VOICE->levelenv);
    float sample=level*VOICE->wave->v[VOICE->p>>SYNTH_WAVE_SHIFT];
    (*v)+=sample;
    VOICE->p+=VOICE->dp;
  }
  if (VOICE->levelenv.finished) voice->finished=1;
}
 
static void _wave_update_pitchenv(float *v,int c,struct synth_voice *voice) {
  for (;c-->0;v++) {
    float level=synth_env_next(VOICE->levelenv);
    float sample=level*VOICE->wave->v[VOICE->p>>SYNTH_WAVE_SHIFT];
    (*v)+=sample;
    float rate=synth_env_next(VOICE->pitchenv);
    rate=VOICE->baserate*powf(2.0f,rate/1200.0f);
    uint32_t dp=(uint32_t)(rate*4294967296.0f);
    VOICE->p+=dp;
  }
  if (VOICE->levelenv.finished) voice->finished=1;
}

/* Release.
 */
 
static void _wave_release(struct synth_voice *voice) {
  synth_env_release(&VOICE->levelenv);
  synth_env_release(&VOICE->pitchenv);
}

/* New.
 */
 
struct synth_voice *synth_voice_wave_new(
  struct synth *synth,
  struct synth_wave *wave,
  float rate_norm,
  float velocity,
  int durframes,
  const struct synth_env_config *levelenv,
  const struct synth_env_config *pitchenv
) {
  struct synth_voice *voice=synth_voice_new(synth,sizeof(struct synth_voice_wave));
  if (!voice) return 0;
  voice->magic='w';
  voice->del=_wave_del;
  voice->release=_wave_release;
  if (synth_wave_ref(wave)<0) {
    synth_voice_del(voice);
    return 0;
  }
  VOICE->wave=wave;
  synth_env_init(&VOICE->levelenv,levelenv,velocity,durframes);
  if (pitchenv->pointc>0) {
    synth_env_init(&VOICE->pitchenv,pitchenv,velocity,durframes);
    voice->update=_wave_update_pitchenv;
    VOICE->baserate=rate_norm;
  } else {
    float cents;
    if (pitchenv->flags&SYNTH_ENV_FLAG_VELOCITY) {
      cents=pitchenv->inivlo*(1.0f-velocity)+pitchenv->inivhi*velocity;
    } else {
      cents=pitchenv->inivlo;
    }
    rate_norm*=powf(2.0f,cents/1200.0f);
    VOICE->dp=(uint32_t)(rate_norm*4295967296.0f);
    voice->update=_wave_update_flat;
  }
  return voice;
}
