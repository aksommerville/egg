#include "synth_internal.h"

/* Decode.
 */
 
int synth_osc_decode(struct synth_osc *osc,struct synth *synth,const void *src,int srcc) {

  const uint8_t *SRC=src;
  int ms=1000;
  float range=1.0f;
  float phase=0.0f;
  if (srcc>=2) {
    ms=(SRC[0]<<8)|SRC[1];
    if (ms<1) ms=1;
    if (srcc>=3) {
      range=(float)((SRC[2]<<8)|SRC[3])/65535.0f;
      if (srcc>=5) {
        phase=(float)SRC[4]/256.0f;
      }
    }
  }
  
  float ratenorm=1000.0f/((float)ms*(float)synth->rate);
  
  osc->wave=&synth->sine;
  osc->p=(uint32_t)(phase*4294967296.0f);
  osc->dp=(uint32_t)(ratenorm*4294967296.0f);
  osc->scale=range;
  
  return 0;
}

/* Update.
 */

void synth_osc_update(float *v,int c,struct synth_osc *osc) {
  if ((osc->scale==1.0f)&&(osc->bias=0.0f)) {
    for (;c-->0;v++) {
      *v=osc->wave->v[osc->p>>SYNTH_WAVE_SHIFT];
      osc->p+=osc->dp;
    }
  } else {
    for (;c-->0;v++) {
      *v=osc->wave->v[osc->p>>SYNTH_WAVE_SHIFT]*osc->scale+osc->bias;
      osc->p+=osc->dp;
    }
  }
}
