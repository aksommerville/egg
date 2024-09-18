#ifndef INMGR_INTERNAL_H
#define INMGR_INTERNAL_H

#include "eggrt/eggrt_internal.h"

/* Egg Platform API doesn't technically impose a limit, but we need one for sanity's sake.
 * Very few games go above 4, 4 and 2 are the traditional hardware limits.
 * 8 is conceivable, I've written games that go that high.
 * 16 strains credibility. That's a good limit.
 */
#define INMGR_PLAYER_LIMIT 16

/* Composite button IDs for axes and hats.
 */
#define EGG_BTN_HORZ (EGG_BTN_LEFT|EGG_BTN_RIGHT)
#define EGG_BTN_VERT (EGG_BTN_UP|EGG_BTN_DOWN)
#define EGG_BTN_DPAD (EGG_BTN_HORZ|EGG_BTN_VERT)

struct inmgr_device_button {
  int srcbtnid;
  int srcvalue;
  int dstbtnid; // EGG_BTN_DPAD, EGG_SIGNAL_*, or single bit (no HORZ or VERT)
  int dstvalue;
  int srclo,srchi;
};

struct inmgr_device {
  int devid;
  int state;
  int playerid;
  struct inmgr_device_button *buttonv;
  int buttonc,buttona;
};

struct inmgr_tm_button {
  int srcbtnid;
  int dstbtnid; // All composites and signals allowed.
};

struct inmgr_tm {
  int vid,pid,version;
  char *name;
  int namec;
  struct inmgr_tm_button *buttonv;
  int buttonc,buttona;
};

void inmgr_signal(struct inmgr *inmgr,int sigid);
void inmgr_set_button(struct inmgr *inmgr,int playerid,int btnid,int value);

struct inmgr_device *inmgr_device_by_devid(const struct inmgr *inmgr,int devid);
int inmgr_devicev_search(const struct inmgr *inmgr,int devid);
struct inmgr_device *inmgr_devicev_spawn(struct inmgr *inmgr,int devid); // => WEAK
void inmgr_device_del(struct inmgr_device *device);
int inmgr_device_buttonv_search(const struct inmgr_device *device,int srcbtnid); // Points to the first, if >=0.

struct inmgr_tm *inmgr_tmv_synthesize(
  struct inmgr *inmgr,
  struct hostio_input *driver,int devid, // 0,0 to hijack this function as "add new tm"
  int vid,int pid,int version,const char *name,int namec
); // => WEAK
int inmgr_tm_match(const struct inmgr_tm *tm,int vid,int pid,int version,const char *name,int namec);
int inmgr_tm_apply(struct inmgr *inmgr,struct inmgr_device *device,struct inmgr_tm *tm,struct hostio_input *driver);
int inmgr_decode_tmv(struct inmgr *inmgr,const char *src,int srcc,const char *path);
int inmgr_encode_tmv(struct sr_encoder *dst,struct inmgr *inmgr);

#endif
