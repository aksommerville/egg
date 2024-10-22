#include "synth_internal.h"

/* Decode level envelope.
 */
 
int synth_env_decode_level(struct synth_env_config *dst,const void *_src,int srcc) {
  if (!dst||!_src||(srcc<12)) return -1;
  const uint8_t *src=_src;
  dst->inivlo=0.0f;
  dst->inivhi=0.0f;
  dst->atktlo=(float)src[0];
  dst->atkthi=(float)src[1];
  dst->atkvlo=src[2]/255.0f;
  dst->atkvhi=src[3]/255.0f;
  dst->dectlo=(float)src[4];
  dst->decthi=(float)src[5];
  dst->susvlo=src[6]/255.0f;
  dst->susvhi=src[7]/255.0f;
  dst->rlstlo=(float)((src[8]<<8)|src[9]);
  dst->rlsthi=(float)((src[10]<<8)|src[11]);
  dst->rlsvlo=0.0f;
  dst->rlsvhi=0.0f;
  return 12;
}

/* Decode auxiliary envelope.
 */

int synth_env_decode_aux(
  struct synth_env_config *dst,
  const struct synth_env_config *ref,
  const void *_src,int srcc,
  int noop
) {
  if (!dst||!ref||!_src||(srcc<16)) return -1;
  const uint8_t *src=_src;
  dst->noop=0;
  if ((noop>=0)&&(noop<=0xffff)) {
    const uint8_t q[2]={noop>>8,noop};
    dst->noop=1;
    int i=0;
    for (;i<16;i+=2) if (memcmp(src+i,q,2)) { dst->noop=0; break; }
  }
  dst->inivlo=((src[0]<<8)|src[1])/65535.0f;
  dst->inivhi=((src[2]<<8)|src[3])/65535.0f;
  dst->atktlo=ref->atktlo;
  dst->atkthi=ref->atkthi;
  dst->atkvlo=((src[4]<<8)|src[5])/65535.0f;
  dst->atkvhi=((src[6]<<8)|src[7])/65535.0f;
  dst->dectlo=ref->dectlo;
  dst->decthi=ref->decthi;
  dst->susvlo=((src[8]<<8)|src[9])/65535.0f;
  dst->susvhi=((src[10]<<8)|src[11])/65535.0f;
  dst->rlstlo=ref->rlstlo;
  dst->rlsthi=ref->rlsthi;
  dst->rlsvlo=((src[12]<<8)|src[13])/65535.0f;
  dst->rlsvhi=((src[14]<<8)|src[15])/65535.0f;
  return 16;
}

int synth_env_decode_aux_u16(
  struct synth_env_config *dst,
  const struct synth_env_config *ref,
  const void *_src,int srcc,
  int noop
) {
  if (!dst||!ref||!_src||(srcc<16)) return -1;
  const uint8_t *src=_src;
  dst->noop=0;
  if ((noop>=0)&&(noop<=0xffff)) {
    const uint8_t q[2]={noop>>8,noop};
    dst->noop=1;
    int i=0;
    for (;i<16;i+=2) if (memcmp(src+i,q,2)) { dst->noop=0; break; }
  }
  dst->inivlo=(float)((src[0]<<8)|src[1]);
  dst->inivhi=(float)((src[2]<<8)|src[3]);
  dst->atktlo=ref->atktlo;
  dst->atkthi=ref->atkthi;
  dst->atkvlo=(float)((src[4]<<8)|src[5]);
  dst->atkvhi=(float)((src[6]<<8)|src[7]);
  dst->dectlo=ref->dectlo;
  dst->decthi=ref->decthi;
  dst->susvlo=(float)((src[8]<<8)|src[9]);
  dst->susvhi=(float)((src[10]<<8)|src[11]);
  dst->rlstlo=ref->rlstlo;
  dst->rlsthi=ref->rlsthi;
  dst->rlsvlo=(float)((src[12]<<8)|src[13]);
  dst->rlsvhi=(float)((src[14]<<8)|src[15]);
  return 16;
}

/* Adjust config values.
 */
 
void synth_env_config_scale(struct synth_env_config *dst,float scale) {
  dst->inivlo*=scale;
  dst->inivhi*=scale;
  dst->atkvlo*=scale;
  dst->atkvhi*=scale;
  dst->susvlo*=scale;
  dst->susvhi*=scale;
  dst->rlsvlo*=scale;
  dst->rlsvhi*=scale;
}

