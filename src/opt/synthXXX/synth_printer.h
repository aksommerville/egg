/* synth_printer.h
 * Run a private mono bus to generate a PCM dump.
 */
 
#ifndef SYNTH_PRINTER_H
#define SYNTH_PRINTER_H

struct synth_printer {
  struct synth *synth; // WEAK
  struct synth_pcm *pcm; // STRONG. Length is established at startup, but we may reduce it at the end. Samples are zero until we reach them.
  struct synth_node *bus; // STRONG
  int p;
};

void synth_printer_del(struct synth_printer *printer);

/* Caller must arrange to keep (src) in scope until printing completes.
 */
struct synth_printer *synth_printer_new(struct synth *synth,const void *src,int srcc);

/* Print at least (c) more samples, or up to the end.
 * Return >0 if more remains to be printed.
 * Update with (c<=0) to only test completion.
 */
int synth_printer_update(struct synth_printer *printer,int c);

#endif
