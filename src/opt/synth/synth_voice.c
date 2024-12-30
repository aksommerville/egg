#include "synth_internal.h"

/* Delete.
 */

void synth_voice_del(struct synth_voice *voice) {
  if (!voice) return;
  voice->del(voice);
  free(voice);
}

/* Dummy hooks.
 */
 
static void _voice_del_dummy(struct synth_voice *voice) {
}

static void _voice_update_dummy(float *v,int c,struct synth_voice *voice) {
  voice->finished=1;
}

static void _voice_release_dummy(struct synth_voice *voice) {
  voice->finished=1;
}

/* New.
 */

struct synth_voice *synth_voice_new(struct synth *synth,int objlen) {
  if (!synth||(objlen<(int)sizeof(struct synth_voice))) return 0;
  struct synth_voice *voice=calloc(1,objlen);
  if (!voice) return 0;
  voice->synth=synth;
  voice->magic=' ';
  voice->del=_voice_del_dummy;
  voice->update=_voice_update_dummy;
  voice->release=_voice_release_dummy;
  return voice;
}