void synth_env_config_bias(struct synth_env_config *dst,float bias) {
  dst->inivlo+=bias;
  dst->inivhi+=bias;
  dst->atkvlo+=bias;
  dst->atkvhi+=bias;
  dst->susvlo+=bias;
  dst->susvhi+=bias;
  dst->rlsvlo+=bias;
  dst->rlsvhi+=bias;
}

/* Instantiate runner.
 */

void synth_env_run(struct synth_env_runner *runner,const struct synth_env_config *config,float velocity,int durms,int rate) {
  if (!runner||!config||(rate<1)) return;
  float framesperms=(float)rate/1000.0f;
  
  if (velocity<=0.0f) {
    runner->iniv=config->inivlo;
    runner->atkv=config->atkvlo;
    runner->susv=config->susvlo;
    runner->rlsv=config->rlsvlo;
    if ((runner->atkt=(int)(config->atktlo*framesperms))<1) runner->atkt=1;
    if ((runner->dect=(int)(config->dectlo*framesperms))<1) runner->dect=1;
    if ((runner->rlst=(int)(config->rlstlo*framesperms))<1) runner->rlst=1;
  } else if (velocity>=1.0f) {
    runner->iniv=config->inivhi;
    runner->atkv=config->atkvhi;
    runner->susv=config->susvhi;
    runner->rlsv=config->rlsvhi;
    if ((runner->atkt=(int)(config->atkthi*framesperms))<1) runner->atkt=1;
    if ((runner->dect=(int)(config->decthi*framesperms))<1) runner->dect=1;
    if ((runner->rlst=(int)(config->rlsthi*framesperms))<1) runner->rlst=1;
  } else {
    float loweight=1.0f-velocity;
    runner->iniv=config->inivlo*loweight+config->inivhi*velocity;
    runner->atkv=config->atkvlo*loweight+config->atkvhi*velocity;
    runner->susv=config->susvlo*loweight+config->susvhi*velocity;
    runner->rlsv=config->rlsvlo*loweight+config->rlsvhi*velocity;
    if ((runner->atkt=(int)((config->atktlo*loweight+config->atkthi*velocity)*framesperms))<1) runner->atkt=1;
    if ((runner->dect=(int)((config->dectlo*loweight+config->decthi*velocity)*framesperms))<1) runner->dect=1;
    if ((runner->rlst=(int)((config->rlstlo*loweight+config->rlsthi*velocity)*framesperms))<1) runner->rlst=1;
  }
  
  // Sustain time is a little different, and it's not sensitive to velocity.
  int durframes=(int)((float)durms*framesperms);
  if ((runner->sust=durframes-runner->dect-runner->atkt)<1) runner->sust=1;
  
  // Trick synth_env_advance() into setting up (v,c,dv) for us, so those all live in the same place.
  runner->p=-1;
  synth_env_advance(runner);
}

/* Early release.
 */

void synth_env_release(struct synth_env_runner *runner) {
  if (runner->p<2) {
    runner->sust=1;
  } else if (runner->p==2) {
    runner->c=0;
  }
}

/* Advance to next leg.
 */

void synth_env_advance(struct synth_env_runner *runner) {
  runner->p++;
  switch (runner->p) {
    case 0: { // Begin attack.
        runner->v=runner->iniv;
        runner->c=runner->atkt;
        runner->dv=(runner->atkv-runner->v)/(float)runner->c;
      } break;
    case 1: { // Enter decay.
        runner->v=runner->atkv;
        runner->c=runner->dect;
        runner->dv=(runner->susv-runner->v)/(float)runner->c;
      } break;
    case 2: { // Enter sustain.
        runner->v=runner->susv;
        runner->c=runner->sust;
        runner->dv=0.0f;
      } break;
    case 3: { // Enter release.
        runner->v=runner->susv;
        runner->c=runner->rlst;
        runner->dv=(runner->rlsv-runner->v)/(float)runner->c;
      } break;
    default: { // Complete.
        runner->p=4;
        runner->v=runner->rlsv;
        runner->dv=0.0f;
        runner->c=INT_MAX;
      }
  }
}
