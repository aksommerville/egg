#include "../synth_internal.h"

/* Instance definition.
 */
 
struct synth_node_gain {
  struct synth_node hdr;
  float gain,clip,gate;
};

#define NODE ((struct synth_node_gain*)node)

/* Cleanup.
 */
 
static void _gain_del(struct synth_node *node) {
}

/* Update.
 */
 
static void _gain_update(float *v,int framec,struct synth_node *node) {
  float nclip=-NODE->clip;
  framec*=node->chanc; // We are a rare case of pure LTI filters. We don't care whether it's stereo or mono.
  if (NODE->gate>0.0f) {
    float ngate=-NODE->gate;
    for (;framec-->0;v++) {
      (*v)*=NODE->gain;
      if (*v>NODE->clip) *v=NODE->clip;
      else if (*v<nclip) *v=nclip;
      if ((*v<NODE->gate)&&(*v>ngate)) *v=0.0f;
    }
  } else {
    for (;framec-->0;v++) {
      (*v)*=NODE->gain;
      if (*v>NODE->clip) *v=NODE->clip;
      else if (*v<nclip) *v=nclip;
    }
  }
}

/* Init.
 */
 
static int _gain_init(struct synth_node *node) {
  node->update=_gain_update;
  NODE->gain=1.0f;
  NODE->clip=1.0f;
  NODE->gate=0.0f;
  return 0;
}

/* Ready.
 */
 
static int _gain_ready(struct synth_node *node) {
  return 0;
}

/* Type definition.
 */
 
const struct synth_node_type synth_node_type_gain={
  .name="gain",
  .objlen=sizeof(struct synth_node_gain),
  .del=_gain_del,
  .init=_gain_init,
  .ready=_gain_ready,
};

/* Setup.
 */
 
int synth_node_gain_setup(struct synth_node *node,const uint8_t *arg,int argc) {
  if (!node||(node->type!=&synth_node_type_gain)||node->ready) return -1;
  if (argc<2) return 0;
  NODE->gain=(float)((arg[0]<<8)|arg[1])/256.0f;
  if (argc<3) return 0;
  NODE->clip=(float)arg[2]/255.0f;
  if (argc<4) return 0;
  NODE->gate=(float)arg[3]/255.0f;
  return 0;
}
