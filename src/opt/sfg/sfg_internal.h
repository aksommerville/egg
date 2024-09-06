#ifndef SFG_INTERNAL_H
#define SFG_INTERNAL_H

#include "sfg.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <math.h>

#define SFG_RATE_MIN 200
#define SFG_RATE_MAX 200000

#define SFG_SHAPE_SINE 0
#define SFG_SHAPE_SQUARE 1
#define SFG_SHAPE_SAW 2
#define SFG_SHAPE_TRIANGLE 3
#define SFG_SHAPE_NOISE 4

struct sfg_voice {
// Oscillator.
  uint8_t shape;
  //TODO wave
  float fmrate; // relative to carrier
  //TODO fm range env
  float fmlforate; // hz
  float fmlfodepth;
  //TODO freq env
  float freqlforate; // hz
  int freqlfodepth; // cents (TODO normalize at decode?)
};

struct sfg_printer {
  struct sfg_pcm *pcm;
  int rate;
  int p;
  struct sfg_voice *voicev;
  int voicec,voicea;
};

/* Generate the signal graph for an encoded sound into a fresh printer.
 * Returns the output duration in samples on success.
 * (printer->rate) must be populated first, and all else zeroed.
 */
int sfg_printer_decode(struct sfg_printer *printer,const uint8_t *src,int srcc);

/* Generate (c) samples at (v), which must be initially zeroed.
 * Does not update (printer->p) and doesn't care whether (v) belongs to (printer->pcm).
 * Does advance state of the signal graph.
 */
int sfg_printer_update_internal(float *v,int c,struct sfg_printer *printer);

struct sfg_voice *sfg_printer_spawn_voice(struct sfg_printer *printer);

#endif
