/* hostio_video.h
 * Video drivers create an output context and possibly report keyboard and mouse events.
 */
 
#ifndef HOSTIO_VIDEO_H
#define HOSTIO_VIDEO_H

struct hostio_video;
struct hostio_video_type;
struct hostio_video_delegate;
struct hostio_video_setup;

struct hostio_video_delegate {
  void *userdata;
  void (*cb_close)(void *userdata);
  void (*cb_resize)(void *userdata,int w,int h);
  void (*cb_focus)(void *userdata,int focus);
  int (*cb_key)(void *userdata,int keycode,int value); // keycode is HID page 7, return nonzero to acknowledge.
  void (*cb_char)(void *userdata,int codepoint); // codepoint is Unicode. Only for unacknowledged key events.
  void (*cb_mmotion)(void *userdata,int x,int y);
  void (*cb_mbutton)(void *userdata,int btnid,int value);
  void (*cb_mwheel)(void *userdata,int dx,int dy);
};

struct hostio_video_setup {
  int w,h; // Full size, usually you don't specify.
  int fbw,fbh; // Guidance for size, mostly it's the aspect that matters.
  int ratemin,ratemax;
  int fullscreen;
  const char *device; // Exact meaning varies between drivers.
  const char *title; // UTF-8, for presentation.
  const void *iconrgba; // Given to window manager.
  int iconw,iconh;
};

struct hostio_video {
  const struct hostio_video_type *type;
  struct hostio_video_delegate delegate;
  int w,h;
  int fullscreen;
  int rate;
};

struct hostio_video_type {
  const char *name;
  const char *desc;
  int objlen;
  int appointment_only;
  int provides_keyboard;
  void (*del)(struct hostio_video *driver);
  int (*init)(struct hostio_video *driver,const struct hostio_video_setup *setup);
  int (*update)(struct hostio_video *driver);
  void (*set_fullscreen)(struct hostio_video *driver,int fullscreen);
  void (*suppress_screensaver)(struct hostio_video *driver);
  int (*begin_frame)(struct hostio_video *driver);
  int (*end_frame)(struct hostio_video *driver);
  
  // Drivers are not required to report mouse events while hidden, but they are allowed to.
  // Existence of this hook is a signal that the driver has native mouse support.
  void (*show_cursor)(struct hostio_video *driver,int show);
};

void hostio_video_del(struct hostio_video *driver);

struct hostio_video *hostio_video_new(
  const struct hostio_video_type *type,
  const struct hostio_video_delegate *delegate,
  const struct hostio_video_setup *setup
);

const struct hostio_video_type *hostio_video_type_by_name(const char *name,int namec);
const struct hostio_video_type *hostio_video_type_by_index(int p);

#endif
