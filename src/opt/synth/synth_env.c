#include "synth_env.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdint.h>

/* Decode new envelope.
 * u8 sustain index, OOB for none
 * u16 init lo
 * u16 init hi
 * repeat:
 *   u16 delay lo
 *   u16 delay hi
 *   u16 level lo
 *   u16 level hi
 */
 
struct synth_env *synth_env_decode(const void *src,int srcc,int rate) {
  const uint8_t *SRC=src;
  if (!src||(srcc<5)||(rate<1)) return 0;
  int pointc=(srcc-5)/8; // Zero is legal.
  if (pointc>255) return 0; // No particular reason for this limit, just a sanity check.
  struct synth_env *env=calloc(1,sizeof(struct synth_env)+sizeof(struct synth_env_point)*pointc);
  if (!env) return 0;
  float frate=(float)rate;
  
  #define RDT(p) (((float)((SRC[p]<<8)|SRC[p+1])*frate)/1000.0f)
  #define RDV(p) ((float)((SRC[p]<<8)|SRC[p+1])/65535.0f)
  
  if (SRC[0]<pointc) env->susp=SRC[0];
  else env->susp=-1;
  env->initlo=RDV(1);
  env->inithi=RDV(3);
  env->pointc=pointc;
  
  struct synth_env_point *point=env->pointv;
  SRC+=5;
  int i=pointc;
  for (;i-->0;point++,SRC+=8) {
    point->tlo=RDT(0);
    point->thi=RDT(2);
    point->vlo=RDV(4);
    point->vhi=RDV(6);
  }
  
  #undef RDT
  #undef RDV
  return env;
}

/* Scale values.
 */
 
void synth_env_adjust_values(struct synth_env *env,float bias,float scale) {
  env->initlo=(env->initlo+bias)*scale;
  env->inithi=(env->inithi+bias)*scale;
  struct synth_env_point *point=env->pointv;
  int i=env->pointc;
  for (;i-->0;point++) {
    point->vlo=(point->vlo+bias)*scale;
    point->vhi=(point->vhi+bias)*scale;
  }
}

/* Initialize runner.
 */

void synth_env_runner_init(struct synth_env_runner *runner,const struct synth_env *env,float velocity) {
  if (runner->env=env) {
    if (velocity<=0.0f) {
      runner->loweight=1.0f;
      runner->hiweight=0.0f;
      runner->level=env->initlo;
    } else if (velocity>=1.0f) {
      runner->loweight=0.0f;
      runner->hiweight=1.0f;
      runner->level=env->inithi;
    } else {
      runner->loweight=velocity;
      runner->hiweight=1.0f-velocity;
      runner->level=runner->loweight*env->initlo+runner->hiweight*env->inithi;
    }
    runner->released=0;
    runner->pointp=0;
    if (env->pointc>0) {
      const struct synth_env_point *point=runner->env->pointv;
      runner->target=point->vlo*runner->loweight+point->vhi*runner->hiweight;
      runner->c=(int)(point->tlo*runner->loweight+point->thi*runner->hiweight);
      if (runner->c<1) runner->c=1;
      runner->d=(runner->target-runner->level)/(float)runner->c;
      runner->hold=0;
    } else {
      runner->target=runner->level;
      runner->c=INT_MAX;
      runner->d=0.0f;
      runner->hold=1;
    }
  } else {
    // Null env is illegal and we will segfault if this counter runs out.
    // But we're not able to fail here. And this setup actually will behave sanely in every reasonable scenario.
    runner->level=0.0f;
    runner->hold=1;
    runner->d=0.0f;
    runner->c=INT_MAX;
    runner->pointp=0;
    runner->target=0.0f;
    runner->released=1;
    runner->loweight=runner->hiweight=0.0f;
  }
}

/* Release.
 */

void synth_env_runner_release(struct synth_env_runner *runner) {
  if (runner->released) return;
  runner->released=1;
  if (runner->pointp==runner->env->susp) runner->hold=0;
}

void synth_env_runner_release_later(struct synth_env_runner *runner,int framec) {
  if (runner->released) return;
  if (framec>0) runner->deferrelease=framec;
  else synth_env_runner_release(runner);
}

/* Advance to next point.
 */

void synth_env_runner_advance(struct synth_env_runner *runner) {
  runner->level=runner->target;
  runner->pointp++;
  if (runner->pointp<runner->env->pointc) {
    const struct synth_env_point *point=runner->env->pointv+runner->pointp;
    runner->target=point->vlo*runner->loweight+point->vhi*runner->hiweight;
    runner->c=(int)(point->tlo*runner->loweight+point->thi*runner->hiweight);
    if (runner->c<1) runner->c=1;
    runner->d=(runner->target-runner->level)/(float)runner->c;
    if ((runner->pointp==runner->env->susp)&&!runner->released) {
      runner->hold=1;
    }
  } else {
    runner->pointp=runner->env->pointc;
    runner->d=0.0f;
    runner->c=INT_MAX;
    runner->hold=1;
  }
}
