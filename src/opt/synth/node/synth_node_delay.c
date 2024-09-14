#include "../synth_internal.h"

/* Instance definition.
 */
 
struct synth_node_delay {
  struct synth_node hdr;
};

#define NODE ((struct synth_node_delay*)node)

/* Cleanup.
 */
 
static void _delay_del(struct synth_node *node) {
}

/* Update.
 */
 
static void _delay_update(float *v,int framec,struct synth_node *node) {
  //TODO
}

/* Init.
 */
 
static int _delay_init(struct synth_node *node) {
  node->update=_delay_update;
  fprintf(stderr,"%s:%d:%s:TODO\n",__FILE__,__LINE__,__func__);
  return 0;
}

/* Ready.
 */
 
static int _delay_ready(struct synth_node *node) {
  return 0;
}

/* Type definition.
 */
 
const struct synth_node_type synth_node_type_delay={
  .name="delay",
  .objlen=sizeof(struct synth_node_delay),
  .del=_delay_del,
  .init=_delay_init,
  .ready=_delay_ready,
};

/* Setup.
 */
 
int synth_node_delay_setup(struct synth_node *node,const uint8_t *arg,int argc) {
  if (!node||(node->type!=&synth_node_type_delay)||node->ready) return -1;
  //TODO
  return 0;
}
