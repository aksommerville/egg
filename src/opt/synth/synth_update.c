#include "synth_internal.h"

/* Pre-update hook.
 */
 
void synth_update_before(struct synth *synth,int framec) {
  int i=synth->printerc,err;
  while (i-->0) {
    struct synth_printer *printer=synth->printerv[i];
    if ((err=synth_printer_update(printer,framec))<=0) {
      fprintf(stderr,"%s: Drop printer %p, status=%d\n",__func__,printer,err);
      synth->printerc--;
      memmove(synth->printerv+i,synth->printerv+i+1,sizeof(void*)*(synth->printerc-i));
      synth_printer_del(printer);
    }
  }
  synth->new_printer_framec=framec;
}

/* Post-update hook.
 */
 
void synth_update_after(struct synth *synth) {
  synth->new_printer_framec=0;
  int i=synth->busc;
  while (i-->0) {
    struct synth_node *bus=synth->busv[i];
    if (bus->finished) {
      fprintf(stderr,"%s: Drop bus %p due to finished.\n",__func__,bus);
      synth->busc--;
      memmove(synth->busv+i,synth->busv+i+1,sizeof(void*)*(synth->busc-i));
      synth_node_del(bus);
    }
  }
}

/* Update to bounded and zeroed buffer.
 */
 
static void synth_update_bounded(float *v,int framec,struct synth *synth) {
  int i=synth->busc;
  while (i-->0) {
    struct synth_node *bus=synth->busv[i];
    bus->update(v,framec,bus);
  }
}

/* Update per channel count, unbounded.
 * Buffer is zeroed by caller.
 */
 
static void synth_update_mono(float *v,int framec,struct synth *synth) {
  while (framec>SYNTH_UPDATE_LIMIT_FRAMES) {
    synth_update_bounded(v,SYNTH_UPDATE_LIMIT_FRAMES,synth);
    v+=SYNTH_UPDATE_LIMIT_FRAMES;
    framec-=SYNTH_UPDATE_LIMIT_FRAMES;
  }
  synth_update_bounded(v,framec,synth);
}
 
static void synth_update_stereo(float *v,int framec,struct synth *synth) {
  while (framec>SYNTH_UPDATE_LIMIT_FRAMES) {
    synth_update_bounded(v,SYNTH_UPDATE_LIMIT_FRAMES,synth);
    v+=SYNTH_UPDATE_LIMIT_FRAMES<<1;
    framec-=SYNTH_UPDATE_LIMIT_FRAMES;
  }
  synth_update_bounded(v,framec,synth);
}
 
static void synth_update_multi(float *v,int framec,struct synth *synth) {
  synth_update_stereo(v,framec,synth);
  int skipc=synth->chanc-2;
  float *dst=v+framec*synth->chanc;
  const float *src=v+framec*2;
  while (framec-->0) {
    dst-=skipc;
    *(--dst)=*(--src);
    *(--dst)=*(--src);
  }
}

/* Update, dispatch on channel count.
 */
 
void synth_update_inner(float *v,int c,struct synth *synth) {
  memset(v,0,sizeof(float)*c);
  switch (synth->chanc) {
    case 1: synth_update_mono(v,c,synth); break;
    case 2: synth_update_stereo(v,c>>1,synth); break;
    default: synth_update_multi(v,c/synth->chanc,synth); break;
  }
}
