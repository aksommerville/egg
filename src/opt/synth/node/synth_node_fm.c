#include "../synth_internal.h"

/* Instance definition.
 */
 
struct synth_node_fm {
  struct synth_node hdr;
  float carp;
  float dpbase; // delta radians
  float modp;
  float modrate;
  float range;
  float triml,trimr;
  struct synth_env_runner level;
  struct synth_env_runner pitchenv;
  struct synth_env_runner rangeenv;
};

#define NODE ((struct synth_node_fm*)node)

/* Cleanup.
 */
 
static void _fm_del(struct synth_node *node) {
}

/* Init.
 */
 
static int _fm_init(struct synth_node *node) {
  return 0;
}

/* Update.
 * Hooks are named "[ms][p][r]" for the 3 switches, which turn off for optimization:
 */
 
static void _fm_update_mpr(float *v,int framec,struct synth_node *node) {
  for (;framec--;v+=1) {
    float cardp=NODE->dpbase;
    cardp*=powf(2.0f,synth_env_update(&NODE->pitchenv));
    
    float sample=sinf(NODE->carp);
    float mod=sinf(NODE->modp);
    NODE->modp+=cardp*NODE->modrate;
    if (NODE->modp>=M_PI) NODE->modp-=M_PI*2.0f;
    mod*=synth_env_update(&NODE->rangeenv);
    NODE->carp+=cardp+cardp*mod;
    
    sample*=synth_env_update(&NODE->level);
    v[0]+=sample*NODE->triml;
  }
  if (synth_env_is_finished(&NODE->level)) node->finished=1;
}
 
static void _fm_update_spr(float *v,int framec,struct synth_node *node) {
  for (;framec--;v+=2) {
    float cardp=NODE->dpbase;
    cardp*=powf(2.0f,synth_env_update(&NODE->pitchenv));
    
    float sample=sinf(NODE->carp);
    float mod=sinf(NODE->modp);
    NODE->modp+=cardp*NODE->modrate;
    if (NODE->modp>=M_PI) NODE->modp-=M_PI*2.0f;
    mod*=synth_env_update(&NODE->rangeenv);
    NODE->carp+=cardp+cardp*mod;
    
    sample*=synth_env_update(&NODE->level);
    v[0]+=sample*NODE->triml;
    v[1]+=sample*NODE->trimr;
  }
  if (synth_env_is_finished(&NODE->level)) node->finished=1;
}
 
static void _fm_update_mp(float *v,int framec,struct synth_node *node) {
  for (;framec--;v+=1) {
    float cardp=NODE->dpbase;
    cardp*=powf(2.0f,synth_env_update(&NODE->pitchenv));
    
    float sample=sinf(NODE->carp);
    float mod=sinf(NODE->modp);
    NODE->modp+=cardp*NODE->modrate;
    if (NODE->modp>=M_PI) NODE->modp-=M_PI*2.0f;
    mod*=NODE->range;
    NODE->carp+=cardp+cardp*mod;
    
    sample*=synth_env_update(&NODE->level);
    v[0]+=sample*NODE->triml;
  }
  if (synth_env_is_finished(&NODE->level)) node->finished=1;
}
 
static void _fm_update_sp(float *v,int framec,struct synth_node *node) {
  for (;framec--;v+=2) {
    float cardp=NODE->dpbase;
    cardp*=powf(2.0f,synth_env_update(&NODE->pitchenv));
    
    float sample=sinf(NODE->carp);
    float mod=sinf(NODE->modp);
    NODE->modp+=cardp*NODE->modrate;
    if (NODE->modp>=M_PI) NODE->modp-=M_PI*2.0f;
    mod*=NODE->range;
    NODE->carp+=cardp+cardp*mod;
    
    sample*=synth_env_update(&NODE->level);
    v[0]+=sample*NODE->triml;
    v[1]+=sample*NODE->trimr;
  }
  if (synth_env_is_finished(&NODE->level)) node->finished=1;
}
 
static void _fm_update_mr(float *v,int framec,struct synth_node *node) {
  for (;framec--;v+=1) {
    float cardp=NODE->dpbase;
    
    float sample=sinf(NODE->carp);
    float mod=sinf(NODE->modp);
    NODE->modp+=cardp*NODE->modrate;
    if (NODE->modp>=M_PI) NODE->modp-=M_PI*2.0f;
    mod*=synth_env_update(&NODE->rangeenv);
    NODE->carp+=cardp+cardp*mod;
    
    sample*=synth_env_update(&NODE->level);
    v[0]+=sample*NODE->triml;
  }
  if (synth_env_is_finished(&NODE->level)) node->finished=1;
}
 
