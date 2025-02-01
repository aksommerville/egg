#include "eggdev/eggdev_internal.h"

/* Get value by key. Never null.
 */
 
static const char *eggdev_config_get(const char *k) {
  #define _(tag) if (!strcmp(k,#tag)) return eggdev_buildcfg.tag;
  EGGDEV_BUILDCFG_FOR_EACH
  #undef _
  return "";
}

/* Config, main entry point.
 */
 
int eggdev_main_config() {
  if (eggdev.srcpathc) {
    int i=0; for (;i<eggdev.srcpathc;i++) {
      const char *k=eggdev.srcpathv[i];
      const char *v=eggdev_config_get(k);
      fprintf(stdout,"%s\n",v);
    }
  } else {
    #define _(tag) fprintf(stdout,"%s = %s\n",#tag,eggdev_buildcfg.tag);
    EGGDEV_BUILDCFG_FOR_EACH
    #undef _
  }
  return 0;
}
