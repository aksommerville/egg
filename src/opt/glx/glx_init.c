#include "glx_internal.h"

/* Initialize display and atoms.
 */
 
static int glx_init_display(struct hostio_video *driver,const struct hostio_video_setup *setup) {
  
  if (!(DRIVER->dpy=XOpenDisplay(setup->device))) return -1;
  DRIVER->screen=DefaultScreen(DRIVER->dpy);

  #define GETATOM(tag) DRIVER->atom_##tag=XInternAtom(DRIVER->dpy,#tag,0);
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
  
  return 0;
}

/* Get the size of the monitor we're going to display on.
 * NOT the size of the logical desktop, if it can be avoided.
 * We don't actually know which monitor will be chosen, and we don't want to force it, so use the smallest.
 */
 
static void glx_estimate_monitor_size(int *w,int *h,const struct hostio_video *driver) {
  *w=*h=0;
  #if USE_xinerama
    int infoc=0;
    XineramaScreenInfo *infov=XineramaQueryScreens(DRIVER->dpy,&infoc);
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
    *w=DisplayWidth(DRIVER->dpy,0);
    *h=DisplayHeight(DRIVER->dpy,0);
    if ((*w<1)||(*h<1)) {
      *w=640;
      *h=480;
    }
  }
}

/* Init, first stage.
 */
 
int glx_init_start(struct hostio_video *driver,const struct hostio_video_setup *setup) {

  if (glx_init_display(driver,setup)<0) return -1;
  
  driver->rate=60;
  if ((setup->ratemin>0)&&(driver->rate<setup->ratemin)) driver->rate=setup->ratemin;
  if ((setup->ratemax>0)&&(driver->rate>setup->ratemax)) driver->rate=setup->ratemax;

  if ((setup->w>0)&&(setup->h>0)) {
    driver->w=setup->w;
    driver->h=setup->h;
  } else {
    int fbw=640,fbh=360;
    if ((setup->fbw>0)&&(setup->fbh>0)) {
      fbw=setup->fbw;
      fbh=setup->fbh;
    }
    int monw=0,monh=0;
    glx_estimate_monitor_size(&monw,&monh,driver);
    monw=(monw*7)>>3; // pretend the screen is slightly smaller than it is; don't take the whole thing when windowed
    monh=(monh*7)>>3;
    int xscale=monw/fbw;
    int yscale=monh/fbh;
    int scale=(xscale<yscale)?xscale:yscale;
    if (scale<1) scale=1;
    driver->w=fbw*scale;
    driver->h=fbh*scale;
  }
  
  return 0;
}

/* Initialize window with GX.
 */
 
static GLXFBConfig glx_get_fbconfig(struct hostio_video *driver) {

  int attrv[]={
    GLX_X_RENDERABLE,1,
    GLX_DRAWABLE_TYPE,GLX_WINDOW_BIT,
    GLX_RENDER_TYPE,GLX_RGBA_BIT,
    GLX_X_VISUAL_TYPE,GLX_TRUE_COLOR,
    GLX_RED_SIZE,8,
    GLX_GREEN_SIZE,8,
    GLX_BLUE_SIZE,8,
    GLX_ALPHA_SIZE,0,
    GLX_DEPTH_SIZE,0,
    GLX_STENCIL_SIZE,0,
    GLX_DOUBLEBUFFER,1,
  0};
  
  if (!glXQueryVersion(DRIVER->dpy,&DRIVER->glx_version_major,&DRIVER->glx_version_minor)) {
    return (GLXFBConfig){0};
  }
  
  int fbc=0;
  GLXFBConfig *configv=glXChooseFBConfig(DRIVER->dpy,DRIVER->screen,attrv,&fbc);
  if (!configv||(fbc<1)) return (GLXFBConfig){0};
  GLXFBConfig config=configv[0];
  XFree(configv);
  
  return config;
}
 
static int glx_init_window_gx_inner(struct hostio_video *driver,const struct hostio_video_setup *setup,XVisualInfo *vi) {
  
  XSetWindowAttributes wattr={
    .event_mask=
      StructureNotifyMask|
      KeyPressMask|KeyReleaseMask|
      FocusChangeMask|
      ButtonPressMask|ButtonReleaseMask|
      PointerMotionMask|
    0,
  };
  wattr.colormap=XCreateColormap(DRIVER->dpy,RootWindow(DRIVER->dpy,vi->screen),vi->visual,AllocNone);
  
  if (!(DRIVER->win=XCreateWindow(
    DRIVER->dpy,RootWindow(DRIVER->dpy,DRIVER->screen),
    0,0,driver->w,driver->h,0,
    vi->depth,InputOutput,vi->visual,
    CWBorderPixel|CWColormap|CWEventMask,&wattr
  ))) return -1;
  
  return 0;
}
 
