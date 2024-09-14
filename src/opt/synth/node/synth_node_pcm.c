#include "../synth_internal.h"

/* Instance definition.
 */
 
struct synth_node_pcm {
  struct synth_node hdr;
};

#define NODE ((struct synth_node_pcm*)node)

/* Cleanup.
 */
 
static void _pcm_del(struct synth_node *node) {
}

/* Update.
 */
 
static void _pcm_update(float *v,int framec,struct synth_node *node) {
  //TODO
}

/* Init.
 */
 
static int _pcm_init(struct synth_node *node) {
  node->update=_pcm_update;
  fprintf(stderr,"%s:%d:%s:TODO\n",__FILE__,__LINE__,__func__);
  return 0;
}

/* Ready.
 */
 
static int _pcm_ready(struct synth_node *node) {
  node->finished=1;//TODO
  return 0;
}

/* Type definition.
 */
 
const struct synth_node_type synth_node_type_pcm={
  .name="pcm",
  .objlen=sizeof(struct synth_node_pcm),
  .del=_pcm_del,
  .init=_pcm_init,
  .ready=_pcm_ready,
};

/* Setup.
 */
 
int synth_node_pcm_setup(struct synth_node *node,struct synth_pcm *pcm,float trim,float pan) {
  if (!node||!pcm||(node->type!=&synth_node_type_pcm)||node->ready) return -1;
  //TODO
  return 0;
}
