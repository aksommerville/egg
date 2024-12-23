#include "macos.h"
#include "macioc_internal.h"
#include "egg/egg.h"
#include <stdio.h>
#include <Cocoa/Cocoa.h>

struct macioc macioc={0};

/* Prerun argv. I pass a couple args from the build setup, to set working directory and TTY.
 */

int macos_prerun_argv(int argc,char **argv) {
  int i=1;
  while (i<argc) {
  
    if (!memcmp(argv[i],"--chdir=",8)) {
      chdir(argv[i]+8);
      argc--;
      memmove(argv+i,argv+i+1,sizeof(void*)*(argc-i));
      continue;
    }

    if (!memcmp(argv[i],"--reopen-tty=",13)) {
      int fd=open(argv[i]+13,O_RDWR);
      if (fd>=0) {
        dup2(fd,STDOUT_FILENO);
        dup2(fd,STDIN_FILENO);
        dup2(fd,STDERR_FILENO);
        close(fd);
      }
      argc--;
      memmove(argv+i,argv+i+1,sizeof(void*)*(argc-i));
      continue;
    }

    i++;
  }
  return argc;
}

/* Main.
 */

int macos_main(
  int argc,char **argv,
  void (*cb_quit)(void *userdata),
  int (*cb_init)(void *userdata),
  void (*cb_update)(void *userdata),
  void *userdata
) {
  macioc.delegate.rate=60.0;
  macioc.delegate.quit=cb_quit;
  macioc.delegate.init=cb_init;
  macioc.delegate.update=cb_update;
  return NSApplicationMain(argc,(void*)argv);
}

/* Terminate.
 */

void macioc_terminate(int status) {
  macioc.terminate=1;
  [NSApplication.sharedApplication terminate:0];
}

/* System language.
 */
 
int macos_get_preferred_languages(int *dst,int dsta) {
  if (!dst||(dsta<1)) return 0;
  const NSArray *languages=NSLocale.preferredLanguages;
  if (!languages) return 0;
  int dstc=0;
  int c=languages.count;
  int i=0; for (;i<c;i++) {
    if (dstc>=dsta) break;
    const char *lang=[[languages objectAtIndex:i] UTF8String];
    if (!lang) continue;
    if ((lang[0]<'a')||(lang[0]>'z')) continue;
    if ((lang[1]<'a')||(lang[1]>'z')) continue;
    int code=EGG_LANG_FROM_STRING(lang);
    // I haven't seen this happen, but wouldn't be surprised if they allow multiple of one language (eg "en-US" and "en-UK").
    int unique=1;
    int j=dstc;
    while (j-->0) {
      if (code==dst[j]) {
        unique=0;
        break;
      }
    }
    if (!unique) continue;
    dst[dstc++]=code;
  }
  return dstc;
}
