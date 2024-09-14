#include "../synth_internal.h"

/* Instance definition.
 */
 
struct synth_node_iir3 {
  struct synth_node hdr;
};

#define NODE ((struct synth_node_iir3*)node)

/* Cleanup.
 */
 
static void _iir3_del(struct synth_node *node) {
}

/* Update.
 */
 
static void _iir3_update(float *v,int framec,struct synth_node *node) {
  //TODO
}

/* Init.
 */
 
static int _iir3_init(struct synth_node *node) {
  node->update=_iir3_update;
  fprintf(stderr,"%s:%d:%s:TODO\n",__FILE__,__LINE__,__func__);
  return 0;
}

/* Ready.
 */
 
static int _iir3_ready(struct synth_node *node) {
  return 0;
}

/* Type definition.
 */
 
const struct synth_node_type synth_node_type_iir3={
  .name="iir3",
  .objlen=sizeof(struct synth_node_iir3),
  .del=_iir3_del,
  .init=_iir3_init,
  .ready=_iir3_ready,
};

/* Setup.
 */
 
int synth_node_iir3_setup_lopass(struct synth_node *node,float norm) {
  if (!node||(node->type!=&synth_node_type_iir3)||node->ready) return -1;
  //TODO
  return 0;
}
 
int synth_node_iir3_setup_hipass(struct synth_node *node,float norm) {
  if (!node||(node->type!=&synth_node_type_iir3)||node->ready) return -1;
  //TODO
  return 0;
}
 
int synth_node_iir3_setup_bpass(struct synth_node *node,float norm,float width) {
  if (!node||(node->type!=&synth_node_type_iir3)||node->ready) return -1;
  //TODO
  return 0;
}
 
int synth_node_iir3_setup_notch(struct synth_node *node,float norm,float width) {
  if (!node||(node->type!=&synth_node_type_iir3)||node->ready) return -1;
  //TODO
  return 0;
}
