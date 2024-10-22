#include "synth_internal.h"

/* Delete.
 */
 
void synth_pcm_del(struct synth_pcm *pcm) {
  if (!pcm) return;
  if (pcm->refc-->1) return;
  free(pcm);
}

/* Retain.
 */
 
int synth_pcm_ref(struct synth_pcm *pcm) {
  if (!pcm) return -1;
  if (pcm->refc<1) return -1;
  if (pcm->refc==INT_MAX) return -1;
  pcm->refc++;
  return 0;
}

/* New.
 */
 
struct synth_pcm *synth_pcm_new(int c) {
  if ((c<0)||(c>SYNTH_PCM_LIMIT)) return 0;
  struct synth_pcm *pcm=calloc(1,sizeof(struct synth_pcm)+sizeof(float)*c);
  if (!pcm) return 0;
  pcm->refc=1;
  pcm->c=c;
  return pcm;
}
