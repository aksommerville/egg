#include "../synth_internal.h"

/* Instance definition.
 */
 
struct synth_node_pipe {
  struct synth_node hdr;
};

#define NODE ((struct synth_node_pipe*)node)

/* Cleanup.
 */
 
static void _pipe_del(struct synth_node *node) {
}

/* Init.
 */
 
static int _pipe_init(struct synth_node *node) {
  return 0;
}

/* Update.
 */
 
static void _pipe_update(float *v,int framec,struct synth_node *node) {
  int i=0;
  for (;i<node->childc;i++) {
    struct synth_node *child=node->childv[i];
    child->update(v,framec,child);
  }
}

/* Ready.
 */
 
static int _pipe_ready(struct synth_node *node) {
  node->update=_pipe_update;
  int i=node->childc; while (i-->0) {
    if (synth_node_ready(node->childv[i])<0) return -1;
  }
  return 0;
}

/* Type definition.
 */
 
const struct synth_node_type synth_node_type_pipe={
  .name="pipe",
  .objlen=sizeof(struct synth_node_pipe),
  .del=_pipe_del,
  .init=_pipe_init,
  .ready=_pipe_ready,
};

/* Add operation.
 */
 
static int pipe_add_op(struct synth_node *node,uint8_t opcode,const uint8_t *src,int srcc) {
  // See etc/doc/audio-format.md
  struct synth_node *op;
  switch (opcode) {
    case 0x01: {
        if (
          !(op=synth_node_spawn(node,&synth_node_type_waveshaper,0))||
          (synth_node_waveshaper_configure(op,src,srcc)<0)
        ) return -1;
      } break;
    case 0x02: {
        if (
          !(op=synth_node_spawn(node,&synth_node_type_delay,0))||
          (synth_node_delay_configure(op,src,srcc)<0)
        ) return -1;
      } break;
    case 0x03: {
        if (
          !(op=synth_node_spawn(node,&synth_node_type_tremolo,0))||
          (synth_node_tremolo_configure(op,src,srcc)<0)
        ) return -1;
      } break;
    case 0x04:
    case 0x05:
    case 0x06:
    case 0x07: {
        if (
          !(op=synth_node_spawn(node,&synth_node_type_iir,0))||
          (synth_node_iir_configure(op,opcode,src,srcc)<0)
        ) return -1;
      } break;
    default: {
        fprintf(stderr,"Unimplemented synth post opcode 0x%02x\n",opcode);
        return -1;
      }
  }
  return 0;
}

/* Configure.
 */
 
int synth_node_pipe_configure(struct synth_node *node,const void *src,int srcc) {
  if (!node||(node->type!=&synth_node_type_pipe)||node->ready) return -1;
  const uint8_t *SRC=src;
  int srcp=0;
  while (srcp<srcc) {
    if (srcp>srcc-2) return -1;
    uint8_t opcode=SRC[srcp++];
    uint8_t len=SRC[srcp++];
    if (srcp>srcc-len) return -1;
    if (pipe_add_op(node,opcode,SRC+srcp,len)<0) return -1;
    srcp+=len;
  }
  return 0;
}
