#include "mf_internal.h"

/* Minify in memory without context.
 */
 
int eggdev_minify_inner(struct sr_encoder *dst,const char *src,int srcc,const char *srcpath,int fmt) {
  int dstc0=dst->c;
  char sfx[16];
  int sfxc=eggdev_normalize_suffix(sfx,16,srcpath,-1);
  if (!fmt) fmt=eggdev_cvta2a_guess_format(sfx,sfxc,src,srcc,0);
  switch (fmt) {
  
    case EGGDEV_FMT_HTML: {
        struct eggdev_minify_html ctx={
          .dst=dst,
          .srcpath=srcpath,
          .src=src,
          .srcc=srcc,
        };
        int err=eggdev_minify_html(&ctx);
        if (err<0) {
          eggdev_minify_html_cleanup(&ctx);
          if (err!=-2) fprintf(stderr,"%s: Unspecified error minifying HTML.\n",srcpath);
          return -2;
        }
      } break;
      
    case EGGDEV_FMT_CSS: {
        struct eggdev_minify_css ctx={
          .dst=dst,
          .srcpath=srcpath,
          .src=src,
          .srcc=srcc,
        };
        int err=eggdev_minify_css(&ctx);
        if (err<0) {
          eggdev_minify_css_cleanup(&ctx);
          if (err!=-2) fprintf(stderr,"%s: Unspecified error minifying CSS.\n",srcpath);
          return -2;
        }
      } break;
      
    case EGGDEV_FMT_JS: {
        struct eggdev_minify_js ctx={
          .dst=dst,
          .srcpath=srcpath,
        };
        int err=eggdev_minify_js(&ctx,src,srcc);
        if (err<0) {
          eggdev_minify_js_cleanup(&ctx);
          if (err!=-2) fprintf(stderr,"%s: Unspecified error minifying Javascript.\n",srcpath);
          return -2;
        }
      } break;
      
    default: {
        fprintf(stderr,"%s:WARNING: Unexpected format for minification. Emitting verbatim.\n",srcpath);
        if (sr_encode_raw(dst,src,srcc)<0) return -1;
      }
  }
  // Force LF at the end of every non-empty file, for legibility.
  if ((dst->c>dstc0)&&(((char*)dst->v)[dst->c-1]!=0x0a)) {
    if (sr_encode_u8(dst,0x0a)<0) return -1;
  }
  return 0;
}

/* Minify, main entry point.
 */
 
int eggdev_main_minify_WIP() {//XXX Work in progress. Will eventually replace the other eggdev_main_minify
  if (!eggdev.dstpath) {
    fprintf(stderr,"%s: Output path required.\n",eggdev.exename);
    return -2;
  }
  if (eggdev.srcpathc!=1) {
    fprintf(stderr,"%s: Minify expects exactly 1 input file, found %d.\n",eggdev.exename,eggdev.srcpathc);
    return -2;
  }
  const char *srcpath=eggdev.srcpathv[0],*dstpath=eggdev.dstpath;
  void *src=0;
  int srcc=file_read(&src,srcpath);
  if (srcc<0) {
    fprintf(stderr,"%s: Failed to read file.\n",srcpath);
    return -2;
  }
  struct sr_encoder dst={0};
  int err=eggdev_minify_inner(&dst,src,srcc,srcpath,0);
  free(src);
  if (err<0) {
    if (err!=-2) fprintf(stderr,"%s: Unspecified error minifying.\n",srcpath);
    sr_encoder_cleanup(&dst);
    return -2;
  }
  if (file_write(dstpath,dst.v,dst.c)<0) {
    fprintf(stderr,"%s: Failed to write file, %d bytes\n",dstpath,dst.c);
    sr_encoder_cleanup(&dst);
    return -2;
  }
  sr_encoder_cleanup(&dst);
  return 0;
}
