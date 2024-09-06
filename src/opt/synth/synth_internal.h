#ifndef SYNTH_INTERNAL_H
#define SYNTH_INTERNAL_H

#include "synth.h"
#include "opt/sfg/sfg.h"
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

/* Global context.
 ***************************************************************/

struct synth {
  int rate,chanc;
  float *qbuf; // Used only by synth_updatei().
  int qbufa;
  
  struct synth_sounds {
    int rid;
    const void *v;
    int c;
    struct sfg_pcm **pcmv;
    int pcmc,pcma;
  } *soundsv;
  int soundsc,soundsa;
  
  struct synth_song {
    int rid;
    const void *v;
    int c;
  } *songv;
  int songc,songa;
  
  struct sfg_printer **printerv;
  int printerc,printera;
  int preprintc; // New printers, immediately run for so many samples.
  
  //TODO Song player.
  //TODO PCM printers.
  //TODO MIDI bus.
  //TODO PCM players.
  //TODO Tuned voices.
};

int synth_soundsv_search(const struct synth *synth,int rid);
int synth_songv_search(const struct synth *synth,int rid);

int synth_sounds_pcmv_search(const struct synth_sounds *sounds,int id);
int synth_sounds_pcmv_insert(struct synth_sounds *sounds,int p,struct sfg_pcm *pcm); // HANDOFF on success
int synth_sounds_get_sound(void *dstpp,const uint8_t *v,int c,int id);

int synth_printerv_add(struct synth *synth,struct sfg_printer *printer); // HANDOFF

#endif
