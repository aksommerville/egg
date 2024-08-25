/* eggdev_buildcfg.c
 * The only purpose of this file is to collect build-time configuration from make.
 * The rest of eggdev should not depend on build-time defines; they should all go thru us instead.
 */

#include "eggdev_internal.h"

const struct eggdev_buildcfg eggdev_buildcfg={
  #define _(tag) .tag=EGGDEV_##tag"",
  EGGDEV_BUILDCFG_FOR_EACH
  #undef _
};

int _eggdev_buildcfg_assert(const char *k,const char *v) {
  if (v&&v[0]) return 0;
  fprintf(stderr,"%s: EGGDEV_%s was not defined at build time.\n",eggdev.exename,k);
  return -2;
}
