/* synth_pcm.h
 */
 
#ifndef SYNTH_PCM_H
#define SYNTH_PCM_H

/* Single-period wave.
 **********************************************************************/
 
#define SYNTH_WAVE_SIZE_BITS 10
#define SYNTH_WAVE_SIZE_SAMPLES (1<<SYNTH_WAVE_SIZE_BITS)
#define SYNTH_WAVE_SHIFT (32-SYNTH_WAVE_SIZE_BITS)

struct synth_wave {
  int refc;
  float v[SYNTH_WAVE_SIZE_SAMPLES];
};

void synth_wave_del(struct synth_wave *wave);
int synth_wave_ref(struct synth_wave *wave);
struct synth_wave *synth_wave_new();

/* (synth->sine) is populated if we succeed.
 */
int synth_require_sine(struct synth *synth);

/* Overwrite a single-period wave in (dst), from encoded data straight off a wave channel header.
 * Returns the consumed length from (src).
 * - u8 Shape: (0,1,2,3,4)=(custom,sine,square,saw,triangle)
 * - u8 Harmonics count, if shape==0
 * - ... Harmonics, u16 each.
 */
int synth_wave_decode(struct synth_wave *dst,struct synth *synth,const void *src,int srcc);

/* PCM dump.
 *********************************************************************/

struct synth_pcm {
  int refc;
  int c;
  float v[];
};

void synth_pcm_del(struct synth_pcm *pcm);
int synth_pcm_ref(struct synth_pcm *pcm);
struct synth_pcm *synth_pcm_new(int c);

/* From sanitized WAV or EGS.
 */
struct synth_pcm *synth_pcm_decode(const void *src,int srcc,int dstrate);

/* Printer.
 ********************************************************************/
 
struct synth_printer {
  struct synth_pcm *pcm;
  int p;
  struct synth *synth; // STRONG, not the parent synthesizer.
};

void synth_printer_del(struct synth_printer *printer);

/* We borrow (src) and keep reading it throughout printer's life.
 * Caller must hold it constant.
 */
struct synth_printer *synth_printer_new(const void *src,int srcc,int rate);

/* Returns <=0 when finished.
 */
int synth_printer_update(struct synth_printer *printer,int c);

#endif
