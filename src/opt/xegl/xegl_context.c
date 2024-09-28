#include "xegl_internal.h"

/* Delete.
 */

void xegl_del(struct xegl *xegl) {
  if (!xegl) return;
  eglDestroyContext(xegl->egldisplay,xegl->eglcontext);
  XCloseDisplay(xegl->dpy);
  free(xegl);
}

/* Estimate monitor size.
 * With Xinerama in play, we look for the smallest monitor.
 * Otherwise we end up with the logical desktop size, which is really never what you want.
 * Never fails to return something positive, even if it's a wild guess.
 */
 
static void xegl_estimate_monitor_size(int *w,int *h,const struct xegl *xegl) {
  *w=*h=0;
  #if USE_xinerama
    int infoc=0;
    XineramaScreenInfo *infov=XineramaQueryScreens(xegl->dpy,&infoc);
    if (infov) {
      if (infoc>0) {
        *w=infov[0].width;
        *h=infov[0].height;
        int i=infoc; while (i-->1) {
          if ((infov[i].width<*w)||(infov[i].height<*h)) {
            *w=infov[i].width;
            *h=infov[i].height;
          }
        }
      }
      XFree(infov);
    }
  #endif
  if ((*w<1)||(*h<1)) {
    *w=DisplayWidth(xegl->dpy,0);
    *h=DisplayHeight(xegl->dpy,0);
    if ((*w<1)||(*h<1)) {
      *w=640;
      *h=480;
    }
  }
}

/* Set window title.
 */
 
void xegl_set_title(struct xegl *xegl,const char *src) {
  if (!src) src="";
  
  // I've seen these properties in GNOME 2, unclear whether they might still be in play:
  XTextProperty prop={.value=(void*)src,.encoding=xegl->atom_STRING,.format=8,.nitems=0};
  while (prop.value[prop.nitems]) prop.nitems++;
  XSetWMName(xegl->dpy,xegl->win,&prop);
  XSetWMIconName(xegl->dpy,xegl->win,&prop);
  XSetTextProperty(xegl->dpy,xegl->win,&prop,xegl->atom__NET_WM_ICON_NAME);
    
  // This one becomes the window title and bottom-bar label, in GNOME 3:
  prop.encoding=xegl->atom_UTF8_STRING;
  XSetTextProperty(xegl->dpy,xegl->win,&prop,xegl->atom__NET_WM_NAME);
    
  // This daffy bullshit becomes the Alt-Tab text in GNOME 3:
  {
    char tmp[256];
    int len=prop.nitems+1+prop.nitems;
    if (len<sizeof(tmp)) {
      memcpy(tmp,prop.value,prop.nitems);
      tmp[prop.nitems]=0;
      memcpy(tmp+prop.nitems+1,prop.value,prop.nitems);
      tmp[prop.nitems+1+prop.nitems]=0;
      prop.value=tmp;
      prop.nitems=prop.nitems+1+prop.nitems;
      prop.encoding=xegl->atom_STRING;
      XSetTextProperty(xegl->dpy,xegl->win,&prop,xegl->atom_WM_CLASS);
    }
  }
}

/* Set window icon.
 */
 
static void xegl_copy_icon_pixels(long *dst,const uint8_t *src,int c) {
  for (;c-->0;dst++,src+=4) {
    *dst=(src[3]<<24)|(src[0]<<16)|(src[1]<<8)|src[2];
  }
}
 
static void xegl_set_icon(struct xegl *xegl,const void *rgba,int w,int h) {
  if (!rgba||(w<1)||(h<1)||(w>XEGL_ICON_SIZE_LIMIT)||(h>XEGL_ICON_SIZE_LIMIT)) return;
  int length=2+w*h;
  long *pixels=calloc(sizeof(long),length);
  if (!pixels) return;
  pixels[0]=w;
  pixels[1]=h;
  xegl_copy_icon_pixels(pixels+2,rgba,w*h);
  XChangeProperty(xegl->dpy,xegl->win,xegl->atom__NET_WM_ICON,XA_CARDINAL,32,PropModeReplace,(unsigned char*)pixels,length);
  free(pixels);
}

/* Initialize. Display is opened, and nothing else.
 */
 
