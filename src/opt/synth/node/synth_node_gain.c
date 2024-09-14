#include "../synth_internal.h"

/* Instance definition.
 */
 
struct synth_node_gain {
  struct synth_node hdr;
};

#define NODE ((struct synth_node_gain*)node)

/* Cleanup.
 */
 
static void _gain_del(struct synth_node *node) {
}

/* Update.
 */
 
static void _gain_update(float *v,int framec,struct synth_node *node) {
  //TODO
}

/* Init.
 */
 
static int _gain_init(struct synth_node *node) {
  node->update=_gain_update;
  fprintf(stderr,"%s:%d:%s:TODO\n",__FILE__,__LINE__,__func__);
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
  //TODO
  return 0;
}
