#include "eggdev/eggdev_internal.h"
#include "jst.h"

/* Minify, main entry point.
 */
 
int eggdev_main_minify() {
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
  struct jst_context jst={0};
  int err=jst_minify(&dst,&jst,src,srcc,srcpath);
  jst_context_cleanup(&jst);
  free(src);
  if (err<0) {
    if (err!=-2) fprintf(stderr,"%s: Unspecified error minifying Javascript.\n",srcpath);
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