static int xegl_init(struct xegl *xegl,const struct xegl_setup *setup) {
  xegl->screen=DefaultScreen(xegl->dpy);
  
  #define GETATOM(tag) xegl->atom_##tag=XInternAtom(xegl->dpy,#tag,0);
  GETATOM(WM_PROTOCOLS)
  GETATOM(WM_DELETE_WINDOW)
  GETATOM(_NET_WM_STATE)
  GETATOM(_NET_WM_STATE_FULLSCREEN)
  GETATOM(_NET_WM_STATE_ADD)
  GETATOM(_NET_WM_STATE_REMOVE)
  GETATOM(_NET_WM_ICON)
  GETATOM(_NET_WM_ICON_NAME)
  GETATOM(_NET_WM_NAME)
  GETATOM(STRING)
  GETATOM(UTF8_STRING)
  GETATOM(WM_CLASS)
  #undef GETATOM
  
  // Choose window size.
  if ((setup->w>0)&&(setup->h>0)) {
    xegl->w=setup->w;
    xegl->h=setup->h;
  } else {
    int monw,monh;
    xegl_estimate_monitor_size(&monw,&monh,xegl);
    // There's usually some fixed chrome on both the main screen and around the window.
    // If we got a reasonable size, cut it to 3/4 to accomodate that chrome.
    if ((monw>=256)&&(monh>=256)) {
      monw=(monw*3)>>2;
      monh=(monh*3)>>2;
    }
    if ((setup->fbw>0)&&(setup->fbh>0)) {
      int xscale=monw/setup->fbw;
      int yscale=monh/setup->fbh;
      int scale=(xscale<yscale)?xscale:yscale;
      if (scale<1) scale=1;
      xegl->w=setup->fbw*scale;
      xegl->h=setup->fbh*scale;
    } else {
      xegl->w=monw;
      xegl->h=monh;
    }
  }
  
  // Create EGL context.
  EGLAttrib egldpyattr[]={
    EGL_NONE,
  };
  xegl->egldisplay=xegl->dpy; // ?
  if (!(xegl->egldisplay=eglGetPlatformDisplay(EGL_PLATFORM_X11_KHR,xegl->dpy,egldpyattr))) return -1;
  if (!eglInitialize(xegl->egldisplay,&xegl->version_major,&xegl->version_minor)) return -1;
  EGLint eglattribv[]={
    EGL_BUFFER_SIZE,     32,
    EGL_RED_SIZE,         8,
    EGL_GREEN_SIZE,       8,
    EGL_BLUE_SIZE,        8,
    EGL_ALPHA_SIZE,       0,
    EGL_DEPTH_SIZE,      EGL_DONT_CARE,
    EGL_STENCIL_SIZE,    EGL_DONT_CARE,
    EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
    EGL_SURFACE_TYPE,    EGL_WINDOW_BIT|EGL_PIXMAP_BIT,
    EGL_NONE,
  };
  EGLConfig eglconfig=0;
  int eglcfgc=0;
  if (!eglChooseConfig(xegl->egldisplay,eglattribv,&eglconfig,1,&eglcfgc)) return -1;
  if (eglcfgc<1) return -1;
  if (!eglBindAPI(EGL_OPENGL_ES_API)) return -1;
  EGLint ctxattribv[]={
    EGL_CONTEXT_MAJOR_VERSION,2,
    EGL_CONTEXT_MINOR_VERSION,0,
    EGL_NONE,
  };
  if (!(xegl->eglcontext=eglCreateContext(xegl->egldisplay,eglconfig,0,ctxattribv))) return -1;
  
  // Create window.
  XSetWindowAttributes wattr={
    .event_mask=
      StructureNotifyMask|
      KeyPressMask|KeyReleaseMask|
      FocusChangeMask|
    0,
  };
  if (xegl->delegate.cb_mmotion) {
    wattr.event_mask|=PointerMotionMask|EnterWindowMask|LeaveWindowMask;
  }
  if (xegl->delegate.cb_mbutton||xegl->delegate.cb_mwheel) {
    wattr.event_mask|=ButtonPressMask|ButtonReleaseMask;
  }
  xegl->win=XCreateWindow(
    xegl->dpy,RootWindow(xegl->dpy,xegl->screen),
    0,0,xegl->w,xegl->h,0,
    CopyFromParent,InputOutput,CopyFromParent,
    CWBorderPixel|CWEventMask,&wattr
  );
  if (!xegl->win) return -1;
  
  // Create EGL surface and context.
  EGLAttrib eglwinattr[]={
    EGL_NONE,
  };
  if (!(xegl->eglsurface=eglCreatePlatformWindowSurface(xegl->egldisplay,eglconfig,&xegl->win,eglwinattr))) return -1;
  eglMakeCurrent(xegl->egldisplay,xegl->eglsurface,xegl->eglsurface,xegl->eglcontext);
  
  // Title and icon, if provided.
  if (setup->title&&setup->title[0]) {
    xegl_set_title(xegl,setup->title);
  }
  if (setup->iconrgba&&(setup->iconw>0)&&(setup->iconh>0)) {
    xegl_set_icon(xegl,setup->iconrgba,setup->iconw,setup->iconh);
  }
  
  // Fullscreen if requested.
  if (setup->fullscreen) {
    XChangeProperty(
      xegl->dpy,xegl->win,
      xegl->atom__NET_WM_STATE,
      XA_ATOM,32,PropModeReplace,
      (unsigned char*)&xegl->atom__NET_WM_STATE_FULLSCREEN,1
    );
    xegl->fullscreen=1;
  }
  
  // Enable window.
  XMapWindow(xegl->dpy,xegl->win);
  XSync(xegl->dpy,0);
  XSetWMProtocols(xegl->dpy,xegl->win,&xegl->atom_WM_DELETE_WINDOW,1);
  
  // Hide cursor.
  xegl->cursor_visible=1; // x11's default...
  xegl_show_cursor(xegl,0); // our default
  
  return 0;
}

