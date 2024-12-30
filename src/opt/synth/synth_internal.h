#ifndef SYNTH_INTERNAL_H
#define SYNTH_INTERNAL_H

// Spelling Bee does exceed 32 voices at times.
#define SYNTH_VOICE_LIMIT 64
#define SYNTH_CHANNEL_COUNT 16 /* Not negotiable. */
#define SYNTH_RATE_MIN 200
#define SYNTH_RATE_MAX 200000
#define SYNTH_CHANC_MIN 1
#define SYNTH_CHANC_MAX 8

#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>
#include <math.h>
#include "egg/egg.h"
#include "synth.h"
#include "synth_env.h"
#include "synth_pcm.h"
#include "synth_voice.h"
#include "synth_channel.h"

/* Global context.
 ********************************************************/
 
struct synth {
  int rate,chanc;
  double framesperms;
  float global_trim;
  int neuter;
  
  float *qbuf;
  int qbufa;
  struct synth_wave *sine;
  float rate_by_noteid[128];
  
  struct synth_voice *voicev[SYNTH_VOICE_LIMIT];
  int voicec;
  
  struct synth_printer **printerv;
  int printerc,printera;
  int preprintc; // Nonzero if an update is in progress. New printers must run so many frames on construction.
  
  struct synth_res {
    int tid; // EGG_TID_sound or EGG_TID_song
    int rid;
    const void *src;
    int srcc;
    struct synth_pcm *pcm;
  } *resv;
  int resc,resa;
  
  struct synth_channel channelv[SYNTH_CHANNEL_COUNT];
  int channelc; // 0..16, but channelv may contain dummies
  
  int songid;
  int songrepeat;
  const uint8_t *song; // Events only.
  int songc;
  int songp;
  int songdelay; // frames
  int playhead; // frames
  void *songown; // free on song change
  struct synth_voice *songvoice; // WEAK; owned by (voicev). For PCM songs only.
};

/* Decode PCM synchronously, or install a printer to do it.
 * Returns STRONG.
 * We'll try to generate one sample of silence if decoding fails.
 */ 
struct synth_pcm *synth_init_pcm(struct synth *synth,const void *src,int srcc);

#endif
