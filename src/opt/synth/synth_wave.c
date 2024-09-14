#include "synth_internal.h"
#include <string.h>
#include <math.h>

/* Generate primitives.
 */
 
void synth_wave_generate_sine(struct synth_wave *wave) {
  float *v=wave->v;
  int i=SYNTH_WAVE_SIZE_SAMPLES;
  float p=0.0f,dp=(M_PI*2.0f)/SYNTH_WAVE_SIZE_SAMPLES;
  for (;i-->0;v++,p+=dp) *v=sinf(p);
}
 
static void synth_wave_generate_square(struct synth_wave *wave) {
  const int halflen=SYNTH_WAVE_SIZE_SAMPLES>>1;
  float *v=wave->v;
  int i=halflen;
  for (;i-->0;v++) *v=1.0f;
  for (i=halflen;i-->0;v++) *v=-1.0f;
}

static void synth_wave_generate_saw(float *v,int c,float d) {
  d/=(float)c;
  int i=c;
  float p=(d>=0.0f)?-1.0f:1.0f;
  for (;i-->0;v++,p+=d) *v=p;
}

static void synth_wave_generate_triangle(struct synth_wave *wave) {
  const int halflen=SYNTH_WAVE_SIZE_SAMPLES>>1;
  synth_wave_generate_saw(wave->v,halflen,1.0f);
  synth_wave_generate_saw(wave->v+halflen,halflen,-1.0f);
}

/* Harmonize against self.
 */
 
static void synth_wave_add_harmonic(struct synth_wave *dst,const struct synth_wave *src,float coef,int step) {
  if (step>=SYNTH_WAVE_SIZE_SAMPLES) return;
  float *dstp=dst->v;
  int srcp=0,i=SYNTH_WAVE_SIZE_SAMPLES;
  for (;i-->0;dstp++) {
    (*dstp)+=src->v[srcp]*coef;
    if ((srcp+=step)>=SYNTH_WAVE_SIZE_SAMPLES) srcp-=SYNTH_WAVE_SIZE_SAMPLES;
  }
}
 
static void synth_wave_harmonics(struct synth_wave *wave,const uint8_t *serial,int coefc) {
  struct synth_wave ref;
  memcpy(&ref,wave,sizeof(struct synth_wave));
  memset(wave,0,sizeof(struct synth_wave));
  int step=1;
  for (;step<=coefc;step++,serial+=2) {
    if (!serial[0]&&!serial[1]) continue;
    float coef=(float)((serial[0]<<8)|serial[1])/65535.0f;
    synth_wave_add_harmonic(wave,&ref,coef,step);
  }
}

/* Shape and harmonics.
 */
 
void synth_wave_synthesize(struct synth_wave *wave,struct synth *synth,int shape,const uint8_t *coefv,int coefc) {
  switch (shape) {
    case 0: memcpy(wave->v,synth->sine.v,sizeof(synth->sine.v)); break;
    case 1: synth_wave_generate_square(wave); break;
    case 2: synth_wave_generate_saw(wave->v,SYNTH_WAVE_SIZE_SAMPLES,-1.0f); break;
    case 3: synth_wave_generate_triangle(wave); break;
    default: memcpy(wave->v,synth->sine.v,sizeof(synth->sine.v)); break; // unknown => sine
  }
  coefc>>=1;
  if (coefc) synth_wave_harmonics(wave,coefv,coefc);
}
