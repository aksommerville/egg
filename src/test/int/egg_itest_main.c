#include "test/egg_test.h"
#include "egg_itest_toc.h"

// We link against most of eggdev but not its main. So we need to declare its dummy globals.
#include "eggdev/eggdev_internal.h"
struct eggdev eggdev={0};

int main(int argc,char **argv) {
  const struct egg_itest *itest=egg_itestv;
  int i=sizeof(egg_itestv)/sizeof(struct egg_itest);
  for (;i-->0;itest++) {
    if (!egg_test_filter(itest->name,itest->tags,itest->enable)) {
      fprintf(stderr,"EGG_TEST SKIP %s (%s) %s:%d\n",itest->name,itest->tags,itest->file,itest->line);
    } else if (itest->fn()<0) {
      fprintf(stderr,"EGG_TEST FAIL %s (%s) %s:%d\n",itest->name,itest->tags,itest->file,itest->line);
    } else {
      fprintf(stderr,"EGG_TEST PASS %s\n",itest->name);
    }
  }
  return 0;
}