/* New.
 */

struct xegl *xegl_new(
  const struct xegl_delegate *delegate,
  const struct xegl_setup *setup
) {
  struct xegl *xegl=calloc(1,sizeof(struct xegl));
  if (!xegl) return 0;
  
  if (!(xegl->dpy=XOpenDisplay(setup->device))) {
    free(xegl);
    return 0;
  }
  xegl->delegate=*delegate;
  
  if (xegl_init(xegl,setup)<0) {
    xegl_del(xegl);
    return 0;
  }
  return xegl;
}

/* Begin frame.
 */

int xegl_begin(struct xegl *xegl) {
  eglMakeCurrent(xegl->egldisplay,xegl->eglsurface,xegl->eglsurface,xegl->eglcontext);
  return 0;
}

/* End frame.
 */
 
int xegl_end(struct xegl *xegl) {
  xegl->screensaver_suppressed=0;
  eglSwapBuffers(xegl->egldisplay,xegl->eglsurface);
  return 0;
}

/* Show/hide cursor.
 */

void xegl_show_cursor(struct xegl *xegl,int show) {
  if (show) {
    if (xegl->cursor_visible) return;
    XDefineCursor(xegl->dpy,xegl->win,0);
    xegl->cursor_visible=1;
  } else {
    if (!xegl->cursor_visible) return;
    XColor color;
    Pixmap pixmap=XCreateBitmapFromData(xegl->dpy,xegl->win,"\0\0\0\0\0\0\0\0",1,1);
    Cursor cursor=XCreatePixmapCursor(xegl->dpy,pixmap,pixmap,&color,&color,0,0);
    XDefineCursor(xegl->dpy,xegl->win,cursor);
    XFreeCursor(xegl->dpy,cursor);
    XFreePixmap(xegl->dpy,pixmap);
    xegl->cursor_visible=0;
  }
}

/* Lock/unlock cursor.
 */
 
void xegl_lock_cursor(struct xegl *xegl,int lock) {
  if (lock) {
    if (xegl->cursor_locked) return;
    xegl->cursor_locked=1;
    xegl_show_cursor(xegl,0);
  } else {
    if (!xegl->cursor_locked) return;
    xegl->cursor_locked=0;
  }
}

/* Fullscreen.
 */
 
static void xegl_change_fullscreen(struct xegl *xegl,int fullscreen) {
  XEvent evt={
    .xclient={
      .type=ClientMessage,
      .message_type=xegl->atom__NET_WM_STATE,
      .send_event=1,
      .format=32,
      .window=xegl->win,
      .data={.l={
        fullscreen,
        xegl->atom__NET_WM_STATE_FULLSCREEN,
      }},
    }
  };
  XSendEvent(xegl->dpy,RootWindow(xegl->dpy,xegl->screen),0,SubstructureNotifyMask|SubstructureRedirectMask,&evt);
  XFlush(xegl->dpy);
}
 
void xegl_set_fullscreen(struct xegl *xegl,int fullscreen) {
  if (fullscreen) {
    if (xegl->fullscreen) return;
    xegl_change_fullscreen(xegl,1);
    xegl->fullscreen=1;
  } else {
    if (!xegl->fullscreen) return;
    xegl_change_fullscreen(xegl,0);
    xegl->fullscreen=0;
  }
}

/* Suppress screensaver.
 */
 
void xegl_suppress_screensaver(struct xegl *xegl) {
  if (xegl->screensaver_suppressed) return;
  xegl->screensaver_suppressed=1;
  XForceScreenSaver(xegl->dpy,ScreenSaverReset);
}

/* Trivial accessors.
 */
 
void xegl_get_size(int *w,int *h,const struct xegl *xegl) {
  *w=xegl->w;
  *h=xegl->h;
}

int xegl_get_fullscreen(const struct xegl *xegl) {
  return xegl->fullscreen;
}
