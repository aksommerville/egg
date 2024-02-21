#include "glx_internal.h"

/* Key press, release, or repeat.
 */
 
static int glx_evt_key(struct hostio_video *driver,XKeyEvent *evt,int value) {

  /* Pass the raw keystroke. */
  if (driver->delegate.cb_key) {
    KeySym keysym=XkbKeycodeToKeysym(DRIVER->dpy,evt->keycode,0,0);
    if (keysym) {
      int keycode=glx_usb_usage_from_keysym((int)keysym);
      if (keycode) {
        int err=driver->delegate.cb_key(driver->delegate.userdata,keycode,value);
        if (err) return err; // Stop here if acknowledged.
      }
    }
  }
  
  /* Pass text if press or repeat, and text can be acquired. */
  if (driver->delegate.cb_char) {
    int shift=(evt->state&ShiftMask)?1:0;
    KeySym tkeysym=XkbKeycodeToKeysym(DRIVER->dpy,evt->keycode,0,shift);
    if (shift&&!tkeysym) { // If pressing shift makes this key "not a key anymore", fuck that and pretend shift is off
      tkeysym=XkbKeycodeToKeysym(DRIVER->dpy,evt->keycode,0,0);
    }
    if (tkeysym) {
      int codepoint=glx_codepoint_from_keysym(tkeysym);
      if (codepoint && (evt->type == KeyPress || evt->type == KeyRepeat)) {
        driver->delegate.cb_char(driver->delegate.userdata,codepoint);
      }
    }
  }
  
  return 0;
}

/* Mouse events.
 */
 
static int glx_evt_mbtn(struct hostio_video *driver,XButtonEvent *evt,int value) {
  
  // I swear X11 used to automatically report the wheel as (6,7) while shift held, and (4,5) otherwise.
  // After switching to GNOME 3, seems it is only ever (4,5).
  #define SHIFTABLE(v) (evt->state&ShiftMask)?v:0,(evt->state&ShiftMask)?0:v
  
  switch (evt->button) {
    case 1: if (driver->delegate.cb_mbutton) driver->delegate.cb_mbutton(driver->delegate.userdata,1,value); break;
    case 2: if (driver->delegate.cb_mbutton) driver->delegate.cb_mbutton(driver->delegate.userdata,3,value); break;
    case 3: if (driver->delegate.cb_mbutton) driver->delegate.cb_mbutton(driver->delegate.userdata,2,value); break;
    case 4: if (value&&driver->delegate.cb_mwheel) driver->delegate.cb_mwheel(driver->delegate.userdata,SHIFTABLE(-1)); break;
    case 5: if (value&&driver->delegate.cb_mwheel) driver->delegate.cb_mwheel(driver->delegate.userdata,SHIFTABLE(1)); break;
    case 6: if (value&&driver->delegate.cb_mwheel) driver->delegate.cb_mwheel(driver->delegate.userdata,-1,0); break;
    case 7: if (value&&driver->delegate.cb_mwheel) driver->delegate.cb_mwheel(driver->delegate.userdata,1,0); break;
  }
  #undef SHIFTABLE
  return 0;
}

static int glx_evt_mmotion(struct hostio_video *driver,XMotionEvent *evt) {
  if (driver->delegate.cb_mmotion) {
    driver->delegate.cb_mmotion(driver->delegate.userdata,evt->x,evt->y);
  }
  return 0;
}

/* Client message.
 */
 
static int glx_evt_client(struct hostio_video *driver,XClientMessageEvent *evt) {
  if (evt->message_type==DRIVER->atom_WM_PROTOCOLS) {
    if (evt->format==32) {
      if (evt->data.l[0]==DRIVER->atom_WM_DELETE_WINDOW) {
        if (driver->delegate.cb_close) {
          driver->delegate.cb_close(driver->delegate.userdata);
        }
      }
    }
  }
  return 0;
}

/* Configuration event (eg resize).
 */
 
static int glx_evt_configure(struct hostio_video *driver,XConfigureEvent *evt) {
  int nw=evt->width,nh=evt->height;
  if ((nw!=driver->w)||(nh!=driver->h)) {
    driver->w=nw;
    driver->h=nh;
    if (driver->delegate.cb_resize) {
      driver->delegate.cb_resize(driver->delegate.userdata,nw,nh);
    }
  }
  return 0;
}

/* Focus.
 */
 
static int glx_evt_focus(struct hostio_video *driver,XFocusInEvent *evt,int value) {
  if (value==DRIVER->focus) return 0;
  DRIVER->focus=value;
  if (driver->delegate.cb_focus) {
    driver->delegate.cb_focus(driver->delegate.userdata,value);
  }
  return 0;
}

/* Process one event.
 */
 
static int glx_receive_event(struct hostio_video *driver,XEvent *evt) {
  if (!evt) return -1;
  switch (evt->type) {
  
    case KeyPress: return glx_evt_key(driver,&evt->xkey,1);
    case KeyRelease: return glx_evt_key(driver,&evt->xkey,0);
    case KeyRepeat: return glx_evt_key(driver,&evt->xkey,2);
    
    case ButtonPress: return glx_evt_mbtn(driver,&evt->xbutton,1);
    case ButtonRelease: return glx_evt_mbtn(driver,&evt->xbutton,0);
    case MotionNotify: return glx_evt_mmotion(driver,&evt->xmotion);
    
    case ClientMessage: return glx_evt_client(driver,&evt->xclient);
    
    case ConfigureNotify: return glx_evt_configure(driver,&evt->xconfigure);
    
    case FocusIn: return glx_evt_focus(driver,&evt->xfocus,1);
    case FocusOut: return glx_evt_focus(driver,&evt->xfocus,0);
    
  }
  return 0;
}

/* Update.
 */
 
int glx_update(struct hostio_video *driver) {
  int evtc=XEventsQueued(DRIVER->dpy,QueuedAfterFlush);
  while (evtc-->0) {
    XEvent evt={0};
    XNextEvent(DRIVER->dpy,&evt);
    if ((evtc>0)&&(evt.type==KeyRelease)) {
      XEvent next={0};
      XNextEvent(DRIVER->dpy,&next);
      evtc--;
      if ((next.type==KeyPress)&&(evt.xkey.keycode==next.xkey.keycode)&&(evt.xkey.time>=next.xkey.time-GLX_KEY_REPEAT_INTERVAL)) {
        evt.type=KeyRepeat;
        if (glx_receive_event(driver,&evt)<0) return -1;
      } else {
        if (glx_receive_event(driver,&evt)<0) return -1;
        if (glx_receive_event(driver,&next)<0) return -1;
      }
    } else {
      if (glx_receive_event(driver,&evt)<0) return -1;
    }
  }
  return 0;
}