static void _fm_update_sr(float *v,int framec,struct synth_node *node) {
  for (;framec--;v+=2) {
    float cardp=NODE->dpbase;
    
    float sample=sinf(NODE->carp);
    float mod=sinf(NODE->modp);
    NODE->modp+=cardp*NODE->modrate;
    if (NODE->modp>=M_PI) NODE->modp-=M_PI*2.0f;
    mod*=synth_env_update(&NODE->rangeenv);
    NODE->carp+=cardp+cardp*mod;
    
    sample*=synth_env_update(&NODE->level);
    v[0]+=sample*NODE->triml;
    v[1]+=sample*NODE->trimr;
  }
  if (synth_env_is_finished(&NODE->level)) node->finished=1;
}
 
static void _fm_update_m(float *v,int framec,struct synth_node *node) {
  for (;framec--;v+=1) {
    float cardp=NODE->dpbase;
    
    float sample=sinf(NODE->carp);
    float mod=sinf(NODE->modp);
    NODE->modp+=cardp*NODE->modrate;
    if (NODE->modp>=M_PI) NODE->modp-=M_PI*2.0f;
    mod*=NODE->range;
    NODE->carp+=cardp+cardp*mod;
    
    sample*=synth_env_update(&NODE->level);
    v[0]+=sample*NODE->triml;
  }
  if (synth_env_is_finished(&NODE->level)) node->finished=1;
}
 
static void _fm_update_s(float *v,int framec,struct synth_node *node) {
  for (;framec--;v+=2) {
    float cardp=NODE->dpbase;
    
    float sample=sinf(NODE->carp);
    float mod=sinf(NODE->modp);
    NODE->modp+=cardp*NODE->modrate;
    if (NODE->modp>=M_PI) NODE->modp-=M_PI*2.0f;
    mod*=NODE->range;
    NODE->carp+=cardp+cardp*mod;
    
    sample*=synth_env_update(&NODE->level);
    v[0]+=sample*NODE->triml;
    v[1]+=sample*NODE->trimr;
  }
  if (synth_env_is_finished(&NODE->level)) node->finished=1;
}

/* Ready.
 */
 
static int _fm_ready(struct synth_node *node) {
  if (!synth_env_is_ready(&NODE->level)) return -1;
  int stereo=(node->chanc==2),pitch=0,range=0;
  if (NODE->pitchenv.atkt) {
    if (!synth_env_is_ready(&NODE->pitchenv)) return -1;
    pitch=1;
  }
  if (NODE->rangeenv.atkt) {
    if (!synth_env_is_ready(&NODE->rangeenv)) return -1;
    range=1;
  }
  switch ((stereo?0x100:0)|(pitch?0x010:0)|(range?0x001:0)) {
    case 0x000: node->update=_fm_update_m; break;
    case 0x001: node->update=_fm_update_mr; break;
    case 0x010: node->update=_fm_update_mp; break;
    case 0x011: node->update=_fm_update_mpr; break;
    case 0x100: node->update=_fm_update_s; break;
    case 0x101: node->update=_fm_update_sr; break;
    case 0x110: node->update=_fm_update_sp; break;
    case 0x111: node->update=_fm_update_spr; break;
  }
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

/* Trivial accessors.
 */
 
struct synth_env_runner *synth_node_fm_get_levelenv(struct synth_node *node) {
  if (!node||(node->type!=&synth_node_type_fm)) return 0;
  return &NODE->level;
}

struct synth_env_runner *synth_node_fm_get_pitchenv(struct synth_node *node) {
  if (!node||(node->type!=&synth_node_type_fm)) return 0;
  return &NODE->pitchenv;
}

struct synth_env_runner *synth_node_fm_get_rangeenv(struct synth_node *node) {
  if (!node||(node->type!=&synth_node_type_fm)) return 0;
  return &NODE->rangeenv;
}

/* Configure.
 */
 
int synth_node_fm_configure(struct synth_node *node,float modrate,float range,float freq,float triml,float trimr) {
  if (!node||(node->type!=&synth_node_type_fm)||node->ready) return -1;
  NODE->modrate=modrate;
  NODE->range=range;
  NODE->dpbase=freq*M_PI*2.0f;
  NODE->triml=triml;
  NODE->trimr=trimr;
  return 0;
}
