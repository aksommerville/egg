#include "glx_internal.h"

/* Delete.
 */

static void _glx_del(struct hostio_video *driver) {
  if (DRIVER->dpy) XCloseDisplay(DRIVER->dpy);
}

/* New.
 */

static int _glx_init(struct hostio_video *driver,const struct hostio_video_setup *setup) {
  // I copied this from Full Moon, which had alternatives to opengl, then removed all the alternatives.
  // So this 3-step process seems a bit silly now:
  if (glx_init_start(driver,setup)<0) return -1;
  if (glx_init_opengl(driver,setup)<0) return -1;
  if (glx_init_finish(driver,setup)<0) return -1;
  return 0;
}

/* Fullscreen.
 */

static void _glx_set_fullscreen(struct hostio_video *driver,int state) {
  state=state?1:0;
  if (state==driver->fullscreen) return;
  XEvent evt={
    .xclient={
      .type=ClientMessage,
      .message_type=DRIVER->atom__NET_WM_STATE,
      .send_event=1,
      .format=32,
      .window=DRIVER->win,
      .data={.l={
        state,
        DRIVER->atom__NET_WM_STATE_FULLSCREEN,
      }},
    }
  };
  XSendEvent(DRIVER->dpy,RootWindow(DRIVER->dpy,DRIVER->screen),0,SubstructureNotifyMask|SubstructureRedirectMask,&evt);
  XFlush(DRIVER->dpy);
  driver->fullscreen=state;
}

/* Screensaver.
 */
 
static void _glx_suppress_screensaver(struct hostio_video *driver) {
  if (DRIVER->screensaver_suppressed) return;
  XForceScreenSaver(DRIVER->dpy,ScreenSaverReset);
  DRIVER->screensaver_suppressed=1;
}

/* GX frame control.
 */

static int _glx_begin_frame(struct hostio_video *driver) {
  DRIVER->screensaver_suppressed=0;
  glXMakeCurrent(DRIVER->dpy,DRIVER->win,DRIVER->ctx);
  glViewport(0,0,driver->w,driver->h);
  return 0;
}

static int _glx_end_frame(struct hostio_video *driver) {
  glXSwapBuffers(DRIVER->dpy,DRIVER->win);
  return 0;
}

/* Cursor.
 */
 
void glx_show_cursor(struct hostio_video *driver,int show) {
  if (show) {
    if (DRIVER->cursor_visible) return;
    DRIVER->cursor_visible=1;
    XDefineCursor(DRIVER->dpy,DRIVER->win,0);
  } else {
    if (!DRIVER->cursor_visible) return;
    DRIVER->cursor_visible=0;
    XColor color;
    Pixmap pixmap=XCreateBitmapFromData(DRIVER->dpy,DRIVER->win,"\0\0\0\0\0\0\0\0",1,1);
    Cursor cursor=XCreatePixmapCursor(DRIVER->dpy,pixmap,pixmap,&color,&color,0,0);
    XDefineCursor(DRIVER->dpy,DRIVER->win,cursor);
    XFreeCursor(DRIVER->dpy,cursor);
    XFreePixmap(DRIVER->dpy,pixmap);
  }
}

/* Type definition.
 */
 
const struct hostio_video_type hostio_video_type_glx={
  .name="glx",
  .desc="X11 and OpenGL for Linux. Recommended.",
  .objlen=sizeof(struct hostio_video_glx),
  .appointment_only=0,
  .provides_keyboard=1,
  .del=_glx_del,
  .init=_glx_init,
  .begin_frame=_glx_begin_frame,
  .end_frame=_glx_end_frame,
  .update=glx_update,
  .set_fullscreen=_glx_set_fullscreen,
  .suppress_screensaver=_glx_suppress_screensaver,
  .show_cursor=glx_show_cursor,
};
