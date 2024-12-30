#include "synth_internal.h"

/* Instance.
 */
 
struct synth_voice_pcm {
  struct synth_voice hdr;
  struct synth_pcm *pcm;
  int p;
  int loopp; // <0 for no loop
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
  while (c>0) {
    if (VOICE->p>=VOICE->pcm->c) {
      if (VOICE->loopp<0) {
        voice->finished=1;
        return;
      }
      VOICE->p=VOICE->loopp;
    }
    int updc=VOICE->pcm->c-VOICE->p;
    if (updc>c) updc=c;
    int i=updc;
    const float *src=VOICE->pcm->v+VOICE->p;
    for (;i-->0;v++,src++) (*v)+=(*src);
    VOICE->p+=updc;
    v+=updc;
    c-=updc;
  }
}
 
static void _pcm_update_trim(float *v,int c,struct synth_voice *voice) {
  while (c>0) {
    if (VOICE->p>=VOICE->pcm->c) {
      if (VOICE->loopp<0) {
        voice->finished=1;
        return;
      }
      VOICE->p=VOICE->loopp;
    }
    int updc=VOICE->pcm->c-VOICE->p;
    if (updc>c) updc=c;
    int i=updc;
    const float *src=VOICE->pcm->v+VOICE->p;
    for (;i-->0;v++,src++) (*v)+=(*src)*VOICE->trim;
    VOICE->p+=updc;
    v+=updc;
    c-=updc;
  }
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
  voice->magic='p';
  voice->del=_pcm_del;
  VOICE->loopp=-1;
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

/* Repeat position.
 */

void synth_voice_pcm_set_repeat(struct synth_voice *voice,int frame) {
  if (!voice||(voice->magic!='p')) return;
  if (frame>=VOICE->pcm->c) return;
  VOICE->loopp=frame;
}

int synth_voice_pcm_get_position(const struct synth_voice *voice) {
  if (!voice||(voice->magic!='p')) return 0;
  return VOICE->p;
}

/* Set playhead.
 */
 
int synth_voice_pcm_set_position(struct synth_voice *voice,int p) {
  if (!voice||(voice->magic!='p')) return p;
  if (p<0) p=0; else if (p>VOICE->pcm->c) p=VOICE->pcm->c;
  VOICE->p=p;
  voice->finished=0;
  return p;
}

/* Stop playing, report completed.
 */
 
void synth_voice_pcm_abort(struct synth_voice *voice) {
  if (!voice||(voice->magic!='p')) return;
  VOICE->loopp=-1;
  VOICE->p=VOICE->pcm->c;
  voice->finished=1;
}
