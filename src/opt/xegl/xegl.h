/* xegl.h
 * Required:
 * Optional: hostio(drop glue file if absent) xinerama(not a real unit)
 * Link: -lX11 (-lXinerama) -lGL
 *
 * Interface to X11 with OpenGL.
 * "xegl" and "x11fb" could have been implemented as one unit.
 * I chose to separate them so that x11fb consumers don't need to link pointlessly against OpenGL.
 */
 
#ifndef XEGL_H
#define XEGL_H

struct xegl;

// Identical to hostio_video_delegate, except we call with (userdata) instead of (driver).
// When hostio is in use, callbacks pass through directly.
struct xegl_delegate {
  void *userdata;
  void (*cb_close)(void *userdata);
  void (*cb_focus)(void *userdata,int focus);
  void (*cb_resize)(void *userdata,int w,int h);
  int (*cb_key)(void *userdata,int keycode,int value);
  void (*cb_text)(void *userdata,int codepoint);
  void (*cb_mmotion)(void *userdata,int x,int y);
  void (*cb_mbutton)(void *userdata,int btnid,int value);
  void (*cb_mwheel)(void *userdata,int dx,int dy);
};

// Identical to hostio_video_setup.
struct xegl_setup {
  const char *title;
  const void *iconrgba;
  int iconw,iconh;
  int w,h; // Actual window. Typically (0,0) to let us decide.
  int fullscreen;
  int fbw,fbh; // Only for guiding initial window size decision.
  const char *device; // DISPLAY
};

void xegl_del(struct xegl *xegl);

struct xegl *xegl_new(
  const struct xegl_delegate *delegate,
  const struct xegl_setup *setup
);

int xegl_update(struct xegl *xegl);

/* The OpenGL context is guaranteed in scope only between begin and end.
 */
int xegl_begin(struct xegl *xegl);
int xegl_end(struct xegl *xegl);

/* Odds and ends.
 * The cursor is initially hidden.
 * Locking the cursor implicitly hides it, and unlocking does not show it again.
 * Initial fullscreen state is specified in setup.
 */
void xegl_show_cursor(struct xegl *xegl,int show);
void xegl_lock_cursor(struct xegl *xegl,int lock);
void xegl_set_fullscreen(struct xegl *xegl,int fullscreen);
void xegl_suppress_screensaver(struct xegl *xegl);
void xegl_set_title(struct xegl *xegl,const char *src);

void xegl_get_size(int *w,int *h,const struct xegl *xegl);
int xegl_get_fullscreen(const struct xegl *xegl);

#endif
