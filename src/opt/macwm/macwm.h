/* macwm.h
 * Interface to MacOS window manager.
 * Single window.
 * You must establish contact with the operating system (eg with 'macioc').
 * Link: -framework Cocoa -framework Quartz -framework OpenGL -framework MetalKit -framework Metal -framework CoreGraphics
 */

#ifndef MACWM_H
#define MACWM_H

struct macwm;

#define MACWM_RENDERMODE_FRAMEBUFFER 1
#define MACWM_RENDERMODE_OPENGL 2
#define MACWM_RENDERMODE_METAL 3

struct macwm_delegate {
  void *userdata;
  void (*close)(void *userdata);
  void (*resize)(void *userdata,int w,int h);
  int (*key)(void *userdata,int keycode,int value);
  void (*text)(void *userdata,int codepoint);
  void (*mmotion)(void *userdata,int x,int y);
  void (*mbutton)(void *userdata,int btnid,int value);
  void (*mwheel)(void *userdata,int dx,int dy);
};

struct macwm_setup {
  int w,h; // Window size, or we make something up.
  int fullscreen;
  const char *title;
  int rendermode;
  int fbw,fbh; // Required for MACWM_RENDERMODE_FRAMEBUFFER, ignored otherwise.
};

void macwm_del(struct macwm *macwm);

struct macwm *macwm_new(
  const struct macwm_delegate *delegate,
  const struct macwm_setup *setup
);

void macwm_get_size(int *w,int *h,const struct macwm *macwm);
int macwm_get_fullscreen(const struct macwm *macwm);

#define MACWM_FULLSCREEN_ON       1
#define MACWM_FULLSCREEN_OFF      0
#define MACWM_FULLSCREEN_TOGGLE  -1
void macwm_set_fullscreen(struct macwm *macwm,int state);

int macwm_update(struct macwm *macwm);

/* Valid for MACWM_RENDERMODE_FRAMEBUFFER only.
 * (fb) must be 32-bit RGBX at the size declared at new().
 * The image is not necessarily committed synchronously!
 * You must arrange to keep (fb) constant for a little while after.
 */
void macwm_send_framebuffer(struct macwm *macwm,const void *fb);

/* Valid for MACWM_RENDERMODE_OPENGL and MACWM_RENDERMODE_METAL.
 * All render calls should happen between begin and end.
 */
int macwm_render_begin(struct macwm *macwm);
void macwm_render_end(struct macwm *macwm);

#define MACWM_MBUTTON_LEFT 0
#define MACWM_MBUTTON_RIGHT 1
#define MACWM_MBUTTON_MIDDLE 2

int macwm_hid_from_mac_keycode(int maccode);

/* We use this internally, to drop any keys held when we lose input focus.
 * (that's a bug in Apple's event management IMHO, but easy enough to work around).
 * The public can use it too, if you ever have a need to forget all held keys.
 * This does fire callbacks.
 */
void macwm_release_keys(struct macwm *macwm);

//TODO show/hide cursor

#endif
