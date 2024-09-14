#include "../synth_internal.h"

/* Instance definition.
 */
 
struct synth_node_sub {
  struct synth_node hdr;
};

#define NODE ((struct synth_node_sub*)node)

/* Cleanup.
 */
 
static void _sub_del(struct synth_node *node) {
}

/* Update.
 */
 
static void _sub_update(float *v,int framec,struct synth_node *node) {
  //TODO
}

/* Init.
 */
 
static int _sub_init(struct synth_node *node) {
  node->update=_sub_update;
  fprintf(stderr,"%s:%d:%s:TODO\n",__FILE__,__LINE__,__func__);
  return 0;
}

/* Ready.
 */
 
static int _sub_ready(struct synth_node *node) {
  return 0;
}

/* Type definition.
 */
 
const struct synth_node_type synth_node_type_sub={
  .name="sub",
  .objlen=sizeof(struct synth_node_sub),
  .del=_sub_del,
  .init=_sub_init,
  .ready=_sub_ready,
};
