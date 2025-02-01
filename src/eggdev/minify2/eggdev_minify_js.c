#include "mf_internal.h"

/* Cleanup.
 */
 
void eggdev_minify_js_cleanup(struct eggdev_minify_js *ctx) {
}

/* Minify Javascript, main entry point.
 */
 
int eggdev_minify_js(struct eggdev_minify_js *ctx,const char *src,int srcc) {
  //TODO
  if (sr_encode_raw(ctx->dst,src,srcc)<0) return -1;
  return 0;
}
