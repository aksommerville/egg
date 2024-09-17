#ifndef SYNTH_INTERNAL_H
#define SYNTH_INTERNAL_H

#define SYNTH_RATE_MIN 200
#define SYNTH_RATE_MAX 200000
#define SYNTH_CHANC_MIN 1
#define SYNTH_CHANC_MAX 8 /* We'll only use the first two. */
#define SYNTH_QBUF_SIZE_FRAMES 1024
#define SYNTH_UPDATE_LIMIT_FRAMES 1024
#define SYNTH_PCM_SIZE_LIMIT (1<<20) /* Hard limit in samples for safety. Outside of this, we impose a 5-second limit. */
#define SYNTH_CHANNEL_COUNT 16 /* Not subject to debate; inherited from MIDI. */

#define SYNTH_WAVE_SIZE_BITS 10
#define SYNTH_WAVE_SIZE_SAMPLES (1<<SYNTH_WAVE_SIZE_BITS)
#define SYNTH_WAVE_SHIFT (32-SYNTH_WAVE_SIZE_BITS)

#include "synth.h"
#include "synth_formats.h"
#include "synth_env.h"
#include "synth_osc.h"
#include "synth_node.h"
#include "synth_filters.h"
#include "synth_printer.h"
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <stdio.h>
#include <math.h>

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
    float pan; // If nonzero, sound expressed a preference for pan in drum kits.
  } *soundv;
  int soundc,sounda;
  
  struct synth_song {
    int rid;
    const void *v;
    int c;
  } *songv;
  int songc,songa;
  
  struct synth_node **busv;
  int busc,busa;
  struct synth_node *song; // WEAK. The last requested song.
  int fade_time_normal; // Fade out when replacing with another song.
  int fade_time_quick; // Fade out incoming song when cancelling the outgoing fade. Incoming is usually hard-delayed so doesn't matter.
  int new_song_delay;
  float ratefv[128]; // Normalized rates, indexed by noteid.
  struct synth_wave sine;
  
  struct synth_printer **printerv;
  int printerc,printera;
  int print_framec; // Nonzero during updates. New printers must produce so much immediately.
};

int synth_soundv_search(const struct synth *synth,int rid,int index);
int synth_songv_search(const struct synth *synth,int rid);

/* Noop if (sound->pcm) exists.
 * Otherwise we do the necessaries to get it printed, or fail.
 * New PCMs usually aren't fully printed right away.
 */
int synth_sound_require(struct synth *synth,struct synth_sound *sound);

// "unlist" to only drop WEAK references, for a bus you've removed manually.
// "kill" to also effect that manual removal, and delete the node.
void synth_unlist_bus(struct synth *synth,struct synth_node *bus);
void synth_kill_bus(struct synth *synth,struct synth_node *bus);

// Add an unconfigured bus. (type) defaults to bus, but can be anything that generates a signal.
struct synth_node *synth_add_bus(struct synth *synth,const struct synth_node_type *type);

// Generate reference sine wave from scratch -- This should only happen once.
void synth_wave_generate_sine(struct synth_wave *wave);

// Coefficients are u0.16; provide in encoded form.
void synth_wave_synthesize(struct synth_wave *wave,struct synth *synth,int shape,const uint8_t *coefv,int coefc);

#endif
