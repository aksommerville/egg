/* synth_filters.h
 * Primitives underlying IIR, Delay, etc.
 */
 
#ifndef SYNTH_FILTERS_H
#define SYNTH_FILTERS_H

/* 3-point IIR.
 */
 
struct synth_iir3 {
  float v[5];
  float cv[5];
};

void synth_iir3_init_lopass(struct synth_iir3 *iir3,float thresh);
void synth_iir3_init_hipass(struct synth_iir3 *iir3,float thresh);
void synth_iir3_init_bpass(struct synth_iir3 *iir3,float mid,float width);
void synth_iir3_init_notch(struct synth_iir3 *iir3,float mid,float width);

static inline float synth_iir3_update(struct synth_iir3 *iir3,float src) {
  iir3->v[2]=iir3->v[1];
  iir3->v[1]=iir3->v[0];
  iir3->v[0]=src;
  src=
    iir3->v[0]*iir3->cv[0]+
    iir3->v[1]*iir3->cv[1]+
    iir3->v[2]*iir3->cv[2]+
    iir3->v[3]*iir3->cv[3]+
    iir3->v[4]*iir3->cv[4];
  iir3->v[4]=iir3->v[3];
  iir3->v[3]=src;
  return src;
}

#endif
