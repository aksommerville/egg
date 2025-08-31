#ifndef INMGR_INTERNAL_H
#define INMGR_INTERNAL_H

#include "inmgr.h"
#include "opt/fs/fs.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>
#include <stdint.h>

#define INMGR_EXTBTN_LIMIT 16

/* The various modes are only applicable for specific dstbtnid.
 */
#define INMGR_BUTTON_MODE_NOOP 0
#define INMGR_BUTTON_MODE_SIGNAL 1
#define INMGR_BUTTON_MODE_TWOSTATE 2
#define INMGR_BUTTON_MODE_THREEWAY 3
#define INMGR_BUTTON_MODE_HAT 4
#define INMGR_BUTTON_MODE_LINEAR 5

extern struct inmgr {
  int init;
  int btnmask;
  int override_listenerid;
  
  struct inmgr_extbtn {
    int btnid;
    int lo,hi;
  } extbtnv[INMGR_EXTBTN_LIMIT];
  int extbtnc;
  
  struct inmgr_signal {
    int btnid;
    void (*cb)();
  } *signalv;
  int signalc,signala;
  
  struct inmgr_player {
    int state; // 16 bits.
    int extbtnv[INMGR_EXTBTN_LIMIT];
  } playerv[1+INMGR_PLAYER_LIMIT];
  int playerc; // Not counting player zero.
  
  // Live devices.
  struct inmgr_device {
    int devid;
    int vid,pid,version;
    char *name;
    int namec;
    int tmid;
    int ready; // Configuration completed.
    int keyboard; // Influences default mapping and player assignment.
    int enable; // If zero, do not sync state out to (playerv).
    int playerid;
    int state;
    int extbtnv[INMGR_EXTBTN_LIMIT];
    struct inmgr_button {
      int srcbtnid;
      int srcvalue;
      int mode;
      int srclo,srchi; // Mapping thresholds, usage depends on (mode).
      int dstvalue;
      int dstbtnid;
      int hidusage,lo,hi; // Addl details retained for live remapping or inspection.
    } *buttonv;
    int buttonc,buttona;
  } *devicev;
  int devicec,devicea;
  
  struct inmgr_listener {
    int listenerid;
    void (*cb)(int devid,int btnid,int value,int state,void *userdata);
    void *userdata;
  } *listenerv;
  int listenerc,listenera;
  int listenerid_next;
  
  // Templates, matching the config file.
  struct inmgr_tm {
    int tmid; // Positive and unique.
    int vid,pid,version;
    char *name;
    int namec;
    char *comment;
    int commentc;
    struct inmgr_tmbtn {
      int srcbtnid;
      int dstbtnid;
      char *comment;
      int commentc;
    } *tmbtnv;
    int tmbtnc,tmbtna;
  } *tmv;
  int tmc,tma;
  
} inmgr;

// inmgr_context.c
int inmgr_extbtnv_search(int btnid);
int inmgr_signalv_search(int btnid);
void inmgr_broadcast(int devid,int btnid,int value,int state);
void inmgr_signal(int btnid);

// inmgr_device.c
void inmgr_device_cleanup(struct inmgr_device *device);
int inmgr_devicev_search(int devid);
struct inmgr_device *inmgr_device_by_devid(int devid);
int inmgr_device_buttonv_search(const struct inmgr_device *device,int srcbtnid);
struct inmgr_button *inmgr_button_by_srcbtnid(const struct inmgr_device *device,int srcbtnid);
int inmgr_device_set_name(struct inmgr_device *device,const char *src,int srcc);

// inmgr_connect.c
int inmgr_button_remap(struct inmgr_button *button,int dstbtnid,const char *comment,int commentc);

// inmgr_tm.c
void inmgr_tmbtn_cleanup(struct inmgr_tmbtn *tmbtn);
void inmgr_tm_cleanup(struct inmgr_tm *tm);
int inmgr_load(); // File-not-found is not an error.
struct inmgr_tm *inmgr_tm_for_keyboard();
struct inmgr_tm *inmgr_tm_for_device(const struct inmgr_device *device);
struct inmgr_tm *inmgr_tm_synthesize_from_device(struct inmgr_device *device); // =>WEAK, adds to the global tm list
struct inmgr_tmbtn *inmgr_tmbtn_for_srcbtnid(const struct inmgr_tm *tm,int srcbtnid);
int inmgr_tmbtn_intern(struct inmgr_tm *tm,int srcbtnid,int dstbtnid);

// inmgr_text.c
int inmgr_comment_has_token(const char *comment,int commentc,const char *token,int tokenc);

#endif
