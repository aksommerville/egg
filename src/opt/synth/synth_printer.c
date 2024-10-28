#include "synth_internal.h"

/* Delete.
 */
 
void synth_printer_del(struct synth_printer *printer) {
  if (!printer) return;
  synth_node_del(printer->bus);
  synth_pcm_del(printer->pcm);
  free(printer);
}

/* New.
 */
 
struct synth_printer *synth_printer_new(struct synth *synth,const void *src,int srcc) {
  struct synth_printer *printer=calloc(1,sizeof(struct synth_printer));
  if (!printer) return 0;
  printer->synth=synth;
  
  if (
    !(printer->bus=synth_node_new(&synth_node_type_bus,1,synth))||
    (synth_node_bus_configure(printer->bus,src,srcc,0,-1)<0)||
    (synth_node_ready(printer->bus)<0)
  ) {
    synth_printer_del(printer);
    return 0;
  }
  
  int ms=synth_node_bus_get_duration(printer->bus);
  int framec=(int)ceil(((double)ms*(double)synth->rate)/1000.0);
  if (framec<1) framec=1;
  if (!(printer->pcm=synth_pcm_new(framec))) {
    synth_printer_del(printer);
    return 0;
  }

  return printer;
}

/* Update.
 */
 
int synth_printer_update(struct synth_printer *printer,int c) {
  int updc=printer->pcm->c-printer->p;
  if (updc>c) updc=c;
  if (updc>0) {
    while (updc>=SYNTH_UPDATE_LIMIT_FRAMES) {
      printer->bus->update(printer->pcm->v+printer->p,SYNTH_UPDATE_LIMIT_FRAMES,printer->bus);
      printer->p+=SYNTH_UPDATE_LIMIT_FRAMES;
      updc-=SYNTH_UPDATE_LIMIT_FRAMES;
    }
    if (updc>0) {
      printer->bus->update(printer->pcm->v+printer->p,updc,printer->bus);
      printer->p+=updc;
    }
  }
  return (printer->p<printer->pcm->c)?1:0;
}
