#ifndef EGGRT_INTERNAL_H
#define EGGRT_INTERNAL_H

#include "egg/egg.h"
#include "opt/rom/rom.h"
#include "opt/serial/serial.h"
#include "opt/fs/fs.h"
#include "opt/hostio/hostio.h"
#include "opt/render/render.h"
#include "opt/synth/synth.h"
#include "inmgr/inmgr.h"
#include "incfg/incfg.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>

#if USE_mswin
  #define PATH_SEP_STR "\\"
  #define PATH_SEP_CHAR '\\'
#else
  #define PATH_SEP_STR "/"
  #define PATH_SEP_CHAR '/'
#endif

// When recording or playing back, all updates have a fixed duration.
#define EGGRT_RECORDING_UPDATE_INTERVAL 0.016666
#define EGGRT_RECORDING_FAKE_TIME 1000000000.0

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
  char *storepath;
  char *inmgr_path;
  char *store_extra; // JSON, composed from '--store:KEY=VALUE' args
  char *cfgpath;
  char *record_path;
  char *playback_path;
  
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
  
  // eggrt_store.c:
  struct eggrt_store_field {
    char *k,*v;
    int kc,vc;
  } *storev;
  int storec,storea;
  int store_dirty;
  
  // eggrt_drivers.c:
  struct hostio *hostio;
  void *iconstorage;
  struct render *render;
  struct inmgr *inmgr;
  int fbw,fbh;
  struct synth *synth;
  
  volatile int terminate;
  int exitstatus;
  int focus,debugger; // Hard pause if either is set.
  int debugstep; // Nonzero to advance one frame, when debugger set.
  struct incfg *incfg; // If not null, interactive input configuration is in progress.
  
} eggrt;

int eggrt_configure(int argc,char **argv);
int eggrt_configure_guess_language();

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

void eggrt_clock_init();
void eggrt_clock_report();
double eggrt_clock_update();
double eggrt_now_real();
double eggrt_now_cpu();
void eggrt_sleep(double s);

void eggrt_store_quit();
int eggrt_store_init();
struct eggrt_store_field *eggrt_store_get_field(const char *k,int kc,int create);
int eggrt_store_set_field(struct eggrt_store_field *field,const char *v,int vc); // (field) must have been returned by eggrt_store_get_field
int eggrt_store_save(); // Writes file whether dirty or not; caller should check first.

void eggrt_drivers_quit();
int eggrt_drivers_init();
int eggrt_drivers_update();

void eggrt_record_quit();
int eggrt_record_init();
double eggrt_record_update(double elapsed);

#endif
