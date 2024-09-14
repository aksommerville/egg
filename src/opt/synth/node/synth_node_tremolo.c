#include "../synth_internal.h"

/* Instance definition.
 */
 
struct synth_node_tremolo {
  struct synth_node hdr;
};

#define NODE ((struct synth_node_tremolo*)node)

/* Cleanup.
 */
 
static void _tremolo_del(struct synth_node *node) {
}

/* Update.
 */
 
static void _tremolo_update(float *v,int framec,struct synth_node *node) {
  //TODO
}

/* Init.
 */
 
static int _tremolo_init(struct synth_node *node) {
  node->update=_tremolo_update;
  fprintf(stderr,"%s:%d:%s:TODO\n",__FILE__,__LINE__,__func__);
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
  //TODO
  return 0;
}
