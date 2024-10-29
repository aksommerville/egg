#include "../synth_internal.h"

/* Instance definition.
 */
 
struct synth_node_waveshaper {
  struct synth_node hdr;
  float *vv; // Covers the full range of inputs -1..1.
  int vc;
};

#define NODE ((struct synth_node_waveshaper*)node)

/* Cleanup.
 */
 
static void _waveshaper_del(struct synth_node *node) {
  if (NODE->vv) free(NODE->vv);
}

/* Init.
 */
 
static int _waveshaper_init(struct synth_node *node) {
  return 0;
}

/* Update.
 */
 
static void _waveshaper_update_dc(float *v,int framec,struct synth_node *node) {
  int i=framec*node->chanc;
  for (;i-->0;v++) *v=NODE->vv[0];
}

static void _waveshaper_update(float *v,int framec,struct synth_node *node) {
  int i=framec*node->chanc;
  float vcf=(float)NODE->vc-1.0f;
  for (;i-->0;v++) {
    float p=(((*v)+1.0f)*vcf)/2.0f;
    if (p<=0.0f) *v=NODE->vv[0];
    else if (p>=vcf) *v=NODE->vv[NODE->vc-1];
    else {
      int pi=(int)floorf(p);
      float bw=p-(float)pi;
      float aw=1.0f-bw;
      *v=NODE->vv[pi]*aw+NODE->vv[pi+1]*bw;
    }
  }
}

/* Ready.
 */
 
static int _waveshaper_ready(struct synth_node *node) {
  if (!NODE->vc) return -1;
  if (NODE->vc==1) node->update=_waveshaper_update_dc;
  else node->update=_waveshaper_update;
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

/* Configure.
 */
 
int synth_node_waveshaper_configure(struct synth_node *node,const void *src,int srcc) {
  if (!node||(node->type!=&synth_node_type_waveshaper)||node->ready) return -1;
  if (NODE->vv) return -1;
  const uint8_t *SRC=src;
  int vc=srcc>>1;
  if (vc<1) {
    if (!(NODE->vv=malloc(sizeof(float)))) return -1;
    NODE->vv[0]=0.0f;
    NODE->vc=1;
    return 0;
  }
  int dstc=vc<<1;
  if (!(NODE->vv=malloc(sizeof(float)*dstc))) return -1;
  float *dst=NODE->vv+vc;
  // Read from (src) into the back half of (NODE->vv).
  int i=vc;
  for (;i-->0;SRC+=2,dst++) {
    *dst=(float)((SRC[0]<<8)|SRC[1])/65535.0f;
  }
  // Copy back half to front half, flipping sign.
  const float *cpsrc=NODE->vv+dstc-1;
  for (dst=NODE->vv,i=vc;i-->0;cpsrc--,dst++) *dst=-*cpsrc;
  NODE->vc=dstc;
  return 0;
}
