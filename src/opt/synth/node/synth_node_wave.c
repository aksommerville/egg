#include "../synth_internal.h"

/* Instance definition.
 */
 
struct synth_node_wave {
  struct synth_node hdr;
  const float *wave; // WEAK, owned by our channel.
  uint32_t p;
  uint32_t dp;
  float triml,trimr;
  struct synth_env_runner level;
  struct synth_env_runner pitchenv;
};

#define NODE ((struct synth_node_wave*)node)

/* Cleanup.
 */
 
static void _wave_del(struct synth_node *node) {
}

/* Init.
 */
 
static int _wave_init(struct synth_node *node) {
  return 0;
}

/* Update.
 */
 
static void _wave_update_mono_pitchenv(float *v,int framec,struct synth_node *node) {
  double dpbase=(double)NODE->dp;
  for (;framec--;v+=1) {
    float sample=NODE->wave[NODE->p>>SYNTH_WAVE_SHIFT];
    NODE->p+=(uint32_t)(dpbase*powf(2.0f,synth_env_update(&NODE->pitchenv)));
    sample*=synth_env_update(&NODE->level);
    v[0]+=sample*NODE->triml;
  }
  if (synth_env_is_finished(&NODE->level)) node->finished=1;
}
 
static void _wave_update_stereo_pitchenv(float *v,int framec,struct synth_node *node) {
  double dpbase=(double)NODE->dp;
  for (;framec--;v+=2) {
    float sample=NODE->wave[NODE->p>>SYNTH_WAVE_SHIFT];
    NODE->p+=(uint32_t)(dpbase*powf(2.0f,synth_env_update(&NODE->pitchenv)));
    sample*=synth_env_update(&NODE->level);
    v[0]+=sample*NODE->triml;
    v[1]+=sample*NODE->trimr;
  }
  if (synth_env_is_finished(&NODE->level)) node->finished=1;
}
 
static void _wave_update_mono_flatrate(float *v,int framec,struct synth_node *node) {
  for (;framec--;v+=1) {
    float sample=NODE->wave[NODE->p>>SYNTH_WAVE_SHIFT];
    NODE->p+=NODE->dp;
    sample*=synth_env_update(&NODE->level);
    v[0]+=sample*NODE->triml;
  }
  if (synth_env_is_finished(&NODE->level)) node->finished=1;
}
 
static void _wave_update_stereo_flatrate(float *v,int framec,struct synth_node *node) {
  float hi=NODE->level.v;
  float lo=hi;
  for (;framec--;v+=2) {
    float sample=NODE->wave[NODE->p>>SYNTH_WAVE_SHIFT];
    NODE->p+=NODE->dp;
    float e=synth_env_update(&NODE->level);
    if (e<lo) lo=e; else if (e>hi) hi=e;
    sample*=e;
    v[0]+=sample*NODE->triml;
    v[1]+=sample*NODE->trimr;
  }
  if (synth_env_is_finished(&NODE->level)) node->finished=1;
}

/* Ready.
 */
 
static int _wave_ready(struct synth_node *node) {
  if (!NODE->wave) return -1;
  if (!synth_env_is_ready(&NODE->level)) return -1;
  if (NODE->pitchenv.atkt) {
    if (!synth_env_is_ready(&NODE->pitchenv)) return -1;
    if (node->chanc==1) node->update=_wave_update_mono_pitchenv;
    else node->update=_wave_update_stereo_pitchenv;
  } else {
    if (node->chanc==1) node->update=_wave_update_mono_flatrate;
    else node->update=_wave_update_stereo_flatrate;
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

/* Trivial accessors.
 */
 
struct synth_env_runner *synth_node_wave_get_levelenv(struct synth_node *node) {
  if (!node||(node->type!=&synth_node_type_wave)) return 0;
  return &NODE->level;
}

struct synth_env_runner *synth_node_wave_get_pitchenv(struct synth_node *node) {
  if (!node||(node->type!=&synth_node_type_wave)) return 0;
  return &NODE->pitchenv;
}

/* Configure.
 */
 
int synth_node_wave_configure(struct synth_node *node,const float *wave,float freq,float triml,float trimr) {
  if (!node||(node->type!=&synth_node_type_wave)||node->ready||!wave) return -1;
  NODE->wave=wave;
  NODE->dp=(uint32_t)(freq*4294967295.0f);
  NODE->triml=triml;
  NODE->trimr=trimr;
  return 0;
}
