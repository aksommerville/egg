#include "mswm_internal.h"

/* Window resized.
 */

static int mswm_cb_resize(struct hostio_video *driver,int hint,int w,int h) {
  driver->w=DRIVER->winw=w;
  driver->h=DRIVER->winh=h;
  DRIVER->dstdirty=1;
  if (driver->delegate.cb_resize) driver->delegate.cb_resize(driver->delegate.userdata,w,h);
  return 0;
}

/* Activate.
 */

static int mswm_cb_activate(struct hostio_video *driver,int state) {
  //ps_log(MSWM,TRACE,"%s %d",__func__,state);
  //TODO Should we do something on window focus, like pause the game?
  return 0;
}

/* Keyboard.
 */

static int mswm_evt_key(struct hostio_video *driver,WPARAM wparam,LPARAM lparam) {
  if (!driver->delegate.cb_key) return 0;
  int value;
  int usage=mswm_usage_from_keysym(wparam);
  if (lparam&0x80000000) { // up
    value=0;
    mswm_autorelease_remove(driver,usage);
  } else if (lparam&0x40000000) { // repeat
    value=2;
  } else { // down
    value=1;
    mswm_autorelease_add(driver,usage);
  }
  driver->delegate.cb_key(driver->delegate.userdata,usage,value);
  return 0;
}

static int mswm_evt_char(struct hostio_video *driver,int codepoint,int lparam) {
  // I don't think we can use this, because we aren't checking the key event that initiated it (i mean, they aren't correlated).
  // Luckily, we don't actually use keyboard text input, ever.
  return 0;
  int value;
  if (lparam&0x80000000) return 0;
  if (lparam&0x40000000) {
    value=2;
  } else {
    value=1;
  }
  //if (ps_input_event_key(0,codepoint,value)<0) return -1;
  return 0;
}

/* Mouse.
 */

static int mswm_evt_mbtn(struct hostio_video *driver,int btnid,int state) {
  if (driver->delegate.cb_mbutton) driver->delegate.cb_mbutton(driver->delegate.userdata,btnid,state);
  return 0;
}

static int mswm_evt_mwheel(struct hostio_video *driver,int wparam,int lparam,int horz) {
  if (!driver->delegate.cb_mwheel) return 0;
  int16_t d=(wparam>>16)/WHEEL_DELTA;
  d=-d; // Positive is away from the user, ie up. That's backwards to my preference.
  if (wparam&MK_SHIFT) { // Use SHIFT key to toggle axis.
    horz=!horz;
  }
  if (horz) {
    driver->delegate.cb_mwheel(driver->delegate.userdata,d,0);
  } else {
    driver->delegate.cb_mwheel(driver->delegate.userdata,0,d);
  }
  return 0;
}

static int mswm_evt_mmove(struct hostio_video *driver,int x,int y) {
  if (driver->delegate.cb_mmotion) driver->delegate.cb_mmotion(driver->delegate.userdata,x,y);
  return 0;
}

/* Receive event.
 */

