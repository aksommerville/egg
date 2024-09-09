#ifndef SYNTH_INTERNAL_H
#define SYNTH_INTERNAL_H

#include "synth.h"
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <stdio.h>
#include <math.h>

#define SYNTH_RATE_MIN 200
#define SYNTH_RATE_MAX 200000
#define SYNTH_CHANC_MIN 1
#define SYNTH_CHANC_MAX 8 /* We'll only use the first two. */
#define SYNTH_QBUF_SIZE_FRAMES 1024
#define SYNTH_UPDATE_LIMIT_FRAMES 1024
#define SYNTH_PCM_SIZE_LIMIT (1<<20) /* Hard limit in samples for safety. Outside of this, we impose a 5-second limit. */

/* PCM object.
 *******************************************************/
 
struct synth_pcm {
  int refc;
  int c;
  float v[];
};

void synth_pcm_del(struct synth_pcm *pcm);
int synth_pcm_ref(struct synth_pcm *pcm);
struct synth_pcm *synth_pcm_new(int samplec);

/* Global context.
 ***************************************************************/

struct synth {
  int rate,chanc;
  float *qbuf; // Used only by synth_updatei().
  int qbufa;
  
  struct synth_sound {
    int rid,index;
    const void *v;
    int c;
    struct synth_pcm *pcm; // Null until the first play.
  } *soundv;
  int soundc,sounda;
  
  struct synth_song {
    int rid;
    const void *v;
    int c;
  } *songv;
  int songc,songa;
  
  //TODO Song player.
  //TODO PCM printers.
  //TODO MIDI bus.
  //TODO PCM players.
  //TODO Tuned voices.
};

int synth_soundv_search(const struct synth *synth,int rid,int index);
int synth_songv_search(const struct synth *synth,int rid);

#endif
