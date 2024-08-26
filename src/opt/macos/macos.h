/* macos.h
 * Glue for NSApplicationMain().
 */

#ifndef MACOS_H
#define MACOS_H

int macos_prerun_argv(int argc,char **argv);

int macos_main(
  int argc,char **argv,
  void (*cb_quit)(void *userdata),
  int (*cb_init)(void *userdata),
  void (*cb_update)(void *userdata),
  void *userdata
);

void macioc_terminate(int status);

#endif
