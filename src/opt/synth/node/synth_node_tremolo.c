#include "../synth_internal.h"

/* Instance definition.
 */
 
struct synth_node_tremolo {
  struct synth_node hdr;
  uint32_t p;
  uint32_t dp;
  float wave[SYNTH_WAVE_SIZE_SAMPLES];
};

#define NODE ((struct synth_node_tremolo*)node)

/* Cleanup.
 */
 
static void _tremolo_del(struct synth_node *node) {
}

/* Init.
 */
 
static int _tremolo_init(struct synth_node *node) {
  return 0;
}

/* Update.
 */
 
static void _tremolo_update_mono(float *v,int framec,struct synth_node *node) {
  for (;framec-->0;v+=1,NODE->p+=NODE->dp) {
    v[0]*=NODE->wave[NODE->p>>SYNTH_WAVE_SHIFT];
  }
}
 
static void _tremolo_update_stereo(float *v,int framec,struct synth_node *node) {
  for (;framec-->0;v+=2,NODE->p+=NODE->dp) {
    float adjust=NODE->wave[NODE->p>>SYNTH_WAVE_SHIFT];
    v[0]*=adjust;
    v[1]*=adjust;
  }
}

/* Ready.
 */
 
static int _tremolo_ready(struct synth_node *node) {
  // Check (node->chanc) if you're not LTI.
  if (node->chanc==1) node->update=_tremolo_update_mono;
  else node->update=_tremolo_update_stereo;
  return 0;
}

/* Type definition.
 */
 
const struct synth_node_type synth_node_type_tremolo={
  .name="tremolo",
  .objlen=sizeof(struct synth_node_tremolo),
  .del=_tremolo_del,
  .init=_tremolo_init,
  .ready=_tremolo_ready,
};

/* Configure.
 */
 
int synth_node_tremolo_configure(struct synth_node *node,const void *src,int srcc) {
  if (!node||(node->type!=&synth_node_type_tremolo)||node->ready) return -1;
  const uint8_t *SRC=src;
  if (srcc<4) return -1;
  int period=(SRC[0]<<8)|SRC[1];
  int depth=SRC[2];
  int phase=SRC[3];
  
  if (period<1) {
    NODE->dp=0;
  } else {
    double hz=1000.0/(double)period;
    NODE->dp=(uint32_t)((hz*4294967296.0)/(double)node->synth->rate);
  }
  
  NODE->p=phase|(phase<<8)|(phase<<16)|(phase<<24);
  
  if (!depth) {
    int i=SYNTH_WAVE_SIZE_SAMPLES;
    float *v=NODE->wave;
    for (;i-->0;v++) *v=1.0f;
  } else {
    if (synth_require_sine(node->synth)<0) return -1;
    memcpy(NODE->wave,node->synth->sine,sizeof(float)*SYNTH_WAVE_SIZE_SAMPLES);
    int i=SYNTH_WAVE_SIZE_SAMPLES;
    float *v=NODE->wave;
    float fdepth=(depth/255.0f)*0.5f;
    for (;i-->0;v++) *v=1.0f-((*v)+1.0f)*fdepth;
  }
  
  return 0;
}
