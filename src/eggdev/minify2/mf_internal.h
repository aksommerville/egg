#ifndef MF_INTERNAL_H
#define MF_INTERNAL_H

#include "eggdev/eggdev_internal.h"

/* Generic entry point we can re-enter.
 */
int eggdev_minify_inner(struct sr_encoder *dst,const char *src,int srcc,const char *srcpath,int fmt);

/* CSS.
 ****************************************************************/
 
struct eggdev_minify_css {
  struct sr_encoder *dst; // WEAK
  const char *srcpath;
  const char *src;
  int srcc;
};

void eggdev_minify_css_cleanup(struct eggdev_minify_css *ctx);
int eggdev_minify_css(struct eggdev_minify_css *ctx);

/* HTML.
 ****************************************************************/
 
struct eggdev_minify_html {
  struct sr_encoder *dst; // WEAK
  const char *srcpath;
  const char *src;
  int srcc;
};

void eggdev_minify_html_cleanup(struct eggdev_minify_html *ctx);
int eggdev_minify_html(struct eggdev_minify_html *ctx);

/* Javascript.
 ****************************************************************/
 
struct eggdev_minify_js {
  struct sr_encoder *dst; // WEAK
  const char *srcpath;
};

void eggdev_minify_js_cleanup(struct eggdev_minify_js *ctx);
int eggdev_minify_js(struct eggdev_minify_js *ctx,const char *src,int srcc);

#endif
