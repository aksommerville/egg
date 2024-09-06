#include "eggdev_internal.h"

struct eggdev eggdev={0};

/* Main.
 */
 
int main(int argc,char **argv) {
  int err;
  if ((err=eggdev_configure(argc,argv))<0) {
    if (err!=-2) fprintf(stderr,"%s: Unspecified error configuring.\n",eggdev.exename);
    return 1;
  }
  if (!eggdev.command||!strcmp(eggdev.command,"help")) eggdev_print_help(eggdev.helptopic);
  #define _(tag) else if (!strcmp(eggdev.command,#tag)) err=eggdev_main_##tag();
  _(pack)
  _(unpack)
  _(bundle)
  _(list)
  _(validate)
  _(serve)
  _(config)
  _(dump)
  #undef _
  else {
    fprintf(stderr,"%s: Unknown command '%s'\n",eggdev.exename,eggdev.command);
    eggdev_print_help(0);
    err=-2;
  }
  if (err<0) {
    if (err!=-2) fprintf(stderr,"%s: Unspecified error running command '%s'\n",eggdev.exename,eggdev.command);
    return 1;
  }
  return 0;
}
