#include "../synth_internal.h"

/* Instance definition.
 */
 
struct synth_node_waveshaper {
  struct synth_node hdr;
};

#define NODE ((struct synth_node_waveshaper*)node)

/* Cleanup.
 */
 
static void _waveshaper_del(struct synth_node *node) {
}

/* Update.
 */
 
static void _waveshaper_update(float *v,int framec,struct synth_node *node) {
  //TODO
}

/* Init.
 */
 
static int _waveshaper_init(struct synth_node *node) {
  node->update=_waveshaper_update;
  fprintf(stderr,"%s:%d:%s:TODO\n",__FILE__,__LINE__,__func__);
  return 0;
}

/* Ready.
 */
 
static int _waveshaper_ready(struct synth_node *node) {
  return 0;
}

/* Type definition.
 */
 
const struct synth_node_type synth_node_type_waveshaper={
  .name="waveshaper",
  .objlen=sizeof(struct synth_node_waveshaper),
  .del=_waveshaper_del,
  .init=_waveshaper_init,
  .ready=_waveshaper_ready,
};

/* Setup.
 */
 
int synth_node_waveshaper_setup(struct synth_node *node,const uint8_t *arg,int argc) {
  if (!node||(node->type!=&synth_node_type_waveshaper)||node->ready) return -1;
  //TODO
  return 0;
}