static int glx_init_window_gx(struct hostio_video *driver,const struct hostio_video_setup *setup) {
  GLXFBConfig fbconfig=glx_get_fbconfig(driver);
  XVisualInfo *vi=glXGetVisualFromFBConfig(DRIVER->dpy,fbconfig);
  if (!vi) return -1;
  int err=glx_init_window_gx_inner(driver,setup,vi);
  XFree(vi);
  if (err<0) return -1;
  if (!(DRIVER->ctx=glXCreateNewContext(DRIVER->dpy,fbconfig,GLX_RGBA_TYPE,0,1))) return -1;
  glXMakeCurrent(DRIVER->dpy,DRIVER->win,DRIVER->ctx);
  return 0;
}

/* Init public OpenGL context.
 */
 
int glx_init_opengl(struct hostio_video *driver,const struct hostio_video_setup *setup) {
  if (glx_init_window_gx(driver,setup)<0) return -1;
  return 0;
}

/* Set window title.
 */
 
static void glx_set_title(struct hostio_video *driver,const char *src) {
  
  // I've seen these properties in GNOME 2, unclear whether they might still be in play:
  XTextProperty prop={.value=(void*)src,.encoding=DRIVER->atom_STRING,.format=8,.nitems=0};
  while (prop.value[prop.nitems]) prop.nitems++;
  XSetWMName(DRIVER->dpy,DRIVER->win,&prop);
  XSetWMIconName(DRIVER->dpy,DRIVER->win,&prop);
  XSetTextProperty(DRIVER->dpy,DRIVER->win,&prop,DRIVER->atom__NET_WM_ICON_NAME);
    
  // This one becomes the window title and bottom-bar label, in GNOME 3:
  prop.encoding=DRIVER->atom_UTF8_STRING;
  XSetTextProperty(DRIVER->dpy,DRIVER->win,&prop,DRIVER->atom__NET_WM_NAME);
    
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
      prop.encoding=DRIVER->atom_STRING;
      XSetTextProperty(DRIVER->dpy,DRIVER->win,&prop,DRIVER->atom_WM_CLASS);
    }
  }
}

/* Set window icon.
 */
 
static void glx_copy_icon_pixels(long *dst,const uint8_t *src,int c) {
  for (;c-->0;dst++,src+=4) {
    *dst=(src[3]<<24)|(src[0]<<16)|(src[1]<<8)|src[2];
  }
}
 
static void glx_set_icon(struct hostio_video *driver,const void *rgba,int w,int h) {
  if (!rgba||(w<1)||(h<1)||(w>512)||(h>512)) return;
  int length=2+w*h;
  long *pixels=malloc(sizeof(long)*length);
  if (!pixels) return;
  pixels[0]=w;
  pixels[1]=h;
  glx_copy_icon_pixels(pixels+2,rgba,w*h);
  XChangeProperty(DRIVER->dpy,DRIVER->win,DRIVER->atom__NET_WM_ICON,XA_CARDINAL,32,PropModeReplace,(unsigned char*)pixels,length);
  free(pixels);
}

/* Init, last stage.
 */

int glx_init_finish(struct hostio_video *driver,const struct hostio_video_setup *setup) {

  if (setup->title&&setup->title[0]) {
    glx_set_title(driver,setup->title);
  }
  
  if (setup->iconrgba&&(setup->iconw>0)&&(setup->iconh>0)) {
    glx_set_icon(driver,setup->iconrgba,setup->iconw,setup->iconh);
  }
  
  XMapWindow(DRIVER->dpy,DRIVER->win);
  XSync(DRIVER->dpy,0);
  XSetWMProtocols(DRIVER->dpy,DRIVER->win,&DRIVER->atom_WM_DELETE_WINDOW,1);
  
  if (setup->fullscreen) {
    XChangeProperty(
      DRIVER->dpy,DRIVER->win,
      DRIVER->atom__NET_WM_STATE,
      XA_ATOM,32,PropModeReplace,
      (unsigned char*)&DRIVER->atom__NET_WM_STATE_FULLSCREEN,1
    );
    driver->fullscreen=1;
  }
  
  DRIVER->cursor_visible=1;
  glx_show_cursor(driver,0);

  return 0;
}
