#include "../synth_internal.h"

/* Limit our buffer size, regardless of output rate.
 * A 1-million frame delay is ridiculous, it's got to be a mistake.
 * In theory, the song can ask for a 65-second period.
 */
#define DELAY_LIMIT_FRAMES (1<<20)

/* Instance definition.
 */
 
struct synth_node_delay {
  struct synth_node hdr;
  float dry,wet,sto,fbk;
  float *buf;
  int bufc; // in samples, always a multiple of chanc
  int bufp; // in samples. steps by 2 when stereo
};

#define NODE ((struct synth_node_delay*)node)

/* Cleanup.
 */
 
static void _delay_del(struct synth_node *node) {
  if (NODE->buf) free(NODE->buf);
}

/* Init.
 */
 
static int _delay_init(struct synth_node *node) {
  return 0;
}

/* Update.
 */
 
static void _delay_update_mono(float *v,int framec,struct synth_node *node) {
  for (;framec-->0;v+=1) {
    float dry=v[0];
    float wet=NODE->buf[NODE->bufp];
    NODE->buf[NODE->bufp]=dry*NODE->sto+wet*NODE->fbk;
    v[0]=dry*NODE->dry+wet*NODE->wet;
    if ((NODE->bufp+=1)>=NODE->bufc) NODE->bufp=0;
  }
}
 
static void _delay_update_stereo(float *v,int framec,struct synth_node *node) {
  for (;framec-->0;v+=2) {
    float dryl=v[0];
    float dryr=v[1];
    float wetl=NODE->buf[NODE->bufp];
    float wetr=NODE->buf[NODE->bufp+1];
    NODE->buf[NODE->bufp]=dryl*NODE->sto+wetl*NODE->fbk;
    NODE->buf[NODE->bufp+1]=dryr*NODE->sto+wetr*NODE->fbk;
    v[0]=dryl*NODE->dry+wetl*NODE->wet;
    v[1]=dryr*NODE->dry+wetr*NODE->wet;
    if ((NODE->bufp+=2)>=NODE->bufc) NODE->bufp=0;
  }
}

/* Ready.
 */
 
static int _delay_ready(struct synth_node *node) {
  if (node->chanc==1) node->update=_delay_update_mono;
  else node->update=_delay_update_stereo;
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

/* Configure.
 */
 
int synth_node_delay_configure(struct synth_node *node,const void *src,int srcc) {
  if (!node||(node->type!=&synth_node_type_delay)||node->ready) return -1;
  if (NODE->buf) return -1;
  const uint8_t *SRC=src;
  if (srcc<5) return -1;
  int ms=(SRC[0]<<8)|SRC[1];
  NODE->wet=SRC[2]/255.0f;
  NODE->dry=1.0f-NODE->wet;
  NODE->sto=SRC[3]/255.0f;
  NODE->fbk=SRC[4]/255.0f;
  
  int framec=((double)ms*(double)node->synth->rate)/1000.0f;
  if (framec<1) framec=1;
  if (framec>DELAY_LIMIT_FRAMES) {
    fprintf(stderr,"WARNING: Clamping synth delay period from %d to %d frames due to built-in safety limit.\n",framec,DELAY_LIMIT_FRAMES);
    framec=DELAY_LIMIT_FRAMES;
  }
  NODE->bufc=framec*node->chanc;
  if (!(NODE->buf=calloc(sizeof(float),NODE->bufc))) return -1;

  return 0;
}
