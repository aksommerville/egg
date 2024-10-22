#include "../synth_internal.h"

/* Instance definition.
 */
 
struct synth_node_pcm {
  struct synth_node hdr;
  struct synth_pcm *pcm;
  int p;
  float triml,trimr;
  int rid;
};

#define NODE ((struct synth_node_pcm*)node)

/* Cleanup.
 */
 
static void _pcm_del(struct synth_node *node) {
  synth_pcm_del(NODE->pcm);
}

/* Init.
 */
 
static int _pcm_init(struct synth_node *node) {
  return 0;
}

/* Update.
 */
 
static void _pcm_update_mono(float *v,int framec,struct synth_node *node) {
  int remaining=NODE->pcm->c-NODE->p;
  if (remaining<framec) {
    framec=remaining;
    node->finished=1;
  }
  for (;framec-->0;v++,NODE->p++) {
    (*v)+=NODE->pcm->v[NODE->p]*NODE->triml;
  }
}
 
static void _pcm_update_stereo(float *v,int framec,struct synth_node *node) {
  int remaining=NODE->pcm->c-NODE->p;
  if (remaining<framec) {
    framec=remaining;
    node->finished=1;
  }
  for (;framec-->0;v+=2,NODE->p++) {
    v[0]+=NODE->pcm->v[NODE->p]*NODE->triml;
    v[1]+=NODE->pcm->v[NODE->p]*NODE->trimr;
  }
}

/* Ready.
 */
 
static int _pcm_ready(struct synth_node *node) {
  if (!NODE->pcm) return -1;
  if (node->chanc==1) node->update=_pcm_update_mono;
  else node->update=_pcm_update_stereo;
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

/* Configure.
 */
 
int synth_node_pcm_configure(struct synth_node *node,struct synth_pcm *pcm,float trim,float pan,int rid) {
  if (!node||(node->type!=&synth_node_type_pcm)||node->ready) return -1;
  if (synth_pcm_ref(pcm)<0) return -1;
  NODE->pcm=pcm;
  NODE->rid=rid;
  if (node->chanc==1) {
    NODE->triml=NODE->trimr=trim;
  } else {
         if (pan<=-1.0f) { NODE->triml=1.0f; NODE->trimr=0.0f; }
    else if (pan<0.0f) { NODE->triml=1.0f; NODE->trimr=pan+1.0f; }
    else if (pan>=1.0f) { NODE->triml=0.0f; NODE->trimr=1.0f; }
    else if (pan>0.0f) { NODE->triml=1.0f-pan; NODE->trimr=1.0f; }
    else { NODE->triml=1.0f; NODE->trimr=1.0f; }
    NODE->triml*=trim;
    NODE->trimr*=trim;
  }
  return 0;
}

/* Trivial accessors.
 */
 
int synth_node_pcm_get_rid(const struct synth_node *node) {
  if (!node||(node->type!=&synth_node_type_pcm)) return 0;
  return NODE->rid;
}

double synth_node_pcm_get_playhead(const struct synth_node *node) {
  if (!node||(node->type!=&synth_node_type_pcm)) return 0.0f;
  if (!NODE->pcm) return 0.0f;
  return (double)NODE->p/(double)node->synth->rate;
}

void synth_node_pcm_set_playhead(struct synth_node *node,double s) {
  if (!node||(node->type!=&synth_node_type_pcm)) return;
  NODE->p=(int)(s*node->synth->rate);
  if (NODE->p<0) { NODE->p=0; node->finished=0; }
  else if (NODE->p>=NODE->pcm->c) { NODE->p=NODE->pcm->c; node->finished=1; }
  else node->finished=0;
}
