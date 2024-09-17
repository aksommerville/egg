#include "../synth_internal.h"

/* Instance definition.
 */
 
struct synth_node_pcm {
  struct synth_node hdr;
  struct synth_pcm *pcm;
  int p;
  float mixl,mixr;
};

#define NODE ((struct synth_node_pcm*)node)

/* Cleanup.
 */
 
static void _pcm_del(struct synth_node *node) {
  synth_pcm_del(NODE->pcm);
}

/* Update.
 */
 
static void _pcm_update_mono(float *v,int framec,struct synth_node *node) {
  int updc=NODE->pcm->c-NODE->p;
  if (updc>framec) updc=framec;
  int i=updc;
  for (;i-->0;v++,NODE->p++) {
    v[0]+=NODE->pcm->v[NODE->p]*NODE->mixl;
  }
  if (NODE->p>=NODE->pcm->c) node->finished=1;
}
 
static void _pcm_update_stereo(float *v,int framec,struct synth_node *node) {
  int updc=NODE->pcm->c-NODE->p;
  if (updc>framec) updc=framec;
  int i=updc;
  for (;i-->0;v+=2,NODE->p++) {
    v[0]+=NODE->pcm->v[NODE->p]*NODE->mixl;
    v[1]+=NODE->pcm->v[NODE->p]*NODE->mixr;
  }
  if (NODE->p>=NODE->pcm->c) node->finished=1;
}

/* Init.
 */
 
static int _pcm_init(struct synth_node *node) {
  if (node->chanc==2) node->update=_pcm_update_stereo;
  else node->update=_pcm_update_mono;
  return 0;
}

/* Ready.
 */
 
static int _pcm_ready(struct synth_node *node) {
  if (!NODE->pcm) return -1;
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
  if (synth_pcm_ref(pcm)<0) return -1;
  synth_pcm_del(NODE->pcm);
  NODE->pcm=pcm;
  NODE->mixl=NODE->mixr=trim;
  if (node->chanc==2) {
    if (pan<0.0f) NODE->mixr*=1.0f+pan;
    else NODE->mixl*=1.0f-pan;
  }
  return 0;
}
