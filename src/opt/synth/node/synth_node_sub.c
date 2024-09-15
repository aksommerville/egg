#include "../synth_internal.h"

/* Instance definition.
 */
 
struct synth_node_sub {
  struct synth_node hdr;
  float baserate,rate,width; // norm
  struct synth_iir3 iir3,repeat;
  struct synth_env_runner level;
  float mixl,mixr;
};

#define NODE ((struct synth_node_sub*)node)

/* Cleanup.
 */
 
static void _sub_del(struct synth_node *node) {
}

/* Update.
 */
 
static void _sub_update_stereo(float *v,int framec,struct synth_node *node) {
  for (;framec-->0;v+=2) {
    float sample=((rand()&0xffff)-32768)/32768.0f;
    sample=synth_iir3_update(&NODE->iir3,sample);
    sample=synth_iir3_update(&NODE->repeat,sample);
    sample*=synth_env_runner_next(NODE->level);
    v[0]+=sample*NODE->mixl;
    v[1]+=sample*NODE->mixr;
  }
  if (synth_env_runner_is_finished(&NODE->level)) node->finished=1;
}
 
static void _sub_update_mono(float *v,int framec,struct synth_node *node) {
  for (;framec-->0;v++) {
    float sample=((rand()&0xffff)-32768)/32768.0f;
    sample=synth_iir3_update(&NODE->iir3,sample);
    sample=synth_iir3_update(&NODE->repeat,sample);
    sample*=synth_env_runner_next(NODE->level);
    v[0]+=sample*NODE->mixl;
  }
  if (synth_env_runner_is_finished(&NODE->level)) node->finished=1;
}

/* Init.
 */
 
static int _sub_init(struct synth_node *node) {
  if (node->chanc==2) node->update=_sub_update_stereo;
  else node->update=_sub_update_mono;
  return 0;
}

/* Ready.
 */
 
static int _sub_ready(struct synth_node *node) {
  return 0;
}

/* Type definition.
 */
 
const struct synth_node_type synth_node_type_sub={
  .name="sub",
  .objlen=sizeof(struct synth_node_sub),
  .del=_sub_del,
  .init=_sub_init,
  .ready=_sub_ready,
};

/* Setup.
 */
 
int synth_node_sub_setup(
  struct synth_node *node,
  float width,
  float rate,float velocity,int durframes,
  const struct synth_env *env,
  float trim,float pan
) {
  if (!node||(node->type!=&synth_node_type_sub)||node->ready) return -1;
  NODE->baserate=rate;
  NODE->rate=rate;
  NODE->width=width;
  
  NODE->mixl=NODE->mixr=trim;
  if (node->chanc==2) {
    if (pan<0.0f) NODE->mixr*=pan+2.0f;
    else if (pan>0.0f) NODE->mixl*=1.0f-pan;
  }
  
  synth_env_runner_init(&NODE->level,env,velocity);
  synth_env_runner_release_later(&NODE->level,durframes);
  
  synth_iir3_init_bpass(&NODE->iir3,rate,width);
  memcpy(&NODE->repeat,&NODE->iir3,sizeof(struct synth_iir3));

  return 0;
}

/* Adjust rate.
 */
 
void synth_node_sub_adjust_rate(struct synth_node *node,float multiplier) {
  if (!node||(node->type!=&synth_node_type_sub)) return;
  NODE->rate=NODE->baserate*multiplier;
  synth_iir3_init_bpass(&NODE->iir3,NODE->rate,NODE->width);
  memcpy(&NODE->repeat.cv,&NODE->iir3.cv,sizeof(NODE->iir3.cv));
}
