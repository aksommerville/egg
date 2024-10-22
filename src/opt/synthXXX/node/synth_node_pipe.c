#include "../synth_internal.h"

/* Instance definition.
 */
 
struct synth_node_pipe {
  struct synth_node hdr;
  struct synth_node **opv;
  int opc,opa;
};

#define NODE ((struct synth_node_pipe*)node)

/* Cleanup.
 */
 
static void _pipe_del(struct synth_node *node) {
  if (NODE->opv) {
    while (NODE->opc-->0) synth_node_del(NODE->opv[NODE->opc]);
    free(NODE->opv);
  }
}

/* Update.
 */
 
static void _pipe_update(float *v,int framec,struct synth_node *node) {
  struct synth_node **pp=NODE->opv;
  int i=NODE->opc;
  for (;i-->0;pp++) (*pp)->update(v,framec,*pp);
}

/* Init.
 */
 
static int _pipe_init(struct synth_node *node) {
  node->update=_pipe_update;
  return 0;
}

/* Ready.
 */
 
static int _pipe_ready(struct synth_node *node) {
  int i=NODE->opc;
  while (i-->0) {
    if (synth_node_ready(NODE->opv[i])<0) return -1;
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

/* Instantiate op.
 */
 
static struct synth_node *synth_node_pipe_instantiate_op(struct synth *synth,int chanc,uint8_t opcode,const uint8_t *arg,int argc) {
  struct synth_node *op=0;
  int err=0;
  switch (opcode) {
    case 0x80: {
        if (!(op=synth_node_new(synth,&synth_node_type_gain,chanc))) return 0;
        err=synth_node_gain_setup(op,arg,argc);
      } break;
    case 0x81: {
        if (!(op=synth_node_new(synth,&synth_node_type_waveshaper,chanc))) return 0;
        err=synth_node_waveshaper_setup(op,arg,argc);
      } break;
    case 0x82: {
        if (!(op=synth_node_new(synth,&synth_node_type_delay,chanc))) return 0;
        err=synth_node_delay_setup(op,arg,argc);
      } break;
    case 0x84: {
        if (!(op=synth_node_new(synth,&synth_node_type_tremolo,chanc))) return 0;
        err=synth_node_tremolo_setup(op,arg,argc);
      } break;
    case 0x85:
    case 0x86:
    case 0x87:
    case 0x88: {
        if (!(op=synth_node_new(synth,&synth_node_type_iir3,chanc))) return 0;
        float mid=0.0,wid=0.0;
        if (argc>=2) {
          mid=(float)((arg[0]<<8)|arg[1])/(float)synth->rate;
          if (argc>=4) {
            wid=(float)((arg[2]<<8)|arg[3])/(float)synth->rate;
          }
        }
        switch (opcode) {
          case 0x85: err=synth_node_iir3_setup_lopass(op,mid); break;
          case 0x86: err=synth_node_iir3_setup_hipass(op,mid); break;
          case 0x87: err=synth_node_iir3_setup_bpass(op,mid,wid); break;
          case 0x88: err=synth_node_iir3_setup_notch(op,mid,wid); break;
        }
      } break;
  }
  if (err<0) {
    synth_node_del(op);
    return 0;
  }
  return op;
}

/* Add op.
 */
 
int synth_node_pipe_add_op(struct synth_node *node,uint8_t opcode,const uint8_t *arg,int argc) {
  if (!node||(node->type!=&synth_node_type_pipe)||node->ready) return -1;
  if (NODE->opc>=NODE->opa) {
    int na=NODE->opa+4;
    if (na>INT_MAX/sizeof(void*)) return -1;
    void *nv=realloc(NODE->opv,sizeof(void*)*na);
    if (!nv) return -1;
    NODE->opv=nv;
    NODE->opa=na;
  }
  struct synth_node *op=synth_node_pipe_instantiate_op(node->synth,node->chanc,opcode,arg,argc);
  if (!op) {
    // We are allowed to ignore opcodes 0x80..0xdf. 0xe0..0xff are reserved for future critical ops that can't be ignored.
    if (opcode>=0xe0) {
      fprintf(stderr,"ERROR: Synth channel field 0x%02x not defined.\n",opcode);
      return -2;
    }
    return 0;
  }
  NODE->opv[NODE->opc++]=op;
  return 0;
}
