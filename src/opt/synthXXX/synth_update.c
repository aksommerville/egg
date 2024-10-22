#include "synth_internal.h"

/* Update printers and remove any that finish.
 */
 
static void synth_update_printers(struct synth *synth,int c) {
  int i=synth->printerc;
  while (i-->0) {
    struct synth_printer *printer=synth->printerv[i];
    int err=synth_printer_update(printer,c);
    if (err>0) continue;
    synth->printerc--;
    memmove(synth->printerv+i,synth->printerv+i+1,sizeof(void*)*(synth->printerc-i));
    synth_printer_del(printer);
  }
}

/* Update, floating-point, mono.
 * Buffer is zeroed in advance, we only add to it.
 */
 
static void synth_updatef_mono(float *v,int c,struct synth *synth) {
  int i=synth->busc;
  while (i-->0) {
    struct synth_node *bus=synth->busv[i];
    if (bus->chanc!=1) continue;
    bus->update(v,c,bus);
    if (bus->finished) {
      synth->busc--;
      memmove(synth->busv+i,synth->busv+i+1,sizeof(void*)*(synth->busc-i));
      synth_node_del(bus);
      synth_unlist_bus(synth,bus);
    }
  }
}

/* Update, floating-point, stereo.
 */
 
static void synth_updatef_stereo(float *v,int framec,struct synth *synth) {
  int i=synth->busc;
  while (i-->0) {
    struct synth_node *bus=synth->busv[i];
    if (bus->chanc!=2) continue;
    bus->update(v,framec,bus);
    if (bus->finished) {
      synth->busc--;
      memmove(synth->busv+i,synth->busv+i+1,sizeof(void*)*(synth->busc-i));
      synth_node_del(bus);
    }
  }
}

/* Update, floating-point, limited length, any channel count.
 */
 
static void synth_updatef_limited(float *v,int framec,struct synth *synth) {
  switch (synth->chanc) {
    case 1: synth_updatef_mono(v,framec,synth); break;
    case 2: synth_updatef_stereo(v,framec,synth); break;
    default: {
        synth_updatef_stereo(v,framec,synth);
        const float *src=v+(framec<<1);
        float *dst=v+framec*synth->chanc;
        int zeroc=synth->chanc-2;
        while (framec-->0) {
          int i=zeroc; while (i-->0) *(--dst)=0.0f;
          *(--dst)=*(--src);
          *(--dst)=*(--src);
        }
      }
  }
}

/* Update, floating-point, unlimited length.
 */

void synth_updatef(float *v,int c,struct synth *synth) {
  synth->print_framec=c;
  synth_update_printers(synth,c);
  memset(v,0,sizeof(float)*c);
  int framec=c/synth->chanc;
  while (framec>SYNTH_UPDATE_LIMIT_FRAMES) {
    synth_updatef_limited(v,framec,synth);
    v+=framec*synth->chanc;
    framec-=SYNTH_UPDATE_LIMIT_FRAMES;
  }
  if (framec>0) {
    synth_updatef_limited(v,framec,synth);
  }
  synth->print_framec=0;
}

/* Update, integer, unlimited length.
 */
 
void synth_updatei(int16_t *v,int c,struct synth *synth) {
  if (!synth->qbuf) {
    int na=SYNTH_QBUF_SIZE_FRAMES*synth->chanc;
    if (!(synth->qbuf=malloc(sizeof(float)*na))) {
      memset(v,0,c<<1);
      return;
    }
    synth->qbufa=na;
  }
  while (c>0) {
    int updc=c;
    if (updc>synth->qbufa) updc=synth->qbufa;
    synth_updatef(synth->qbuf,updc,synth);
    const float *src=synth->qbuf;
    int i=updc; for (;i-->0;v++,src++) {
      if (*src<=-1.0f) *v=-32768;
      else if (*src>=1.0f) *v=32767;
      else *v=(*src)*32767.0f;
    }
    c-=updc;
  }
}
