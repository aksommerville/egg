#include "../synth_internal.h"

/* Instance definition.
 */
 
struct synth_node_delay {
  struct synth_node hdr;
  struct synth_cbuf *cbufl,*cbufr;
  float dry,wet,sto,fbk;
};

#define NODE ((struct synth_node_delay*)node)

/* Cleanup.
 */
 
static void _delay_del(struct synth_node *node) {
  synth_cbuf_del(NODE->cbufl);
  synth_cbuf_del(NODE->cbufr);
}

/* Update.
 */
 
static void _delay_update_stereo(float *v,int framec,struct synth_node *node) {
  for (;framec-->0;v+=2) {
    float wetl=synth_cbuf_read(NODE->cbufl);
    float wetr=synth_cbuf_read(NODE->cbufr);
    float dryl=v[0];
    float dryr=v[1];
    v[0]=v[0]*NODE->dry+wetl*NODE->wet;
    v[1]=v[1]*NODE->dry+wetr*NODE->wet;
    synth_cbuf_write(NODE->cbufl,dryl*NODE->sto+wetl*NODE->fbk);
    synth_cbuf_write(NODE->cbufr,dryr*NODE->sto+wetr*NODE->fbk);
  }
}
 
static void _delay_update_mono(float *v,int framec,struct synth_node *node) {
  for (;framec-->0;v+=1) {
    float wet=synth_cbuf_read(NODE->cbufl);
    float dry=v[0];
    v[0]=dry*NODE->dry+wet*NODE->wet;
    synth_cbuf_write(NODE->cbufl,dry*NODE->sto+wet*NODE->fbk);
  }
}

/* Init.
 */
 
static int _delay_init(struct synth_node *node) {
  if (node->chanc==2) node->update=_delay_update_stereo;
  else node->update=_delay_update_mono;
  return 0;
}

/* Ready.
 */
 
static int _delay_ready(struct synth_node *node) {
  if (!NODE->cbufl) return -1;
  if ((node->chanc==2)&&!NODE->cbufr) return -1;
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
  uint8_t src[]={0,0,0,0,0,0};
  if (argc>=sizeof(src)) memcpy(src,arg,sizeof(src));
  else memcpy(src,arg,argc);
  int ms=(src[0]<<8)|src[1];
  int framec=(ms*node->synth->rate)/1000;
  if (framec<1) framec=1;
  synth_cbuf_del(NODE->cbufl);
  if (!(NODE->cbufl=synth_cbuf_new(framec))) return -1;
  if (node->chanc==2) {
    synth_cbuf_del(NODE->cbufr);
    if (!(NODE->cbufr=synth_cbuf_new(framec))) return -1;
  }
  NODE->dry=src[2]/255.0f;
  NODE->wet=src[3]/255.0f;
  NODE->sto=src[4]/255.0f;
  NODE->fbk=src[5]/255.0f;
  return 0;
}
