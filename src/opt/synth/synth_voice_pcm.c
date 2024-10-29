#include "synth_internal.h"

/* Instance.
 */
 
struct synth_voice_pcm {
  struct synth_voice hdr;
  struct synth_pcm *pcm;
  int p;
  float trim;
};

#define VOICE ((struct synth_voice_pcm*)voice)

/* Delete.
 */
 
static void _pcm_del(struct synth_voice *voice) {
  synth_pcm_del(VOICE->pcm);
}

/* Update.
 */
 
static void _pcm_update_1(float *v,int c,struct synth_voice *voice) {
  int updc=VOICE->pcm->c-VOICE->p;
  if (updc>c) updc=c;
  if (updc>0) {
    int i=updc;
    const float *src=VOICE->pcm->v+VOICE->p;
    for (;i-->0;v++,src++) (*v)+=(*src);
    VOICE->p+=updc;
  }
  if (VOICE->p>=VOICE->pcm->c) voice->finished=1;
}
 
static void _pcm_update_trim(float *v,int c,struct synth_voice *voice) {
  int updc=VOICE->pcm->c-VOICE->p;
  if (updc>c) updc=c;
  if (updc>0) {
    int i=updc;
    const float *src=VOICE->pcm->v+VOICE->p;
    for (;i-->0;v++,src++) (*v)+=(*src)*VOICE->trim;
    VOICE->p+=updc;
  }
  if (VOICE->p>=VOICE->pcm->c) voice->finished=1;
}

/* Release.
 */
 
static void _pcm_release(struct synth_voice *voice) {
  // Do nothing. We'll finish naturally either way.
}

/* New.
 */
 
struct synth_voice *synth_voice_pcm_new(
  struct synth *synth,
  struct synth_pcm *pcm,
  float trim
) {
  struct synth_voice *voice=synth_voice_new(synth,sizeof(struct synth_voice_pcm));
  if (!voice) return 0;
  voice->del=_pcm_del;
  if (trim>=1.0f) {
    voice->update=_pcm_update_1;
  } else {
    voice->update=_pcm_update_trim;
    VOICE->trim=trim;
  }
  voice->release=_pcm_release;
  if (synth_pcm_ref(pcm)<0) {
    synth_voice_del(voice);
    return 0;
  }
  VOICE->pcm=pcm;
  return voice;
}
