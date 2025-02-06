#include "mf_internal.h"

/* Digest, main entry point.
 */
 
int mf_js_digest(struct eggdev_minify_js *ctx) {
  if (!ctx||!ctx->root) return -1;
  fprintf(stderr,"%s...\n",__func__);
  
  fprintf(stderr,"...%s\n",__func__);
  return 0;
}
