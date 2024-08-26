#include "mswm_internal.h"

struct hostio_video *mswm_global_driver=0;

/* Delete.
 */

static void _mswm_del(struct hostio_video *driver) {
  
  if (driver==mswm_global_driver) mswm_global_driver=0;

  glDeleteTextures(1,&DRIVER->texid);

  if (DRIVER->hglrc) {
    wglMakeCurrent(DRIVER->hdc,0);
    wglDeleteContext(DRIVER->hglrc);
  }
  if (DRIVER->hdc) {
    DeleteDC(DRIVER->hdc);
  }
  if (DRIVER->hwnd) {
    DestroyWindow(DRIVER->hwnd);
  }

  if (DRIVER->wndclass_atom) {
    UnregisterClass(MSWM_WINDOW_CLASS_NAME,DRIVER->instance);
  }
  
}

/* Setup window class.
 */

static int mswm_populate_wndclass(struct hostio_video *driver) {
  DRIVER->wndclass.cbSize=sizeof(WNDCLASSEX);
  DRIVER->wndclass.style=CS_HREDRAW|CS_VREDRAW|CS_OWNDC;
  DRIVER->wndclass.lpfnWndProc=(WNDPROC)mswm_cb_msg;
  DRIVER->wndclass.hInstance=DRIVER->instance;
  DRIVER->wndclass.lpszClassName=MSWM_WINDOW_CLASS_NAME;
  return 0;
}

static int mswm_register_wndclass(struct hostio_video *driver) {
  DRIVER->wndclass_atom=RegisterClassEx(&DRIVER->wndclass);
  if (!DRIVER->wndclass_atom) {
    int errcode=GetLastError();
    fprintf(stderr,"RegisterClassEx(\"%s\"): Error %d\n",MSWM_WINDOW_CLASS_NAME,errcode);
    return -1;
  }
  return 0;
}

/* Init.
 */

static int _mswm_init(struct hostio_video *driver,const struct hostio_video_setup *config) {
  if (mswm_global_driver) return -1;
  mswm_global_driver=driver;
  
  DRIVER->translate_events=1;
  DRIVER->dstdirty=1;

  if (!(DRIVER->instance=GetModuleHandle(0))) return -1;
  
  if (mswm_populate_wndclass(driver)<0) return -1;
  if (mswm_register_wndclass(driver)<0) return -1;
  
  int w=0,h=0;
  if (config) {
    if ((config->w>0)&&(config->h>0)) {
      w=config->w;
      h=config->h;
    } else if ((config->fbw>0)&&(config->fbh>0)) {
      w=config->fbw;
      h=config->fbh;
    }
  }
  if (!w&&!h) {
    w=640;
    h=360;
  } else {
    if (w<100) w=100;
    else if (w>2000) w=2000;
    if (h<100) h=100;
    else if (h>2000) h=2000;
  }

  /* CreateWindow() takes the outer bounds, including the frame.
   * (w,h) provided to us are the desired inner bounds, ie the part that we control.
   */
  RECT bounds={
    .left=0,
    .top=0,
    .right=w,
    .bottom=h,
  };
  AdjustWindowRect(&bounds,WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN|WS_CLIPSIBLINGS,0);
  int outerw=bounds.right-bounds.left;
  int outerh=bounds.bottom-bounds.top;
  driver->w=w;
  driver->h=h;
  int x=CW_USEDEFAULT,y=0;
  
  DRIVER->hwnd=CreateWindow(
    MSWM_WINDOW_CLASS_NAME,
    (config&&config->title)?config->title:"",
    (0&&config&&config->fullscreen)?(WS_POPUP):(WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN|WS_CLIPSIBLINGS),
    x,y,outerw,outerh,
    0,0,DRIVER->instance,0
  );
  if (!DRIVER->hwnd) {
    fprintf(stderr,"Failed to create window.\n");
    return -1;
  }

  /*TODO use the RGBA icon provided in (config).
   * If if that's really not possible, use our "appicon.ico" somehow.
   * The following code is held over from Full Moon.
  HANDLE hIcon=LoadImage(0,"icon.ico",IMAGE_ICON,0,0,LR_DEFAULTSIZE|LR_LOADFROMFILE);
  if (hIcon) {
    SendMessage(DRIVER->hwnd,WM_SETICON,ICON_SMALL,(LPARAM)hIcon);
    SendMessage(DRIVER->hwnd,WM_SETICON,ICON_BIG,(LPARAM)hIcon);
    SendMessage(GetWindow(DRIVER->hwnd,GW_OWNER),WM_SETICON,ICON_SMALL,(LPARAM)hIcon);
    SendMessage(GetWindow(DRIVER->hwnd,GW_OWNER),WM_SETICON,ICON_BIG,(LPARAM)hIcon);
  }
  /**/
  
  if (0&&config&&config->fullscreen) {
    driver->fullscreen=1;
    ShowWindow(DRIVER->hwnd,SW_MAXIMIZE);
  } else {
    ShowWindow(DRIVER->hwnd,SW_SHOW);
  }
  
  UpdateWindow(DRIVER->hwnd);

  FlashWindow(DRIVER->hwnd,0);

  ShowCursor(0);
  
  return 0;
}

