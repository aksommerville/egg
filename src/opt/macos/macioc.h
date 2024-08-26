/* macioc.h
 * Top-level interface to MacOS.
 * We call NSApplicationMain and position ourselves as its delegate.
 * Optionally triggers an update callback via Dispatch.
 */

#ifndef MACIOC_H
#define MACIOC_H

struct macioc_delegate {
  int rate; // hz, for update(). <=0 we will not update
  void *userdata;
  void (*focus)(void *userdata,int focus);
  void (*quit)(void *userdata);
  int (*init)(void *userdata);
  void (*update)(void *userdata);
};

/* Call from your main().
 */
int macioc_main(int argc,char **argv,const struct macioc_delegate *delegate);

/* Schedule termination at the next convenient time.
 * Not necessarily synchronous.
 */
void macioc_terminate(int status);

#endif
