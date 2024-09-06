/* sfg.h
 * Printer for SFG sounds.
 */
 
#ifndef SFG_H
#define SFG_H

struct sfg_pcm {
  int id;
  int refc;
  int c;
  float v[];
};

struct sfg_printer;

/* Given an encoded 'sounds' (SFG) resource, return the individual sound at the given index.
 * (index) is 1..0xffff.
 * On success, (*dstp) will point into (src) somewhere.
 * -1 if not found and 0 if present but empty, but you shouldn't normally distinguish those.
 */
int sfg_get_sound_serial(void *dstpp,const void *src,int srcc,int index);

/* Call (cb) with each sound in order, including empty ones.
 * Return nonzero to stop.
 */
int sfg_serial_for_each(
  const void *src,int srcc,
  int (*cb)(int index,const void *src,int srcc,void *userdata),
  void *userdata
);

/* PCM dumps are reference-counted and have an immutable length.
 * They also have an 'id' field, unused by sfg and initially zero, caller can do whatever with it.
 */
void sfg_pcm_del(struct sfg_pcm *pcm);
int sfg_pcm_ref(struct sfg_pcm *pcm);
struct sfg_pcm *sfg_pcm_new(int c);

void sfg_printer_del(struct sfg_printer *printer);

/* Serial resource is decoded during new, and the PCM dump is created.
 * No errors are possible after this succeeds.
 */
struct sfg_printer *sfg_printer_new(const void *src,int srcc,int rate);

/* You can fetch the PCM dump at any time after construction.
 * Its duration is correct always.
 * Its samples are zero until sfg_printer_update() reaches them.
 * Length zero is legal.
 */
struct sfg_pcm *sfg_printer_get_pcm(const struct sfg_printer *printer);

/* Generate (c) more samples.
 * Returns >0 if more samples remain, 0 if complete, or <0 (not possible) for errors.
 * You can call with (c<=0) to only test for completion.
 */
int sfg_printer_update(struct sfg_printer *printer,int c);

#endif
