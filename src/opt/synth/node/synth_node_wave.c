#include "../synth_internal.h"

/* Instance definition.
 */
 
struct synth_node_wave {
  struct synth_node hdr;
  uint32_t p;
  uint32_t dp;
  float baserate;
  float velocity;
  int durframes;
  float mixl,mixr;
  struct synth_env_runner level;
  const struct synth_wave *wave; // WEAK
  struct synth_env_runner pitchenv;
  const struct synth_env *pitchenv_source;
  const float *pitchlfo;
};

#define NODE ((struct synth_node_wave*)node)

/* Cleanup.
 */
 
static void _wave_del(struct synth_node *node) {
}

/* Update.
 */
 
static void _wave_update_stereo(float *v,int framec,struct synth_node *node) {
  const float *src=NODE->wave->v;
  for (;framec-->0;v+=2) {
    float sample=src[NODE->p>>SYNTH_WAVE_SHIFT];
    sample*=synth_env_runner_next(NODE->level);
    v[0]+=sample*NODE->mixl;
    v[1]+=sample*NODE->mixr;
    NODE->p+=NODE->dp;
  }
  if (synth_env_runner_is_finished(&NODE->level)) node->finished=1;
}

static void _wave_update_mono(float *v,int framec,struct synth_node *node) {
  const float *src=NODE->wave->v;
  for (;framec-->0;v++) {
    float sample=src[NODE->p>>SYNTH_WAVE_SHIFT];
    sample*=synth_env_runner_next(NODE->level);
    (*v)+=sample*NODE->mixl;
    NODE->p+=NODE->dp;
  }
  if (synth_env_runner_is_finished(&NODE->level)) node->finished=1;
}

// Mono or stereo, with pitch envelope and LFO.
static void _wave_update_bells_whistles(float *v,int framec,struct synth_node *node) {
  const float *src=NODE->wave->v;
  const float *lfo=NODE->pitchlfo;
  float dpbase=(float)NODE->dp;
  int centslo=0,centshi=0;
  for (;framec-->0;v+=node->chanc) {
    float sample=src[NODE->p>>SYNTH_WAVE_SHIFT];
    sample*=synth_env_runner_next(NODE->level);
    if (node->chanc==1) {
      v[0]+=sample*NODE->mixl;
    } else {
      v[0]+=sample*NODE->mixl;
      v[1]+=sample*NODE->mixr;
    }
    uint32_t dp=NODE->dp;
    if (NODE->pitchenv.env) {
      int cents=(int)synth_env_runner_next(NODE->pitchenv);
      float scale=synth_multiplier_from_cents(cents);
      dp=(uint32_t)(dpbase*scale);
    }
    if (lfo) {
      int cents=(int)(*lfo);
      float scale=synth_multiplier_from_cents(cents);
      dp=(uint32_t)((float)dp*scale);
      lfo++;
    }
    NODE->p+=dp;
  }
  if (synth_env_runner_is_finished(&NODE->level)) node->finished=1;
}

/* Init.
 */
 
static int _wave_init(struct synth_node *node) {
  if (node->chanc==2) node->update=_wave_update_stereo;
  else if (node->chanc==1) node->update=_wave_update_mono;
  else return -1;
  return 0;
}

/* Ready.
 */
 
static int _wave_ready(struct synth_node *node) {
  if (!NODE->wave) return -1;
  
  if (NODE->pitchenv_source||NODE->pitchlfo) {
    synth_env_runner_init(&NODE->pitchenv,NODE->pitchenv_source,NODE->velocity);
    synth_env_runner_release_later(&NODE->pitchenv,NODE->durframes);
    node->update=_wave_update_bells_whistles;
  }
  
  return 0;
}

/* Type definition.
 */
 
const struct synth_node_type synth_node_type_wave={
  .name="wave",
  .objlen=sizeof(struct synth_node_wave),
  .del=_wave_del,
  .init=_wave_init,
  .ready=_wave_ready,
};

/* Setup.
 */
 
int synth_node_wave_setup(
  struct synth_node *node,
  const struct synth_wave *wave,
  float rate,float velocity,int durframes,
  const struct synth_env *env,
  float trim,float pan
) {
  if (!node||(node->type!=&synth_node_type_wave)||node->ready) return -1;
  NODE->baserate=rate;
  NODE->dp=(uint32_t)(rate*4294967296.0f);
  NODE->velocity=velocity;
  NODE->durframes=durframes;
  
  NODE->mixl=NODE->mixr=trim;
  if (node->chanc==2) {
    if (pan<0.0f) NODE->mixr*=pan+1.0f;
    else if (pan>0.0f) NODE->mixl*=1.0f-pan;
  }
  
  synth_env_runner_init(&NODE->level,env,velocity);
  synth_env_runner_release_later(&NODE->level,durframes);
  
  NODE->wave=wave;
  
  //fprintf(stderr,"%s (%f)0x%08x mix=%f,%f\n",__func__,rate,NODE->dp,NODE->mixl,NODE->mixr);
  
  return 0;
}

void synth_node_wave_set_pitch_adjustment(struct synth_node *node,const struct synth_env *env,const float *lfo) {
  if (!node||(node->type!=&synth_node_type_wave)||node->ready) return;
  NODE->pitchenv_source=env;
  NODE->pitchlfo=lfo;
}

/* Adjust rate.
 */

void synth_node_wave_adjust_rate(struct synth_node *node,float multiplier) {
  if (!node||(node->type!=&synth_node_type_wave)) return;
  NODE->dp=(uint32_t)((NODE->baserate*multiplier)*4294967296.0f);
}
