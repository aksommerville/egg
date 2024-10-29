#include "synth_internal.h"

/* Instance.
 */
 
struct synth_voice_sub {
  struct synth_voice hdr;
  struct synth_env_runner levelenv;
};

#define VOICE ((struct synth_voice_sub*)voice)

/* Delete.
 */
 
static void _sub_del(struct synth_voice *voice) {
}

/* Update.
 */
 
static void _sub_update(float *v,int c,struct synth_voice *voice) {
  for (;c-->0;v++) {
    float sample=((rand()&0xffff)-0x8000)/32768.0f;
    sample*=synth_env_next(VOICE->levelenv);
    //TODO IIR
    (*v)+=sample;
  }
  if (VOICE->levelenv.finished) voice->finished=1;
}

/* Release.
 */
 
static void _sub_release(struct synth_voice *voice) {
  synth_env_release(&VOICE->levelenv);
}

/* New.
 */
 
struct synth_voice *synth_voice_sub_new(
  struct synth *synth,
  float mid_norm,
  float width_norm,//XXX have caller provide coefficients; they can be precalculated
  float velocity,
  int durframes,
  const struct synth_env_config *levelenv
) {
  struct synth_voice *voice=synth_voice_new(synth,sizeof(struct synth_voice_sub));
  if (!voice) return 0;
  voice->del=_sub_del;
  voice->update=_sub_update;
  voice->release=_sub_release;
  synth_env_init(&VOICE->levelenv,levelenv,velocity,durframes);
  //TODO Prep IIR runner.
  return voice;
}
