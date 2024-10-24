#ifndef SYNTH_INTERNAL_H
#define SYNTH_INTERNAL_H

#include "synth.h"
#include "egg/egg.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>
#include <math.h>

struct synth_pcm;
struct synth_node;
struct synth_node_type;
struct synth_printer;

/* The global update gets sliced into bits no longer than this (NB frames, not samples).
 * This is mostly so nodes have a guaranteed maximum update length, for buffering purposes.
 */
#define SYNTH_UPDATE_LIMIT_FRAMES 1024
#define SYNTH_UPDATE_LIMIT_SAMPLES (SYNTH_UPDATE_LIMIT_FRAMES<<1)

#define SYNTH_WAVE_SIZE_BITS 10
#define SYNTH_WAVE_SIZE_SAMPLES (1<<SYNTH_WAVE_SIZE_BITS)
#define SYNTH_WAVE_SHIFT (32-SYNTH_WAVE_SIZE_BITS)

#define SYNTH_PCM_LIMIT (1<<20)

/* Extra trim applied across the board, at the channel level.
 */
#define SYNTH_GLOBAL_TRIM 0.400

#include "synth_env.h"
#include "synth_printer.h"
#include "synth_node.h"
#include "synth_formats.h"

/* Context.
 ************************************************************************/
 
struct synth {
  int rate,chanc;
  float *qbuf;
  int qbufa;
  
  /* All of type "bus" or "pcm", and all can update at the master chanc.
   */
  struct synth_node **busv;
  int busc,busa;
  
  struct synth_printer **printerv;
  int printerc,printera;
  int new_printer_framec;
  
  /* Sounds and songs are dumped in one table to save us the effort of managing two.
   * Songs run straight off their (src,srcc); sounds acquire (pcm) at the first play.
   */
  struct synth_res {
    int tid,rid;
    const void *src;
    int srcc;
    struct synth_pcm *pcm;
  } *resv;
  int resc,resa;
  
  // For synth_play_song_handoff().
  void *song_storage;
  
  float *sine; // SYNTH_WAVE_SIZE_SAMPLES if not null.
  float freq_by_noteid[128];
};

void synth_update_before(struct synth *synth,int framec);
void synth_update_after(struct synth *synth);
void synth_update_inner(float *v,int c,struct synth *synth);

struct synth_res *synth_res_get(const struct synth *synth,int tid,int rid);

/* Look up a sound resource in our store.
 * If it exists but hasn't been printed yet, arrange to start that.
 * Returns WEAK.
 */
struct synth_pcm *synth_get_pcm(struct synth *synth,int rid);

// => STRONG
struct synth_pcm *synth_acquire_pcm(struct synth *synth,const void *src,int srcc,int rid);

/* Bits.
 *************************************************************************/
 
static inline void synth_signal_add(float *dst,const float *src,int c) {
  for (;c-->0;dst++,src++) (*dst)+=(*src);
}

static inline void synth_signal_mlts(float *v,int c,float a) {
  for (;c-->0;v++) (*v)*=a;
}

int synth_require_sine(struct synth *synth);
float *synth_wave_generate_primitive(struct synth *synth,int shape);
float *synth_wave_generate_harmonics(struct synth *synth,const void *src,int coefc); // Each is u16, ie total length is (coefc<<1) bytes.

#endif
