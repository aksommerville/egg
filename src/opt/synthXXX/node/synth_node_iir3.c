#include "../synth_internal.h"

/* Instance definition.
 */
 
struct synth_node_iir3 {
  struct synth_node hdr;
  struct synth_iir3 l,r;
};

#define NODE ((struct synth_node_iir3*)node)

/* Cleanup.
 */
 
static void _iir3_del(struct synth_node *node) {
}

/* Update.
 */
 
static void _iir3_update_stereo(float *v,int framec,struct synth_node *node) {
  for (;framec-->0;v+=2) {
    v[0]=synth_iir3_update(&NODE->l,v[0]);
    v[1]=synth_iir3_update(&NODE->r,v[1]);
  }
}
 
static void _iir3_update_mono(float *v,int framec,struct synth_node *node) {
  for (;framec-->0;v+=1) {
    v[0]=synth_iir3_update(&NODE->l,v[0]);
  }
}

/* Init.
 */
 
static int _iir3_init(struct synth_node *node) {
  if (node->chanc==2) node->update=_iir3_update_stereo;
  else node->update=_iir3_update_mono;
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
  synth_iir3_init_lopass(&NODE->l,norm);
  memcpy(&NODE->r,&NODE->l,sizeof(struct synth_iir3));
  return 0;
}
 
int synth_node_iir3_setup_hipass(struct synth_node *node,float norm) {
  if (!node||(node->type!=&synth_node_type_iir3)||node->ready) return -1;
  synth_iir3_init_hipass(&NODE->l,norm);
  memcpy(&NODE->r,&NODE->l,sizeof(struct synth_iir3));
  return 0;
}
 
int synth_node_iir3_setup_bpass(struct synth_node *node,float norm,float width) {
  if (!node||(node->type!=&synth_node_type_iir3)||node->ready) return -1;
  synth_iir3_init_bpass(&NODE->l,norm,width);
  memcpy(&NODE->r,&NODE->l,sizeof(struct synth_iir3));
  return 0;
}
 
int synth_node_iir3_setup_notch(struct synth_node *node,float norm,float width) {
  if (!node||(node->type!=&synth_node_type_iir3)||node->ready) return -1;
  synth_iir3_init_notch(&NODE->l,norm,width);
  memcpy(&NODE->r,&NODE->l,sizeof(struct synth_iir3));
  return 0;
}
