/* eggdev_main_bundle.c
 * Bundling is a complex operation, and it's markedly different for HTML and Native.
 * The three variations of Native all have a lot in common.
 * All this file does is distinguish HTML from Native, and call out.
 */

#include "eggdev_internal.h"

int eggdev_bundle_html();
int eggdev_bundle_native();
 
int eggdev_main_bundle() {
  if (!eggdev.dstpath) {
    fprintf(stderr,"%s: Please specify output path as '-oPATH'\n",eggdev.exename);
    return -2;
  }
  int dstpathc=0;
  while (eggdev.dstpath[dstpathc]) dstpathc++;
  if ((dstpathc>=5)&&!memcmp(eggdev.dstpath+dstpathc-5,".html",5)) return eggdev_bundle_html();
  if ((dstpathc>=4)&&!memcmp(eggdev.dstpath+dstpathc-4,".htm",4)) return eggdev_bundle_html();
  return eggdev_bundle_native();
}
