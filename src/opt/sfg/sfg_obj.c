#include "sfg_internal.h"

/* PCM dump.
 */
 
void sfg_pcm_del(struct sfg_pcm *pcm) {
  if (!pcm) return;
  if (pcm->refc-->1) return;
  free(pcm);
}

int sfg_pcm_ref(struct sfg_pcm *pcm) {
  if (!pcm) return -1;
  if (pcm->refc<1) return -1;
  if (pcm->refc==INT_MAX) return -1;
  pcm->refc++;
  return 0;
}

struct sfg_pcm *sfg_pcm_new(int samplec) {
  if (samplec<0) return 0;
  if (samplec>(INT_MAX-1024)/sizeof(float)) return 0;
  struct sfg_pcm *pcm=calloc(1,sizeof(struct sfg_pcm)*sizeof(float)*samplec);
  if (!pcm) return 0;
  pcm->refc=1;
  pcm->c=samplec;
  return pcm;
}

/* Delete voice.
 */
 
static void sfg_voice_cleanup(struct sfg_voice *voice) {
  //TODO
}

/* Delete printer.
 */

void sfg_printer_del(struct sfg_printer *printer) {
  if (!printer) return;
  sfg_pcm_del(printer->pcm);
  if (printer->voicev) {
    while (printer->voicec-->0) sfg_voice_cleanup(printer->voicev+printer->voicec);
    free(printer->voicev);
  }
  free(printer);
}

/* New printer.
 */
 
struct sfg_printer *sfg_printer_new(const void *src,int srcc,int rate) {
  if ((rate<SFG_RATE_MIN)||(rate>SFG_RATE_MAX)) return 0;
  struct sfg_printer *printer=calloc(1,sizeof(struct sfg_printer));
  if (!printer) return 0;
  printer->rate=rate;
  int samplec=sfg_printer_decode(printer,src,srcc);
  if (samplec<0) {
    sfg_printer_del(printer);
    return 0;
  }
  if (!(printer->pcm=sfg_pcm_new(samplec))) {
    sfg_printer_del(printer);
    return 0;
  }
  return printer;
}

/* Trivial accessors.
 */

struct sfg_pcm *sfg_printer_get_pcm(const struct sfg_printer *printer) {
  if (!printer) return 0;
  return printer->pcm;
}

/* Update.
 */

int sfg_printer_update(struct sfg_printer *printer,int c) {
  if (c>0) {
    int updc=printer->pcm->c-printer->p;
    if (updc>c) updc=c;
    if (updc>0) {
      sfg_printer_update_internal(printer->pcm->v,updc,printer);
      printer->p+=updc;
    }
  }
  if (printer->p>=printer->pcm->c) return 0;
  return 1;
}

/* Add voice.
 */
 
struct sfg_voice *sfg_printer_spawn_voice(struct sfg_printer *printer) {
  if (!printer) return 0;
  if (printer->voicec>=printer->voicea) {
    int na=printer->voicea+4;
    if (na>INT_MAX/sizeof(struct sfg_voice)) return 0;
    void *nv=realloc(printer->voicev,sizeof(struct sfg_voice)*na);
    if (!nv) return 0;
    printer->voicev=nv;
    printer->voicea=na;
  }
  struct sfg_voice *voice=printer->voicev+printer->voicec++;
  memset(voice,0,sizeof(struct sfg_voice));
  return voice;
}
