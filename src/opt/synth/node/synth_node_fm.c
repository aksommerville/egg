#include "../synth_internal.h"

#define FM_PSCALE (SYNTH_WAVE_SIZE_SAMPLES/(M_PI*2.0f))
#define FM_WAVE_MASK (SYNTH_WAVE_SIZE_SAMPLES-1)

/* Instance definition.
 */
 
struct synth_node_fm {
  struct synth_node hdr;
  float baserate;
  float carrate;
  float velocity;
  int durframes;
  float mixl,mixr;
  struct synth_env_runner level;
  float carp,modp; // -pi..pi
  const struct synth_wave *carwave; // WEAK
  const struct synth_wave *modwave; // WEAK
  struct synth_env_runner pitchenv;
  const struct synth_env *pitchenv_source;
  const float *pitchlfo;
  float fmrate,fmrange;
  struct synth_env_runner rangeenv;
  const struct synth_env *rangeenv_source;
  const float *rangelfo;
};

#define NODE ((struct synth_node_fm*)node)

/* Cleanup.
 */
 
static void _fm_del(struct synth_node *node) {
}

/* Update.
 * TODO The extra envelopes and LFOs are expensive, and so is checking chanc every frame.
 * Split this into various optimized cases. Current implementation works for all configs.
 */
 
static void _fm_update(float *v,int framec,struct synth_node *node) {
  const float *src=NODE->carwave->v;
  const float *msrc=NODE->modwave->v;
  const float *rlfo=NODE->rangelfo;
  const float *pitchlfo=NODE->pitchlfo;
  for (;framec-->0;) {
    float sample=src[((int)(NODE->carp*FM_PSCALE))&FM_WAVE_MASK];
    float mod=msrc[((int)(NODE->modp*FM_PSCALE))&FM_WAVE_MASK];
    NODE->modp+=NODE->carrate*NODE->fmrate;
    while (NODE->modp>M_PI) NODE->modp-=M_PI*2.0f;
    
    if (NODE->rangeenv_source) {
      mod*=synth_env_runner_next(NODE->rangeenv);
    }
    if (NODE->rangelfo) {
      mod*=(*rlfo);
      rlfo++;
    }
    
    float carrate=NODE->carrate;
    if (NODE->pitchenv.env) {
      int cents=(int)synth_env_runner_next(NODE->pitchenv);
      float scale=synth_multiplier_from_cents(cents);
      carrate*=scale;
    }
    if (pitchlfo) {
      int cents=(int)(*pitchlfo);
      float scale=synth_multiplier_from_cents(cents);
      carrate*=scale;
      pitchlfo++;
    }
    
    NODE->carp+=carrate+carrate*mod*NODE->fmrange;
    while (NODE->carp>M_PI) NODE->carp-=M_PI*2.0f;
    
    sample*=synth_env_runner_next(NODE->level);
    (*v)+=sample*NODE->mixl; v++;
    if (node->chanc==2) {
      (*v)+=sample*NODE->mixr; v++;
    }
  }
  if (synth_env_runner_is_finished(&NODE->level)) node->finished=1;
}

/* Init.
 */
 
static int _fm_init(struct synth_node *node) {
  node->update=_fm_update;
  return 0;
}

/* Ready.
 */
 
static int _fm_ready(struct synth_node *node) {
  if (!NODE->carwave) NODE->carwave=&node->synth->sine;
  if (!NODE->modwave) NODE->modwave=&node->synth->sine;
  synth_env_runner_init(&NODE->rangeenv,NODE->rangeenv_source,NODE->velocity);
  synth_env_runner_release_later(&NODE->rangeenv,NODE->durframes);
  synth_env_runner_init(&NODE->pitchenv,NODE->pitchenv_source,NODE->velocity);
  synth_env_runner_release_later(&NODE->pitchenv,NODE->durframes);
  return 0;
}

/* Type definition.
 */
 
const struct synth_node_type synth_node_type_fm={
  .name="fm",
  .objlen=sizeof(struct synth_node_fm),
  .del=_fm_del,
  .init=_fm_init,
  .ready=_fm_ready,
};

/* Setup.
 */
 
int synth_node_fm_setup(
  struct synth_node *node,
  const struct synth_wave *wave,
  float fmrate,float fmrange,
  float rate,float velocity,int durframes,
  const struct synth_env *env,
  float trim,float pan
) {
  if (!node||(node->type!=&synth_node_type_fm)||node->ready) return -1;
  NODE->baserate=rate*M_PI*2.0f;
  NODE->carrate=NODE->baserate;
  NODE->velocity=velocity;
  NODE->durframes=durframes;
  NODE->fmrate=fmrate;
  NODE->fmrange=fmrange;
  
  NODE->mixl=NODE->mixr=trim;
  if (node->chanc==2) {
    if (pan<0.0f) NODE->mixr*=pan+1.0f;
    else if (pan>0.0f) NODE->mixl*=1.0f-pan;
  }
  
  synth_env_runner_init(&NODE->level,env,velocity);
  synth_env_runner_release_later(&NODE->level,durframes);
  
  NODE->carwave=wave;
  
  return 0;
}

void synth_node_fm_set_pitch_adjustment(struct synth_node *node,const struct synth_env *env,const float *lfo) {
  if (!node||(node->type!=&synth_node_type_fm)||node->ready) return;
  NODE->pitchenv_source=env;
  NODE->pitchlfo=lfo;
}

void synth_node_fm_set_modulation_adjustment(struct synth_node *node,const struct synth_env *env,const float *lfo) {
  if (!node||(node->type!=&synth_node_type_fm)||node->ready) return;
  NODE->rangeenv_source=env;
  NODE->rangelfo=lfo;
}

/* Adjust rate.
 */

void synth_node_fm_adjust_rate(struct synth_node *node,float multiplier) {
  if (!node||(node->type!=&synth_node_type_fm)) return;
  NODE->carrate=NODE->baserate*multiplier;
}
