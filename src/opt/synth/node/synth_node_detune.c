#include "../synth_internal.h"

/* Instance definition.
 */
 
struct synth_node_detune {
  struct synth_node hdr;
  struct synth_cbuf *cbufl,*cbufr;
  struct synth_osc osc;
};

#define NODE ((struct synth_node_detune*)node)

/* Cleanup.
 */
 
static void _detune_del(struct synth_node *node) {
  synth_cbuf_del(NODE->cbufl);
  synth_cbuf_del(NODE->cbufr);
}

/* Update.
 */
 
static void _detune_update_stereo(float *v,int framec,struct synth_node *node) {
  // We're using two buffers, to preserve the stereo position.
  // Resist the temptation to offset phase between them, or anything like that -- it will break them, and sound like two distinct voices at -1 and 1 pan.
  for (;framec-->0;v+=2) {
    int pvp=NODE->cbufl->p; // Doesn't matter which; they can only move in sync.
    synth_cbuf_write(NODE->cbufl,v[0]);
    synth_cbuf_write(NODE->cbufr,v[1]);
    float displacement=synth_osc_next(&NODE->osc);
    int p=(int)(((displacement+1.0f)*0.5f)*NODE->cbufl->c);
    if (p<0) p=0; else if (p>=NODE->cbufl->c) p=NODE->cbufl->c-1;
    p=pvp-p; if (p<0) p+=NODE->cbufl->c;
    v[0]=NODE->cbufl->v[p];
    v[1]=NODE->cbufr->v[p];
  }
}
 
static void _detune_update_mono(float *v,int framec,struct synth_node *node) {
  for (;framec-->0;v+=1) {
    int pvp=NODE->cbufl->p;
    synth_cbuf_write(NODE->cbufl,v[0]);
    float displacement=synth_osc_next(&NODE->osc);
    int p=(int)(((displacement+1.0f)*0.5f)*NODE->cbufl->c);
    if (p<0) p=0; else if (p>=NODE->cbufl->c) p=NODE->cbufl->c-1;
    p=pvp-p; if (p<0) p+=NODE->cbufl->c;
    v[0]=NODE->cbufl->v[p];
  }
}

/* Init.
 */
 
static int _detune_init(struct synth_node *node) {
  if (node->chanc==2) node->update=_detune_update_stereo;
  else node->update=_detune_update_mono;
  return 0;
}

/* Ready.
 */
 
static int _detune_ready(struct synth_node *node) {
  if (!NODE->cbufl) return -1;
  if ((node->chanc==2)&&!NODE->cbufr) return -1;
  return 0;
}

/* Type definition.
 */
 
const struct synth_node_type synth_node_type_detune={
  .name="detune",
  .objlen=sizeof(struct synth_node_detune),
  .del=_detune_del,
  .init=_detune_init,
  .ready=_detune_ready,
};

/* Setup.
 */
 
int synth_node_detune_setup(struct synth_node *node,const uint8_t *arg,int argc) {
  if (!node||(node->type!=&synth_node_type_detune)||node->ready) return -1;
  uint8_t src[]={0,0,0,0,0};
  if (argc>=sizeof(src)) memcpy(src,arg,sizeof(src));
  else memcpy(src,arg,argc);
  if (synth_osc_decode(&NODE->osc,node->synth,src,sizeof(src))<0) return -1;
  NODE->osc.scale=1.0f;
  int depthms=(src[2]<<8)|src[3];
  int framec=(depthms*node->synth->rate)/1000;//XXX
  if (framec<1) framec=1;
  else if (framec>2000) framec=2000;
  synth_cbuf_del(NODE->cbufl);
  if (!(NODE->cbufl=synth_cbuf_new(framec))) return -1;
  if (node->chanc==2) {
    synth_cbuf_del(NODE->cbufr);
    if (!(NODE->cbufr=synth_cbuf_new(framec))) return -1;
  }
  //TODO
  return 0;
}
