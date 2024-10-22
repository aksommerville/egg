#include "synth_internal.h"

/* Init IIR.
 */
 
void synth_iir3_init_lopass(struct synth_iir3 *iir3,float thresh) {
  
  /* Chebyshev low-pass filter, copied from the blue book without really understanding.
   * _The Scientist and Engineer's Guide to Digital Signal Processing_ by Steven W Smith, p 341.
   */
  float rp=-cosf(M_PI/4.0f);
  float ip=sinf(M_PI/4.0f);
  float t=2.0f*tanf(0.5f);
  float w=2.0f*M_PI*thresh;
  float m=rp*rp+ip*ip;
  float d=4.0f-4.0f*rp*t+m*t*t;
  float x0=(t*t)/d;
  float x1=(2.0f*t*t)/d;
  float x2=(t*t)/d;
  float y1=(8.0f-2.0f*m*t*t)/d;
  float y2=(-4.0f-4.0f*rp*t-m*t*t)/d;
  float k=sinf(0.5f-w/2.0f)/sinf(0.5f+w/2.0f);
   
  iir3->cv[0]=(x0-x1*k+x2*k*k)/d;
  iir3->cv[1]=(-2.0f*x0*k+x1+x1*k*k-2.0f*x2*k)/d;
  iir3->cv[2]=(x0*k*k-x1*k+x2)/d;
  iir3->cv[3]=(2.0f*k+y1+y1*k*k-2.0f*y2*k)/d;
  iir3->cv[4]=(-k*k-y1*k+y2)/d;
  
  float sa=iir3->cv[0]+iir3->cv[1]+iir3->cv[2];
  float sb=iir3->cv[3]+iir3->cv[4];
  float gain=(1.0f-sb)/sa;
  iir3->cv[0]*=gain;
  iir3->cv[1]*=gain;
  iir3->cv[2]*=gain;
}

void synth_iir3_init_hipass(struct synth_iir3 *iir3,float thresh) {
  
  /* Chebyshev high-pass filter, copied from the blue book without really understanding.
   * Basically the same as low-pass, just (k) is different and the two leading coefficients are negative.
   * _The Scientist and Engineer's Guide to Digital Signal Processing_ by Steven W Smith, p 341.
   */
  float rp=-cosf(M_PI/4.0f);
  float ip=sinf(M_PI/4.0f);
  float t=2.0f*tanf(0.5f);
  float w=2.0f*M_PI*thresh;
  float m=rp*rp+ip*ip;
  float d=4.0f-4.0f*rp*t+m*t*t;
  float x0=(t*t)/d;
  float x1=(2.0f*t*t)/d;
  float x2=(t*t)/d;
  float y1=(8.0f-2.0f*m*t*t)/d;
  float y2=(-4.0f-4.0f*rp*t-m*t*t)/d;
  float k=-cosf(w/2.0f+0.5f)/cosf(w/2.0f-0.5f);
   
  iir3->cv[0]=-(x0-x1*k+x2*k*k)/d;
  iir3->cv[1]=(-2.0f*x0*k+x1+x1*k*k-2.0f*x2*k)/d;
  iir3->cv[2]=(x0*k*k-x1*k+x2)/d;
  iir3->cv[3]=-(2.0f*k+y1+y1*k*k-2.0f*y2*k)/d;
  iir3->cv[4]=(-k*k-y1*k+y2)/d;
  
  float sa=iir3->cv[0]-iir3->cv[1]+iir3->cv[2];
  float sb=-iir3->cv[3]+iir3->cv[4];
  float gain=(1.0f-sb)/sa;
  iir3->cv[0]*=gain;
  iir3->cv[1]*=gain;
  iir3->cv[2]*=gain;
}

void synth_iir3_init_bpass(struct synth_iir3 *iir3,float mid,float width) {
  
  /* 3-point IIR bandpass.
   * I have only a vague idea of how this works, and the formula is taken entirely on faith.
   * Reference:
   *   Steven W Smith: The Scientist and Engineer's Guide to Digital Signal Processing
   *   Ch 19, p 326, Equation 19-7
   */
  float r=1.0f-3.0f*width;
  float cosfreq=cosf(M_PI*2.0f*mid);
  float k=(1.0f-2.0f*r*cosfreq+r*r)/(2.0f-2.0f*cosfreq);
  
  iir3->cv[0]=1.0f-k;
  iir3->cv[1]=2.0f*(k-r)*cosfreq;
  iir3->cv[2]=r*r-k;
  iir3->cv[3]=2.0f*r*cosfreq;
  iir3->cv[4]=-r*r;
}

void synth_iir3_init_notch(struct synth_iir3 *iir3,float mid,float width) {
  
  /* 3-point IIR notch.
   * From the same source as bandpass, and same level of mystery to me.
   */
  float r=1.0f-3.0f*width;
  float cosfreq=cosf(M_PI*2.0f*mid);
  float k=(1.0f-2.0f*r*cosfreq+r*r)/(2.0f-2.0f*cosfreq);
  
  iir3->cv[0]=k;
  iir3->cv[1]=-2.0f*k*cosfreq;
  iir3->cv[2]=k;
  iir3->cv[3]=2.0f*r*cosfreq;
  iir3->cv[4]=-r*r;
}

/* Circular buffer.
 */

void synth_cbuf_del(struct synth_cbuf *cbuf) {
  if (!cbuf) return;
  free(cbuf);
}

struct synth_cbuf *synth_cbuf_new(int c) {
  if (c<1) return 0;
  if (c>1000000) return 0; // Arbitrary sanity limit.
  struct synth_cbuf *cbuf=calloc(1,sizeof(struct synth_cbuf)+sizeof(float)*c);
  if (!cbuf) return 0;
  cbuf->c=c;
  return cbuf;
}
