#include "../synth_internal.h"

/* Instance definition.
 */
 
struct synth_node_detune {
  struct synth_node hdr;
};

#define NODE ((struct synth_node_detune*)node)

/* Cleanup.
 */
 
static void _detune_del(struct synth_node *node) {
}

/* Update.
 */
 
static void _detune_update(float *v,int framec,struct synth_node *node) {
  //TODO
}

/* Init.
 */
 
static int _detune_init(struct synth_node *node) {
  node->update=_detune_update;
  fprintf(stderr,"%s:%d:%s:TODO\n",__FILE__,__LINE__,__func__);
  return 0;
}

/* Ready.
 */
 
static int _detune_ready(struct synth_node *node) {
  return 0;
}

/* Type definition.
 */
 
const struct synth_node_type synth_node_type_detune={
  .name="detune",
  .objlen=sizeof(struct synth_node_detune),
  .del=_detune_del,
  .init=_detune_init,
  .ready=_detune_ready,
};

/* Setup.
 */
 
int synth_node_detune_setup(struct synth_node *node,const uint8_t *arg,int argc) {
  if (!node||(node->type!=&synth_node_type_detune)||node->ready) return -1;
  //TODO
  return 0;
}
