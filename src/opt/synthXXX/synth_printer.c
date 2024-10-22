#include "synth_internal.h"

/* Delete.
 */
 
void synth_printer_del(struct synth_printer *printer) {
  if (!printer) return;
  synth_pcm_del(printer->pcm);
  synth_node_del(printer->bus);
  free(printer);
}

/* New.
 */

struct synth_printer *synth_printer_new(struct synth *synth,const void *src,int srcc) {
  if (!synth) return 0;
  struct synth_printer *printer=calloc(1,sizeof(struct synth_printer));
  if (!printer) return 0;
  printer->synth=synth;
  
  if (
    !(printer->bus=synth_node_new(synth,&synth_node_type_bus,1))||
    (synth_node_bus_constrain_for_sounds(printer->bus)<0)||
    (synth_node_bus_configure(printer->bus,src,srcc)<0)||
    (synth_node_ready(printer->bus)<0)
  ) {
    synth_printer_del(printer);
    return 0;
  }
  
  int framec=synth_node_bus_get_duration(printer->bus);
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
    printer->bus->update(printer->pcm->v+printer->p,updc,printer->bus);
    printer->p+=updc;
  }
  if (printer->p<printer->pcm->c) return 1;
  while (printer->pcm->c>1) {
    float q=printer->pcm->v[printer->pcm->c-1];
    if (q<-0.00001) break;
    if (q> 0.00001) break;
    printer->pcm->c--;
  }
  return (printer->p>=printer->pcm->c)?0:1;
}
