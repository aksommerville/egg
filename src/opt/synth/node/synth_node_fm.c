#include "../synth_internal.h"

/* Instance definition.
 */
 
struct synth_node_fm {
  struct synth_node hdr;
};

#define NODE ((struct synth_node_fm*)node)

/* Cleanup.
 */
 
static void _fm_del(struct synth_node *node) {
}

/* Update.
 */
 
static void _fm_update(float *v,int framec,struct synth_node *node) {
  //TODO
}

/* Init.
 */
 
static int _fm_init(struct synth_node *node) {
  node->update=_fm_update;
  fprintf(stderr,"%s:%d:%s:TODO\n",__FILE__,__LINE__,__func__);
  return 0;
}

/* Ready.
 */
 
static int _fm_ready(struct synth_node *node) {
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