/* Frame control.
 */
 
static int _mswm_begin_frame(struct hostio_video *driver) {
  return 0;
}

static int _mswm_end_frame(struct hostio_video *driver) {
  glFlush();
  SwapBuffers(DRIVER->hdc);
  return 0;
}

/* Fullscreen.
 */

static void mswm_enter_fullscreen(struct hostio_video *driver) {
  DRIVER->fsrestore.length=sizeof(WINDOWPLACEMENT);
  GetWindowPlacement(DRIVER->hwnd,&DRIVER->fsrestore);

  int fullscreenWidth=GetDeviceCaps(DRIVER->hdc,DESKTOPHORZRES);
  int fullscreenHeight=GetDeviceCaps(DRIVER->hdc,DESKTOPVERTRES);

  SetWindowLongPtr(DRIVER->hwnd,GWL_EXSTYLE,WS_EX_APPWINDOW|WS_EX_TOPMOST);
  SetWindowLongPtr(DRIVER->hwnd,GWL_STYLE,WS_POPUP|WS_VISIBLE);
  SetWindowPos(DRIVER->hwnd,HWND_TOPMOST,0,0,fullscreenWidth,fullscreenHeight,SWP_SHOWWINDOW);
  ShowWindow(DRIVER->hwnd,SW_MAXIMIZE);
  driver->fullscreen=1;
}

static void mswm_exit_fullscreen(struct hostio_video *driver) {
  SetWindowLong(DRIVER->hwnd,GWL_STYLE,WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN|WS_CLIPSIBLINGS);

  /* If we started up in fullscreen mode, we don't know the appropriate size to restore to.
   * Make something up.
   */
  if (!DRIVER->fsrestore.length) {
    DRIVER->fsrestore.length=sizeof(WINDOWPLACEMENT);
    DRIVER->fsrestore.flags=WPF_SETMINPOSITION;
    DRIVER->fsrestore.showCmd=SW_SHOW;
    DRIVER->fsrestore.rcNormalPosition.left=100;
    DRIVER->fsrestore.rcNormalPosition.top=100;
    DRIVER->fsrestore.rcNormalPosition.right=100+640;
    DRIVER->fsrestore.rcNormalPosition.bottom=100+360;
    AdjustWindowRect(&DRIVER->fsrestore.rcNormalPosition,WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN|WS_CLIPSIBLINGS,0);
  }

  SetWindowLongPtr(DRIVER->hwnd,GWL_EXSTYLE,WS_EX_LEFT);
  SetWindowLongPtr(DRIVER->hwnd,GWL_STYLE,WS_OVERLAPPEDWINDOW|WS_VISIBLE);
  SetWindowPlacement(DRIVER->hwnd,&DRIVER->fsrestore);
  SetWindowPos(DRIVER->hwnd,HWND_NOTOPMOST,0,0,0,0,SWP_NOSIZE|SWP_NOMOVE|SWP_FRAMECHANGED);
  ShowWindow(DRIVER->hwnd,SW_RESTORE);

  driver->fullscreen=0;
}
 
static void _mswm_set_fullscreen(struct hostio_video *driver,int fullscreen) {
  if (!DRIVER->window_setup_complete) return;
  if (fullscreen) {
    if (driver->fullscreen) return;
    mswm_enter_fullscreen(driver);
  } else {
    if (!driver->fullscreen) return;
    mswm_exit_fullscreen(driver);
  }
  mswm_autorelease(driver);
}

/* Type definition.
 */
 
const struct hostio_video_type hostio_video_type_mswm={
  .name="mswm",
  .desc="Video for Windows",
  .objlen=sizeof(struct hostio_video_mswm),
  .provides_input=1,
  .del=_mswm_del,
  .init=_mswm_init,
  .update=mswm_update,
  .show_cursor=0,//TODO
  .set_fullscreen=_mswm_set_fullscreen,
  .gx_begin=_mswm_begin_frame,
  .gx_end=_mswm_end_frame,
};

/* Extra support for friend classes.
 */
 
HWND mswm_get_window_handle() {
  struct hostio_video *driver=mswm_global_driver;
  if (!driver) return 0;
  return DRIVER->hwnd;
}
