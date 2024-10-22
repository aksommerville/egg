#include "synth_internal.h"

/* Require the global sine wave.
 */
 
int synth_require_sine(struct synth *synth) {
  if (synth->sine) return 0;
  if (!(synth->sine=malloc(sizeof(float)*SYNTH_WAVE_SIZE_SAMPLES))) return -1;
  float *dst=synth->sine;
  float p=0.0f,dp=(M_PI*2.0f)/SYNTH_WAVE_SIZE_SAMPLES,i=SYNTH_WAVE_SIZE_SAMPLES;
  for (;i-->0;dst++,p+=dp) *dst=sinf(p);
  return 0;
}

/* Primitives.
 */
 
static void synth_wave_generate_sine(float *dst,struct synth *synth) {
  if (synth_require_sine(synth)<0) {
    memset(dst,0,SYNTH_WAVE_SIZE_SAMPLES*sizeof(float));
    return;
  }
  memcpy(dst,synth->sine,SYNTH_WAVE_SIZE_SAMPLES*sizeof(float));
}

static void synth_wave_generate_square(float *dst) {
  int frontc=SYNTH_WAVE_SIZE_SAMPLES>>1;
  int backc=SYNTH_WAVE_SIZE_SAMPLES-frontc;
  for (;frontc-->0;dst++) *dst=1.0f;
  for (;backc-->0;dst++) *dst=-1.0f;
}

static void synth_wave_generate_saw(float *dst) {
  float p=1.0f;
  float dp=-2.0f/(float)SYNTH_WAVE_SIZE_SAMPLES;
  int i=SYNTH_WAVE_SIZE_SAMPLES;
  for (;i-->0;dst++,p+=dp) *dst=p;
}

static void synth_wave_generate_triangle(float *dst) {
  int halfc=SYNTH_WAVE_SIZE_SAMPLES>>1;
  float p=-1.0f;
  float dp=2.0f/(float)halfc;
  float *head=dst,*tail=dst+SYNTH_WAVE_SIZE_SAMPLES-1;
  int i=halfc;
  for (;i-->0;head++,tail--,p+=dp) *head=*tail=p;
}

/* Primitives, dispatch.
 */
 
float *synth_wave_generate_primitive(struct synth *synth,int shape) {
  float *v=malloc(sizeof(float)*SYNTH_WAVE_SIZE_SAMPLES);
  if (!v) return 0;
  switch (shape) { // See etc/doc/audio-format.md
    case 1: synth_wave_generate_sine(v,synth); break;
    case 2: synth_wave_generate_square(v); break;
    case 3: synth_wave_generate_saw(v); break;
    case 4: synth_wave_generate_triangle(v);
    default: free(v); return 0;
  }
  return v;
}

/* Harmonics.
 */
 
static void synth_wave_add_harmonic(float *dst,const float *src,int step,float level) {
  int srcp=0,i=SYNTH_WAVE_SIZE_SAMPLES;
  for (;i-->0;dst++,srcp+=step) {
    if (srcp>=SYNTH_WAVE_SIZE_SAMPLES) srcp-=SYNTH_WAVE_SIZE_SAMPLES;
    (*dst)+=src[srcp]*level;
  }
}
 
float *synth_wave_generate_harmonics(struct synth *synth,const void *src,int coefc) {
  if (synth_require_sine(synth)<0) return 0;
  if (coefc>=SYNTH_WAVE_SIZE_SAMPLES) coefc=SYNTH_WAVE_SIZE_SAMPLES-1;
  const uint8_t *SRC=src;
  float *dst=calloc(sizeof(float),SYNTH_WAVE_SIZE_SAMPLES);
  int step=1; for (;step<=coefc;step++,SRC+=2) {
    int coef=(SRC[0]<<8)|SRC[1];
    if (!coef) continue;
    synth_wave_add_harmonic(dst,synth->sine,step,(float)coef/65535.0f);
  }
  return dst;
}
