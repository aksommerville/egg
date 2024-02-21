#ifndef EGG_NATIVE_INTERNAL_H
#define EGG_NATIVE_INTERNAL_H

#include "egg/egg.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <limits.h>
#include "opt/hostio/hostio.h"
#include "opt/arr/arr.h"

extern struct egg {
  
  // Managed by egg_native_config.c.
  const char *exename;
  char *rompath;
  char *video_driver;
  int video_w;
  int video_h;
  int video_rate;
  int video_fullscreen;
  char *video_device;
  char *audio_driver;
  int audio_rate;
  int audio_chanc;
  int audio_buffersize;
  char *audio_device;
  char *input_driver;
  
  volatile int sigc;
  int terminate;
  void *rom;
  uint8_t romhash[20]; // sha1, binary
  
  char *storepath;
  struct egg_store_entry {
    int kc,vc;
    char *k;
    void *v;
  } *storev;
  int storec,storea;
  int store_dirty;
  
  struct hostio *hostio;
  struct arr arr;
  
  /* EGG_INPUT_USAGE_*.
   * 0 is DISABLE, the default.
   */
  int input_key;
  int input_text;
  int input_mmotion;
  int input_mbutton;
  int input_jbutton;
  int input_jbutton_all;
  int input_janalogue;
  int input_accelerometer;
  
  int mousex,mousey;
  
} egg;

void egg_native_config_cleanup();
int egg_native_config(int argc,char **argv);

int egg_native_drivers_init();

// Main loop should spam this. Do nothing if clean, save if dirty.
int egg_native_store_update();

int egg_native_rom_expects_file();
int egg_native_rom_is_true_native(); // as in, the game's code is native and can't be trusted.

// From the "details" resource if present.
// Mostly you want get_detail, which returns values verbatim.
void egg_native_rom_get_framebuffer(int *w,int *h);
int egg_native_rom_get_storageSize();
int egg_native_rom_get_detail(char *dst,int dsta,const char *k,int kc);

void egg_native_rom_cleanup();
int egg_native_rom_init(); // load things; no client code
int egg_native_rom_start(); // executes client's bootstrap
int egg_native_rom_update(double elapsed);
void egg_native_rom_render();
void egg_native_rom_event(const void/*struct egg_event*/ *event);
void egg_native_rom_farewell();

void egg_native_cb_close(void *userdata);
void egg_native_cb_resize(void *userdata,int w,int h);
void egg_native_cb_focus(void *userdata,int focus);
int egg_native_cb_key(void *userdata,int keycode,int value);
void egg_native_cb_char(void *userdata,int codepoint);
void egg_native_cb_mmotion(void *userdata,int x,int y);
void egg_native_cb_mbutton(void *userdata,int btnid,int value);
void egg_native_cb_mwheel(void *userdata,int dx,int dy);
void egg_native_cb_pcm_out(int16_t *v,int c,void *userdata);
void egg_native_cb_connect(void *userdata,int devid);
void egg_native_cb_disconnect(void *userdata,int devid);
void egg_native_cb_button(void *userdata,int devid,int btnid,int value);

#endif
