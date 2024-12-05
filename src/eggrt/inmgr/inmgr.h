/* inmgr.h
 * Generic input mapping for Egg.
 */
 
#ifndef INMGR_H
#define INMGR_H

union egg_event;
struct inmgr_device;
struct inmgr_tm;

/* "signal" are stateless events that can be mapped to keyboard or gamepads.
 * These always have bit 0x40000000 set.
 * Not visible to games, and not part of the platform API.
 */
#define EGG_SIGNAL_BIT             0x40000000
#define EGG_SIGNAL_QUIT            0x40000001
#define EGG_SIGNAL_FULLSCREEN      0x40000002 /* toggle */
#define EGG_SIGNAL_PAUSE           0x40000003 /* toggle */
#define EGG_SIGNAL_STEP            0x40000004 /* Step one video frame, if hard-paused. */
#define EGG_SIGNAL_SAVESTATE       0x40000005
#define EGG_SIGNAL_LOADSTATE       0x40000006
#define EGG_SIGNAL_SCREENCAP       0x40000007
#define EGG_SIGNAL_CONFIGIN        0x40000008 /* Launch input configurer. */

#define EGG_SIGNAL_FOR_EACH \
  _(QUIT) \
  _(FULLSCREEN) \
  _(PAUSE) \
  _(STEP) \
  _(SAVESTATE) \
  _(LOADSTATE) \
  _(SCREENCAP) \
  _(CONFIGIN)

struct inmgr {
// Owner can read directly:
  int *playerv;
  int playerc; // including player zero
// Private:
  int syskbd; // devid
  struct inmgr_device **devicev;
  int devicec,devicea;
  struct inmgr_tm **tmv;
  int tmc,tma;
  int tmv_dirty;
  void (*cb_override)(struct hostio_input *driver,int devid,int btnid,int value,void *userdata);
  void *userdata_override;
};

void inmgr_del(struct inmgr *inmgr);
struct inmgr *inmgr_new();
int inmgr_update(struct inmgr *inmgr);

int inmgr_enable_system_keyboard(struct inmgr *inmgr,int enable);

void inmgr_event_key(struct inmgr *inmgr,int keycode,int value);
void inmgr_event_connect(struct inmgr *inmgr,struct hostio_input *driver,int devid);
void inmgr_event_disconnect(struct inmgr *inmgr,struct hostio_input *driver,int devid);
void inmgr_event_button(struct inmgr *inmgr,struct hostio_input *driver,int devid,int btnid,int value);

int inmgr_button_repr(char *dst,int dsta,int btnid);
int inmgr_button_eval(int *btnid,const char *src,int srcc);

/* The remainder is only for use by incfg.
 ***************************************************************************/
 
// When we have a non-null override callback, most normal processing is suspended.
void inmgr_override_all(struct inmgr *inmgr,void (*cb)(struct hostio_input *driver,int devid,int btnid,int value,void *userdata),void *userdata);

void inmgr_tm_del(struct inmgr_tm *tm);
struct inmgr_tm *inmgr_tm_new(int vid,int pid,int version,const char *name,int namec);

int inmgr_tm_synthesize_button(struct inmgr_tm *tm,int srcbtnid,int lo,int hi,int dstbtnid);
int inmgr_tm_synthesize_threeway(struct inmgr_tm *tm,int srcbtnid,int lo,int hi,int dstbtnid);
int inmgr_tm_synthesize_hat(struct inmgr_tm *tm,int srcbtnid,int lo,int hi);

/* Add this template to our permanent config.
 * If an identical template (by matching criteria) already exists, retain its position and signal events.
 * Otherwise the new one goes in front.
 * In any success, (tm) is handed off.
 */
int inmgr_install_tm_over(struct inmgr *inmgr,struct inmgr_tm *tm_HANDOFF);

/* Dangerously attempt to delete every device, then simulate reconnection so they find the latest template.
 * Do this after installing new templates on the fly.
 */
int inmgr_reconnect_all(struct inmgr *inmgr);

#endif
