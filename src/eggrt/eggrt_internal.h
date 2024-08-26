#ifndef EGGRT_INTERNAL_H
#define EGGRT_INTERNAL_H

#include "egg/egg.h"
#include "opt/rom/rom.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>

extern struct eggrt {

  // Acquired at eggrt_configure():
  const char *exename;
  char *rompath;
  const char *rptname; // (rompath) if relevant, (exename) otherwise. For game-related logging.
  int lang; // ([0]<<5)|[1], our usual format. "aa" by default.
  char *video_drivers;
  char *audio_drivers;
  char *input_drivers;
  char *video_device;
  char *audio_device;
  int fullscreen;
  int window_w,window_h;
  int audio_rate;
  int audio_chanc;
  int audio_buffer;
  int configure_input;
  
  // eggrt_romsrc.c:
  const void *romserial;
  int romserialc;
  struct rom_res *resv; // Contains all resources, and payloads point into (romserial).
  int resc,resa;
  
  // eggrt_exec.c: (also it has a bunch of its own private globals elsewhere)
  int exec_callstate; // (0,1,2)=(none,initted,quitted)
  
  // eggrt_clock.c:
  double pvtime;
  double framelen;
  int framec;
  double starttime_real;
  double starttime_cpu;
  int clock_faultc;
  
  volatile int terminate;
  int exitstatus;
  int audiolimit;
  
} eggrt;

int eggrt_configure(int argc,char **argv);

extern const int eggrt_has_embedded_rom;
int eggrt_romsrc_init();
void eggrt_romsrc_quit();
int eggrt_rom_get(void *dstpp,int tid,int rid);

void eggrt_exec_quit();
int eggrt_exec_init();
void eggrt_exec_client_quit(int status);
int eggrt_exec_client_init();
int eggrt_exec_client_update(double elapsed);
int eggrt_exec_client_render();
int eggrt_exec_client_synth(void *dstpp,int samplec);

void eggrt_clock_init();
void eggrt_clock_report();
double eggrt_clock_update();
double eggrt_now_real();
double eggrt_now_cpu();
void eggrt_sleep(double s);

#endif
