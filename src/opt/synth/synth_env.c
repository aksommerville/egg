#include "synth_internal.h"

/* Decode.
 */

int synth_env_config_decode(struct synth_env_config *config,const void *src,int srcc,int rate) {
  if (!config||!src||(srcc<1)) return -1;
  memset(config,0,sizeof(struct synth_env_config));
  const uint8_t *SRC=src;
  int srcp=0;
  double framesperms=(double)rate/1000.0;
  
  #define RDV(fld) { \
    if (srcp>srcc-2) return -1; \
    (fld)=((SRC[srcp]<<8)|SRC[srcp+1])/65536.0f; \
    srcp+=2; \
  }
  #define RDT(fld) { \
    if (srcp>=srcc) return -1; \
    int _ms=SRC[srcp++]; \
    if (_ms&0x80) { \
      _ms&=0x7f; \
      _ms<<=7; \
      if (srcp>=srcc) return -1; \
      if (SRC[srcp]&0x80) return -1; \
      _ms|=SRC[srcp++]; \
    } \
    if (((fld)=lround((double)_ms*framesperms))<1) (fld)=1; \
  }
  
  /* Starts with flags. Assert no reserved ones.
   */
  config->flags=SRC[srcp++];
  if (config->flags&SYNTH_ENV_FLAG_RESERVED) return -1;
  
  /* Optional initial values.
   */
  if (config->flags&SYNTH_ENV_FLAG_INIT) {
    RDV(config->inivlo)
    if (config->flags&SYNTH_ENV_FLAG_VELOCITY) {
      RDV(config->inivhi)
    }
  }
  
  /* Optional sustain index.
   */
  if (config->flags&SYNTH_ENV_FLAG_SUSTAIN) {
    if (srcp>=srcc) return -1;
    config->susp=SRC[srcp++];
  } else {
    config->susp=-1;
  }
  
  /* Points.
   */
  if (srcp>=srcc) return -1;
  int pointc=SRC[srcp++];
  if (pointc>SYNTH_ENV_POINT_LIMIT) return -1;
  while (pointc-->0) {
    struct synth_env_config_point *point=config->pointv+config->pointc++;
    RDT(point->tlo)
    RDV(point->vlo)
    if (config->flags&SYNTH_ENV_FLAG_VELOCITY) {
      RDT(point->thi)
      RDV(point->vhi)
    }
  }
  
  /* Force no sustain if index invalid.
   */
  if (config->susp>=config->pointc) {
    config->susp=-1;
  }
  
  #undef RDV
  #undef RDT
  return srcp;
}

/* Adjust config values.
 */

void synth_env_config_bias(struct synth_env_config *config,float d) {
  struct synth_env_config_point *point=config->pointv;
  int i=config->pointc;
  for (;i-->0;point++) {
    point->vlo+=d;
    point->vhi+=d;
  }
}

void synth_env_config_scale(struct synth_env_config *config,float d) {
  struct synth_env_config_point *point=config->pointv;
  int i=config->pointc;
  for (;i-->0;point++) {
    point->vlo*=d;
    point->vhi*=d;
  }
}

/* Init runner.
 */

void synth_env_init(struct synth_env_runner *runner,const struct synth_env_config *config,float velocity,int durframes) {
  runner->susp=-1;
  runner->finished=0;

  /* Apply velocity and capture all points in the config shape.
   */
  const struct synth_env_config_point *src=config->pointv;
  struct synth_env_runner_point *dst=runner->pointv;
  int i=config->pointc;
  if ((velocity<=0.0f)||!(config->flags&SYNTH_ENV_FLAG_VELOCITY)) {
    runner->v=config->inivlo;
    for (;i-->0;dst++,src++) {
      dst->c=src->tlo;
      dst->v=src->vlo;
    }
  } else if (velocity>=1.0f) {
    runner->v=config->inivhi;
    for (;i-->0;dst++,src++) {
      dst->c=src->thi;
      dst->v=src->vhi;
    }
  } else {
    float aweight=1.0f-velocity;
    runner->v=config->inivlo*aweight+config->inivhi*velocity;
    for (;i-->0;dst++,src++) {
      dst->c=(int)(src->tlo*aweight+src->thi*velocity);
      dst->v=src->vlo*aweight+src->vhi*velocity;
    }
  }
  runner->pointc=config->pointc;
  for (i=config->pointc,dst=runner->pointv;i-->0;dst++) {
    if (dst->c<1) dst->c=1;
  }
  
  /* If sustain in play, add a sustain-end point of the appropriate length.
   */
  if ((config->flags&SYNTH_ENV_FLAG_SUSTAIN)&&(config->susp>=0)&&(config->susp<config->pointc)&&(config->pointc<SYNTH_ENV_POINT_LIMIT)) {
    int addc=durframes;
    for (i=0;i<config->susp;i++) addc-=runner->pointv[i].c;
    if (addc>0) {
      dst=runner->pointv+config->susp;
      memmove(dst+1,dst,sizeof(struct synth_env_runner_point)*(runner->pointc-config->susp));
      runner->pointc++;
      dst[1].c=addc;
      runner->susp=config->susp+1;
    }
  }
  
  /* Prep the initial leg.
   */
  runner->pointp=0;
  if (runner->pointp<runner->pointc) {
    runner->dv=(runner->pointv[0].v-runner->v)/runner->pointv[0].c;
    runner->c=runner->pointv[0].c;
  } else {
    runner->dv=0.0f;
    runner->c=INT_MAX;
    runner->finished=1;
  }
}

/* Release runner.
 */

void synth_env_release(struct synth_env_runner *runner) {
  if ((runner->susp<0)||(runner->susp>=runner->pointc)) return;
  if (runner->pointp>runner->susp) {
  } else if (runner->pointp==runner->susp) {
    runner->c=0;
  } else {
    runner->pointv[runner->susp].c=1;
  }
  runner->susp=-1;
}

/* Advance runner.
 */
 
void synth_env_advance(struct synth_env_runner *runner) {
  runner->pointp++;
  if (runner->pointp>=runner->pointc) {
    if (runner->pointc>=1) {
      runner->v=runner->pointv[runner->pointc-1].v;
    }
    runner->dv=0.0f;
    runner->c=INT_MAX;
    runner->finished=1;
  } else {
    struct synth_env_runner_point *point=runner->pointv+runner->pointp;
    runner->v=point[-1].v;
    runner->c=point->c;
    runner->dv=(point->v-runner->v)/runner->c;
  }
}
