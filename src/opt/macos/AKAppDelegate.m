#include "macioc_internal.h"

@implementation AKAppDelegate

/* Startup.
 */

-(void)applicationDidFinishLaunching:(NSNotification*)notification {
  if (macioc.delegate.init) {
    if (macioc.delegate.init(macioc.delegate.userdata)<0) {
      macioc.delegate.init=0;
      macioc_terminate(1);
      return;
    }
    macioc.delegate.init=0;
  }

  if (macioc.delegate.update&&(macioc.delegate.rate>0)) {
    uint64_t interval=1000000000/macioc.delegate.rate;
    // Must be "main_queue" not "global_queue", so updates happen on the main thread.
    // (Cocoa is picky about this).
    dispatch_queue_main_t q=dispatch_get_main_queue();
    dispatch_source_t source=dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER,0,0,q);
    dispatch_source_set_timer(source,dispatch_time(DISPATCH_TIME_NOW,0),interval,0);
    dispatch_source_set_event_handler(source,^(){ macioc.delegate.update(macioc.delegate.userdata); });
    dispatch_activate(source);
  }
}

/* Termination.
 */

-(NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication*)sender {
  return NSTerminateNow;
}

-(BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication*)sender {
  return 1;
}

-(void)applicationWillTerminate:(NSNotification*)notification {
  macioc.terminate=1;
  if (macioc.delegate.quit) {
    void (*fn)(void*)=macioc.delegate.quit;
    macioc.delegate.quit=0; // guarantee not to call more than once
    fn(macioc.delegate.userdata);
  }
}

/* Receive system error.
 */

-(NSError*)application:(NSApplication*)application willPresentError:(NSError*)error {
  const char *message=error.localizedDescription.UTF8String;
  fprintf(stderr,"%s\n",message);
  return error;
}

/* Change input focus.
 */

-(void)applicationDidBecomeActive:(NSNotification*)notification {
  if (macioc.delegate.focus) macioc.delegate.focus(macioc.delegate.userdata,1);
}

-(void)applicationDidResignActive:(NSNotification*)notification {
  if (macioc.delegate.focus) macioc.delegate.focus(macioc.delegate.userdata,0);
}

@end
