#include "../synth_internal.h"

/* Instance definition.
 */
 
struct synth_node_waveshaper {
  struct synth_node hdr;
  float *ptv;
  int ptc;
};

#define NODE ((struct synth_node_waveshaper*)node)

/* Cleanup.
 */
 
static void _waveshaper_del(struct synth_node *node) {
  if (NODE->ptv) free(NODE->ptv);
}

/* Update.
 * We are pure LTI; we don't care how many channels there are.
 */

// Zero or one point, emit DC.
static void _waveshaper_update_0(float *v,int c,struct synth_node *node) {
  c*=node->chanc;
  for (;c-->0;v++) *v=0.0f;
}
static void _waveshaper_update_1(float *v,int c,struct synth_node *node) {
  c*=node->chanc;
  for (;c-->0;v++) *v=NODE->ptv[0];
}

// 3 points and the middle is zero.
static void _waveshaper_update_np3(float *v,int c,struct synth_node *node) {
  c*=node->chanc;
  for (;c-->0;v++) {
    if (*v<0.0f) (*v)*=NODE->ptv[0]; // inverted at ready
    else (*v)*=NODE->ptv[2];
  }
}

// General update, suitable for any point count >=2.
static void _waveshaper_update(float *v,int c,struct synth_node *node) {
  c*=node->chanc;
  const float scale=NODE->ptc/2.0f;
  for (;c-->0;v++) {
    float norm=((*v)+1.0f)*scale;
    int i0=(int)norm;
    if (i0<0) { *v=NODE->ptv[0]; continue; }
    if (i0>=NODE->ptc-1) { *v=NODE->ptv[NODE->ptc-1]; continue; }
    float fract=norm-(float)i0;
    *v=NODE->ptv[i0]*(1.0f-fract)+NODE->ptv[i0+1]*fract;
  }
}

/* Init.
 */
 
static int _waveshaper_init(struct synth_node *node) {
  return 0;
}

/* Ready.
 */
 
static int _waveshaper_ready(struct synth_node *node) {
  if (NODE->ptc<1) node->update=_waveshaper_update_0;
  else if (NODE->ptc==1) node->update=_waveshaper_update_1;
  else if ((NODE->ptc==3)&&(NODE->ptv[1]==0.0f)) { node->update=_waveshaper_update_np3; NODE->ptv[0]*=-1.0f; }
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

/* Setup.
 */
 
int synth_node_waveshaper_setup(struct synth_node *node,const uint8_t *arg,int argc) {
  if (!node||(node->type!=&synth_node_type_waveshaper)||node->ready) return -1;
  if (NODE->ptv) free(NODE->ptv);
  NODE->ptv=0;
  NODE->ptc=0;
  int ptc=argc>>1;
  if (ptc<1) return 0;
  if (!(NODE->ptv=malloc(sizeof(float)*ptc))) return -1;
  NODE->ptc=ptc;
  float *dst=NODE->ptv;
  int i=ptc;
  for (;i-->0;dst++,arg+=2) {
    *dst=(((arg[0]<<8)|arg[1])-0x8000)/32768.0;
  }
  return 0;
}
