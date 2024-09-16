/* synth_osc.h
 * General oscillator, esp for LFO.
 */
 
#ifndef SYNTH_OSC_H
#define SYNCH_OSC_H

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

struct synth_wave {
  float v[SYNTH_WAVE_SIZE_SAMPLES];
};

/* Oscillator.
 ************************************************************/

struct synth_osc {
  const struct synth_wave *wave;
  uint32_t p;
  uint32_t dp;
  float scale;
};

/* All LFO fields encode the same way: (u16 ms,u0.16 range,u0.8 phase)
 * We'll accept that and arrange to emit in -range..range.
 * If you're expecting a supernormal range, multiply (osc->scale) after decoding.
 */
int synth_osc_decode(struct synth_osc *osc,struct synth *synth,const void *src,int srcc);

// Overwrites (v).
void synth_osc_update(float *v,int c,struct synth_osc *osc);

static inline float synth_osc_next(struct synth_osc *osc) {
  float v=osc->wave->v[osc->p>>SYNTH_WAVE_SHIFT]*osc->scale;
  osc->p+=osc->dp;
  return v;
}

#endif
