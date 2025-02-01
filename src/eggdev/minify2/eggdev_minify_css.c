#include "mf_internal.h"

/* Cleanup.
 */
 
void eggdev_minify_css_cleanup(struct eggdev_minify_css *ctx) {
}

/* Minify CSS, main entry point.
 */
 
int eggdev_minify_css(struct eggdev_minify_css *ctx) {
  //TODO
  return sr_encode_raw(ctx->dst,ctx->src,ctx->srcc);
}
