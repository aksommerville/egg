#include "../synth_internal.h"

/* Instance definition.
 */
 
struct synth_node_tremolo {
  struct synth_node hdr;
  struct synth_osc osc;
  float bias;
};

#define NODE ((struct synth_node_tremolo*)node)

/* Cleanup.
 */
 
static void _tremolo_del(struct synth_node *node) {
}

/* Update.
 */
 
static void _tremolo_update_stereo(float *v,int framec,struct synth_node *node) {
  for (;framec-->0;v+=2) {
    float trim=synth_osc_next(&NODE->osc)+NODE->bias;
    v[0]*=trim;
    v[1]*=trim;
  }
}

static void _tremolo_update_mono(float *v,int framec,struct synth_node *node) {
  for (;framec-->0;v+=1) {
    float trim=synth_osc_next(&NODE->osc)+NODE->bias;
    v[0]*=trim;
  }
}

/* Init.
 */
 
static int _tremolo_init(struct synth_node *node) {
  if (node->chanc==2) node->update=_tremolo_update_stereo;
  else node->update=_tremolo_update_mono;
  return 0;
}

/* Ready.
 */
 
static int _tremolo_ready(struct synth_node *node) {
  return 0;
}

/* Type definition.
 */
 
const struct synth_node_type synth_node_type_tremolo={
  .name="tremolo",
  .objlen=sizeof(struct synth_node_tremolo),
  .del=_tremolo_del,
  .init=_tremolo_init,
  .ready=_tremolo_ready,
};

/* Setup.
 */
 
int synth_node_tremolo_setup(struct synth_node *node,const uint8_t *arg,int argc) {
  if (!node||(node->type!=&synth_node_type_tremolo)||node->ready) return -1;
  if (synth_osc_decode(&NODE->osc,node->synth,arg,argc)<0) return -1;
  NODE->osc.scale*=0.5;
  NODE->bias=1.0f-NODE->osc.scale;
  return 0;
}
