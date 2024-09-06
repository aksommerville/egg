#include "sfgc_internal.h"

/* Uncompile, main entry point.
 */
 
int sfg_uncompile(struct sr_encoder *dst,const void *src,int srcc,const char *refname) {
  if (!dst||(srcc<0)||(srcc&&!src)) return -1;
  fprintf(stderr,"TODO %s %s:%d\n",__func__,__FILE__,__LINE__);
  return -2;
}
