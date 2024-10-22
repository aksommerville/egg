#include "../synth_internal.h"

/* Instance definition.
 */
 
struct synth_node_skeleton {
  struct synth_node hdr;
};

#define NODE ((struct synth_node_skeleton*)node)

/* Cleanup.
 */
 
static void _skeleton_del(struct synth_node *node) {
}

/* Init.
 */
 
static int _skeleton_init(struct synth_node *node) {
  return 0;
}

/* Update.
 */
 
static void _skeleton_update(float *v,int framec,struct synth_node *node) {
  // Add to (v). If you're a filter, read from it first. Length never exceeds SYNTH_UPDATE_LIMIT_FRAMES<<1.
}

/* Ready.
 */
 
static int _skeleton_ready(struct synth_node *node) {
  // Check (node->chanc) if you're not LTI.
  node->update=_skeleton_update;
  return 0;
}

/* Type definition.
 */
 
const struct synth_node_type synth_node_type_skeleton={
  .name="skeleton",
  .objlen=sizeof(struct synth_node_skeleton),
  .del=_skeleton_del,
  .init=_skeleton_init,
  .ready=_skeleton_ready,
};