LRESULT mswm_cb_msg(HWND hwnd,UINT msg,WPARAM wparam,LPARAM lparam) {
  //fprintf(stderr,"%s %d %d %d\n",__func__,msg,wparam,lparam);
  struct hostio_video *driver=mswm_global_driver;
  if (!driver) return DefWindowProc(hwnd,msg,wparam,lparam);
  switch (msg) {
  
    case WM_CREATE: {
        if (!hwnd) return -1;
        DRIVER->hwnd=hwnd;
        if (mswm_setup_window(driver)<0) return -1;
      } return 0;
      
    case WM_DESTROY: {
        if (driver->delegate.cb_close) driver->delegate.cb_close(driver->delegate.userdata);
      } return 0;

    /* Hook into these for "are you sure?" dialogue. */
    case WM_QUIT:
    case WM_CLOSE: break;

    case WM_SIZE: {
        if (hwnd!=DRIVER->hwnd) return 0;
        if (mswm_cb_resize(driver,wparam,LOWORD(lparam),HIWORD(lparam))<0) return -1;
      } return 0;

    // Comments on MSDN suggest we should check both WM_ACTIVATEAPP and WM_NCACTIVATE:
    case WM_ACTIVATEAPP:
    case WM_NCACTIVATE: {
        if (hwnd!=DRIVER->hwnd) return 0;
        if (mswm_cb_activate(driver,wparam)<0) return -1;
      } return 0;

    case WM_CHAR: {
        if (mswm_evt_char(driver,wparam,lparam)<0) return -1;
      } return 0;

    case WM_SYSKEYDOWN:
    case WM_SYSKEYUP: break;
    case WM_KEYDOWN:
    case WM_KEYUP: {
        return mswm_evt_key(driver,wparam,lparam);
      }

    case WM_LBUTTONDOWN: return mswm_evt_mbtn(driver,1,1);
    case WM_LBUTTONUP: return mswm_evt_mbtn(driver,1,0);
    case WM_MBUTTONDOWN: return mswm_evt_mbtn(driver,2,1);
    case WM_MBUTTONUP: return mswm_evt_mbtn(driver,2,0);
    case WM_RBUTTONDOWN: return mswm_evt_mbtn(driver,3,1);
    case WM_RBUTTONUP: return mswm_evt_mbtn(driver,3,0);

    // WM_MOUSEHWHEEL exists but my 2-wheel mouse doesn't report it. In fact there is no difference between the two wheels.
    case WM_MOUSEHWHEEL: return mswm_evt_mwheel(driver,wparam,lparam,1);
    case WM_MOUSEWHEEL: return mswm_evt_mwheel(driver,wparam,lparam,0);

    case WM_MOUSEMOVE: return mswm_evt_mmove(driver,LOWORD(lparam),HIWORD(lparam));

    // WM_INPUT_DEVICE_CHANGE appears to be the right way to detect connect/disconnect.
    // But it doesn't fire on Wine, so I'm sticking with WM_DEVICECHANGE and poll every time.
    //case WM_INPUT_DEVICE_CHANGE: fprintf(stderr,"WM_INPUT_DEVICE_CHANGE 0x%08x 0x%08x\n",wparam,lparam); return 0;
    case WM_INPUT: {
        //fprintf(stderr,"%s:%d WM_INPUT\n",__FILE__,__LINE__);
        mshid_event(wparam,lparam);
      } return 0;
    case WM_DEVICECHANGE: {
        //fprintf(stderr,"%s:%d WM_DEVICECHANGE\n",__FILE__,__LINE__);
        if (wparam==7) { // DBT_DEVNODES_CHANGED
          mshid_poll_connections_later();
        }
      } break;

    case WM_SETCURSOR: if (LOWORD(lparam)==HTCLIENT) {
        SetCursor(0);
        return 1;
      } else {
        ShowCursor(1);
      } break;
  }
  return DefWindowProc(hwnd,msg,wparam,lparam);
}

/* Update.
 */
 
int mswm_update(struct hostio_video *driver) {
  int msgc=0;
  MSG msg={0};
  while (PeekMessage(&msg,0,0,0,PM_REMOVE)) {
    if (!msg.hwnd) {
      mswm_cb_msg(msg.hwnd,msg.message,msg.wParam,msg.lParam);
    } else {
      if (DRIVER->translate_events) {
        TranslateMessage(&msg);
      }
      DispatchMessage(&msg);
    }
    msgc++;
  }
  return msgc;
}

/* Autorelease for held keys.
 */
 
void mswm_autorelease_add(struct hostio_video *driver,int usage) {
  if (!driver->delegate.cb_key) return;
  int available=-1,i=MSWM_AUTORELEASE_LIMIT;
  while (i-->0) {
    if (DRIVER->autorelease[i]==usage) return;
    if (!DRIVER->autorelease[i]) available=i;
  }
  if (available>=0) DRIVER->autorelease[available]=usage;
}

void mswm_autorelease_remove(struct hostio_video *driver,int usage) {
  int i=MSWM_AUTORELEASE_LIMIT;
  while (i-->0) {
    if (DRIVER->autorelease[i]==usage) DRIVER->autorelease[i]=0;
  }
}

void mswm_autorelease(struct hostio_video *driver) {
  int i=MSWM_AUTORELEASE_LIMIT;
  while (i-->0) {
    int usage=DRIVER->autorelease[i];
    if (!usage) continue;
    DRIVER->autorelease[i]=0;
    driver->delegate.cb_key(driver->delegate.userdata,usage,0);
  }
}
