#ifndef XEGL_INTERNAL_H
#define XEGL_INTERNAL_H

#include "xegl.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/XKBlib.h>
#include <X11/keysym.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

#if USE_xinerama
  #include <X11/extensions/Xinerama.h>
#endif

#define KeyRepeat (LASTEvent+2)
#define XEGL_KEY_REPEAT_INTERVAL 10
#define XEGL_ICON_SIZE_LIMIT 64

struct xegl {
  struct xegl_delegate delegate;
  
  Display *dpy;
  int screen;
  Window win;
  int w,h;
  int fullscreen;
  
  EGLDisplay egldisplay;
  EGLContext eglcontext;
  EGLSurface eglsurface;
  int version_major,version_minor;
  
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
  int cursor_locked;
};

int xegl_codepoint_from_keysym(int keysym);
int xegl_usb_usage_from_keysym(int keysym);

#endif
