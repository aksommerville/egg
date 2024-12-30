#include "synth_internal.h"

/* Instance.
 */
 
struct synth_voice_sub {
  struct synth_voice hdr;
  struct synth_env_runner levelenv;
  float cv[5]; // IIR coefficients (3 dry, then 2 wet). Constant after construction.
  float mva[5]; // IIR memory, first pass.
  float mvb[5]; // IIR memory, second pass.
  float gain;
};

#define VOICE ((struct synth_voice_sub*)voice)

/* Delete.
 */
 
static void _sub_del(struct synth_voice *voice) {
}

/* Apply IIR filter.
 */
 
static inline float synth_sub_apply_iir(float *mv,const float *cv,float dry) {
  mv[2]=mv[1];
  mv[1]=mv[0];
  mv[0]=dry;
  float wet=
    mv[0]*cv[0]+
    mv[1]*cv[1]+
    mv[2]*cv[2]+
    mv[3]*cv[3]+
    mv[4]*cv[4];
  mv[4]=mv[3];
  mv[3]=wet;
  return wet;
}

/* Update.
 */
 
static void _sub_update(float *v,int c,struct synth_voice *voice) {
  for (;c-->0;v++) {
  
    // White noise.
    float sample=((rand()&0xffff)-0x8000)/32768.0f;
    
    // IIR first pass.
    sample=synth_sub_apply_iir(VOICE->mva,VOICE->cv,sample);
    
    // IIR second pass.
    sample=synth_sub_apply_iir(VOICE->mvb,VOICE->cv,sample);
    
    // Gain and clip.
    sample*=VOICE->gain;
    if (sample<-1.0f) sample=-1.0f;
    else if (sample>1.0f) sample=1.0f;
    
    // Envelope, and emit.
    sample*=synth_env_next(VOICE->levelenv);
    (*v)+=sample;
  }
  if (VOICE->levelenv.finished) voice->finished=1;
}

/* Release.
 */
 
static void _sub_release(struct synth_voice *voice) {
  synth_env_release(&VOICE->levelenv);
}

/* New.
 */
 
struct synth_voice *synth_voice_sub_new(
  struct synth *synth,
  float mid_norm,
  float width_norm,
  float velocity,
  int durframes,
  const struct synth_env_config *levelenv
) {
  struct synth_voice *voice=synth_voice_new(synth,sizeof(struct synth_voice_sub));
  if (!voice) return 0;
  voice->magic='s';
  voice->del=_sub_del;
  voice->update=_sub_update;
  voice->release=_sub_release;
  synth_env_init(&VOICE->levelenv,levelenv,velocity,durframes);
  
  if (mid_norm<0.0f) mid_norm=0.0f;
  else if (mid_norm>0.5f) mid_norm=0.5f;
  if (width_norm<0.0f) width_norm=0.0f;
  else if (width_norm>0.5f) width_norm=0.5f;
  
  /* 3-point IIR bandpass.
   * I have only a vague idea of how this works, and the formula is taken entirely on faith.
   * Reference:
   *   Steven W Smith: The Scientist and Engineer's Guide to Digital Signal Processing
   *   Ch 19, p 326, Equation 19-7
   */
  float r=1.0f-3.0f*width_norm;
  float cosfreq=cosf(M_PI*2.0f*mid_norm);
  float k=(1.0f-2.0f*r*cosfreq+r*r)/(2.0f-2.0f*cosfreq);
  
  VOICE->cv[0]=1.0f-k;
  VOICE->cv[1]=2.0f*(k-r)*cosfreq;
  VOICE->cv[2]=r*r-k;
  VOICE->cv[3]=2.0f*r*cosfreq;
  VOICE->cv[4]=-r*r;
  
  VOICE->gain=10.0f;//TODO How to calculate ideal gain for a narrow bandpass?
  
  return voice;
}
