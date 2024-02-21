#ifndef GLX_INTERNAL_H
#define GLX_INTERNAL_H

#include "opt/hostio/hostio_video.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <endian.h>
#include <limits.h>
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/XKBlib.h>
#include <X11/keysym.h>
#include <GL/glx.h>
#include <GL/gl.h>

// Required only for making intelligent initial-size decisions in a multi-monitor setting.
// apt install libxinerama-dev
#if USE_xinerama
  #include <X11/extensions/Xinerama.h>
#endif

#define KeyRepeat (LASTEvent+2)
#define GLX_KEY_REPEAT_INTERVAL 10
#define GLX_ICON_SIZE_LIMIT 64

struct hostio_video_glx {
  struct hostio_video hdr;
  
  Display *dpy;
  int screen;
  Window win;

  GLXContext ctx;
  int glx_version_minor,glx_version_major;
  
  Atom atom_WM_PROTOCOLS;
  Atom atom_WM_DELETE_WINDOW;
  Atom atom__NET_WM_STATE;
  Atom atom__NET_WM_STATE_FULLSCREEN;
  Atom atom__NET_WM_STATE_ADD;
  Atom atom__NET_WM_STATE_REMOVE;
  Atom atom__NET_WM_ICON;
  Atom atom__NET_WM_ICON_NAME;
  Atom atom__NET_WM_NAME;
  Atom atom_WM_CLASS;
  Atom atom_STRING;
  Atom atom_UTF8_STRING;
  
  int screensaver_suppressed;
  int focus;
  int cursor_visible;
};

#define DRIVER ((struct hostio_video_glx*)driver)

int glx_init_start(struct hostio_video *driver,const struct hostio_video_setup *setup);
int glx_init_opengl(struct hostio_video *driver,const struct hostio_video_setup *setup);
int glx_init_finish(struct hostio_video *driver,const struct hostio_video_setup *setup);

int glx_update(struct hostio_video *driver);

int glx_codepoint_from_keysym(int keysym);
int glx_usb_usage_from_keysym(int keysym);

void glx_show_cursor(struct hostio_video *driver,int show);

#endif
