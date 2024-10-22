#include "../synth_internal.h"

/* Instance definition.
 */
 
struct synth_node_iir {
  struct synth_node hdr;
};

#define NODE ((struct synth_node_iir*)node)

/* Cleanup.
 */
 
static void _iir_del(struct synth_node *node) {
}

/* Init.
 */
 
static int _iir_init(struct synth_node *node) {
  return 0;
}

/* Update.
 */
 
static void _iir_update(float *v,int framec,struct synth_node *node) {
  // Add to (v). If you're a filter, read from it first. Length never exceeds SYNTH_UPDATE_LIMIT_FRAMES<<1.
}

/* Ready.
 */
 
static int _iir_ready(struct synth_node *node) {
  // Check (node->chanc) if you're not LTI.
  node->update=_iir_update;
  return 0;
}

/* Type definition.
 */
 
const struct synth_node_type synth_node_type_iir={
  .name="iir",
  .objlen=sizeof(struct synth_node_iir),
  .del=_iir_del,
  .init=_iir_init,
  .ready=_iir_ready,
};

/* TODO Get WebAudio implemented up to this point before proceeding.
 * It is important that everything behave the same way native and web, or at least sound the same.
 * Use the variable-size lopass and hipass from the Blue Book, and multiple-stage 3-point IIR for bandpass and notch.
 */

/* Low pass.
 */
 
static void iir_configure_lopass(struct synth_node *node,float cutoff) {
  fprintf(stderr,"%s cutoff=%f\n",__func__,cutoff);
}

/* High pass.
 */
 
static void iir_configure_hipass(struct synth_node *node,float cutoff) {
  fprintf(stderr,"%s cutoff=%f\n",__func__,cutoff);
}

/* Bandpass.
 */
 
static void iir_configure_bpass(struct synth_node *node,float mid,float wid) {
  fprintf(stderr,"%s mid=%f width=%f\n",__func__,mid,wid);
}

/* Notch.
 */
 
static void iir_configure_notch(struct synth_node *node,float mid,float wid) {
  fprintf(stderr,"%s mid=%f width=%f\n",__func__,mid,wid);
}

/* Configure.
 */
 
int synth_node_iir_configure(struct synth_node *node,uint8_t opcode,const void *src,int srcc) {
  if (!node||(node->type!=&synth_node_type_iir)||node->ready) return -1;
  const uint8_t *SRC=src;
  switch (opcode) {
    case 0x04: {
        if (srcc<2) return -1;
        float cut=(float)((SRC[0]<<8)|SRC[1])/(float)node->synth->rate;
        iir_configure_lopass(node,cut);
      } break;
    case 0x05: {
        if (srcc<2) return -1;
        float cut=(float)((SRC[0]<<8)|SRC[1])/(float)node->synth->rate;
        iir_configure_hipass(node,cut);
      } break;
    case 0x06: {
        if (srcc<4) return -1;
        float mid=(float)((SRC[0]<<8)|SRC[1])/(float)node->synth->rate;
        float wid=(float)((SRC[2]<<8)|SRC[3])/(float)node->synth->rate;
        iir_configure_bpass(node,mid,wid);
      } break;
    case 0x07: {
        if (srcc<4) return -1;
        float mid=(float)((SRC[0]<<8)|SRC[1])/(float)node->synth->rate;
        float wid=(float)((SRC[2]<<8)|SRC[3])/(float)node->synth->rate;
        iir_configure_notch(node,mid,wid);
      } break;
    default: return -1;
  }
  return 0;
}
