/* synth_printer.h
 * Plays an EGS song and captures into a mono buffer.
 */
 
#ifndef SYNTH_PRINTER_H
#define SYNTH_PRINTER_H

struct synth_printer {
  struct synth *synth; // WEAK
  struct synth_pcm *pcm; // STRONG
  struct synth_node *bus; // STRONG
  int p;
};

void synth_printer_del(struct synth_printer *printer);
struct synth_printer *synth_printer_new(struct synth *synth,const void *src,int srcc);

// Returns zero if finished.
int synth_printer_update(struct synth_printer *printer,int c);

struct synth_pcm {
  int refc;
  int c;
  float v[];
};

void synth_pcm_del(struct synth_pcm *pcm);
int synth_pcm_ref(struct synth_pcm *pcm);
struct synth_pcm *synth_pcm_new(int c);

/* Decode PCM resource.
 * These have a declared sample rate, and we resample to suit the provided master rate.
 */
static inline struct synth_pcm *synth_pcm_decode(int rate,const void *src,int srcc) { return 0; }//TODO

#endif
