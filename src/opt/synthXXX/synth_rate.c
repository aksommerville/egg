#include "synth.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

/* Multiplier from cents.
 */
 
float synth_multiplier_from_cents(int cents) {
  if (!cents) return 1.0f;
  float norm=(float)cents/1200.0f;
  return powf(2.0f,norm);
}

/* Generate rates table in absolute hz.
 */
 
void synth_rates_generate_hz(float *fv) {
  // Abstractly: hz=440.0f*powf(2.0f,(noteid-69.0f)/12.0f)
  // But we'll cheat it a little. Generate the octave starting at A4 (0x45, 440 Hz),
  // then half and double it the rest of the way.
  // We could multiply our way up that first octave too, but I think the risk of rounding error is too high.
  
  float *ref=fv+0x45;
  *ref=440.0f;
  int i=1;
  for (;i<12;i++) ref[i]=ref[0]*powf(2.0f,(float)i/12.0f);
  
  float *p=ref+12;
  for (i=0x45+12;i<128;i++,p++) *p=p[-12]*2.0f;
  for (i=0x45,p=ref-1;i-->0;p--) *p=p[12]*0.5f;
}

/* Normalize rates table per given main rate.
 */
 
void synth_rates_normalizeip(float *fv,int mainratehz) {
  if (mainratehz<1) {
    memset(fv,0,sizeof(float)*128);
    return;
  }
  float adjust=1.0f/(float)mainratehz;
  int i=128;
  for (;i-->0;fv++) {
    (*fv)*=adjust;
    if (*fv>0.5f) *fv=0.5f;
  }
}

/* Quantize normalized rates per build-time constants.
 */

void synth_rates_quantize(uint32_t *iv,const float *fv) {
  int i=128;
  for (;i-->0;iv++,fv++) {
    *iv=(uint32_t)((*fv)*4294967296.0f);
  }
}
