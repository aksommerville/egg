#include "xegl_internal.h"

/* Key press, release, or repeat.
 */
 
static int xegl_evt_key(struct xegl *xegl,XKeyEvent *evt,int value) {

  /* Pass the raw keystroke. */
  if (xegl->delegate.cb_key) {
    KeySym keysym=XkbKeycodeToKeysym(xegl->dpy,evt->keycode,0,0);
    if (keysym) {
      int keycode=xegl_usb_usage_from_keysym((int)keysym);
      if (keycode) {
        int err=xegl->delegate.cb_key(xegl->delegate.userdata,keycode,value);
        if (err) return err; // Stop here if acknowledged.
      }
    }
  }
  
  /* Pass text if press or repeat, and text can be acquired. */
  if (xegl->delegate.cb_text) {
    int shift=(evt->state&ShiftMask)?1:0;
    KeySym tkeysym=XkbKeycodeToKeysym(xegl->dpy,evt->keycode,0,shift);
    if (shift&&!tkeysym) { // If pressing shift makes this key "not a key anymore", fuck that and pretend shift is off
      tkeysym=XkbKeycodeToKeysym(xegl->dpy,evt->keycode,0,0);
    }
    if (tkeysym) {
      int codepoint=xegl_codepoint_from_keysym(tkeysym);
      if (codepoint && (evt->type == KeyPress || evt->type == KeyRepeat)) {
        xegl->delegate.cb_text(xegl->delegate.userdata,codepoint);
      }
    }
  }
  
  return 0;
}

/* Mouse events.
 */
 
static int xegl_evt_mbtn(struct xegl *xegl,XButtonEvent *evt,int value) {
  
  // I swear X11 used to automatically report the wheel as (6,7) while shift held, and (4,5) otherwise.
  // After switching to GNOME 3, seems it is only ever (4,5).
  #define SHIFTABLE(v) (evt->state&ShiftMask)?v:0,(evt->state&ShiftMask)?0:v
  
  switch (evt->button) {
    case 1: if (xegl->delegate.cb_mbutton) xegl->delegate.cb_mbutton(xegl->delegate.userdata,1,value); break;
    case 2: if (xegl->delegate.cb_mbutton) xegl->delegate.cb_mbutton(xegl->delegate.userdata,3,value); break;
    case 3: if (xegl->delegate.cb_mbutton) xegl->delegate.cb_mbutton(xegl->delegate.userdata,2,value); break;
    case 4: if (value&&xegl->delegate.cb_mwheel) xegl->delegate.cb_mwheel(xegl->delegate.userdata,SHIFTABLE(-1)); break;
    case 5: if (value&&xegl->delegate.cb_mwheel) xegl->delegate.cb_mwheel(xegl->delegate.userdata,SHIFTABLE(1)); break;
    case 6: if (value&&xegl->delegate.cb_mwheel) xegl->delegate.cb_mwheel(xegl->delegate.userdata,-1,0); break;
    case 7: if (value&&xegl->delegate.cb_mwheel) xegl->delegate.cb_mwheel(xegl->delegate.userdata,1,0); break;
  }
  #undef SHIFTABLE
  return 0;
}

static int xegl_evt_mmotion(struct xegl *xegl,XMotionEvent *evt) {
  if (xegl->cursor_locked) {
    int midx=xegl->w>>1;
    int midy=xegl->h>>1;
    int dx=evt->x-midx;
    int dy=evt->y-midy;
    if (dx||dy) {
      if (xegl->delegate.cb_mmotion) {
        xegl->delegate.cb_mmotion(xegl->delegate.userdata,dx,dy);
      }
      XWarpPointer(xegl->dpy,None,xegl->win,0,0,0,0,xegl->w>>1,xegl->h>>1);
      XFlush(xegl->dpy);
    }
  } else {
    if (xegl->delegate.cb_mmotion) {
      xegl->delegate.cb_mmotion(xegl->delegate.userdata,evt->x,evt->y);
    }
  }
  return 0;
}

static int xegl_evt_mcrossing(struct xegl *xegl,XCrossingEvent *evt) {
  if (xegl->cursor_locked) return 0;
  // We'll report it as motion.
  // If the event was EnterNotify, coords must be in bounds, and if LeaveNotify they must be out of bounds.
  // And of bloody course, we don't get the same courtesy from my X server :P
  int x=evt->x,y=evt->y;
  if (evt->type==EnterNotify) {
    if (x<0) x=0; else if (x>=xegl->w) x=xegl->w-1;
    if (y<0) y=0; else if (y>=xegl->h) y=xegl->h-1;
  } else {
    if ((x>=0)&&(x<xegl->w)&&(y>=0)&&(y<xegl->h)) {
      // ...fuck's sake.
      // Knock it off the nearest edge.
      int ldist=x,udist=y,rdist=xegl->w-x,bdist=xegl->h-y;
      if ((ldist<=udist)&&(ldist<=rdist)&&(ldist<=bdist)) x=-1;
      else if ((udist<=rdist)&&(udist<=bdist)) y=-1;
      else if (rdist<=bdist) x=xegl->w;
      else y=xegl->h;
    }
  }
  if (xegl->delegate.cb_mmotion) {
    xegl->delegate.cb_mmotion(xegl->delegate.userdata,x,y);
  }
  return 0;
}

/* Client message.
 */
 
static int xegl_evt_client(struct xegl *xegl,XClientMessageEvent *evt) {
  if (evt->message_type==xegl->atom_WM_PROTOCOLS) {
    if (evt->format==32) {
      if (evt->data.l[0]==xegl->atom_WM_DELETE_WINDOW) {
        if (xegl->delegate.cb_close) {
          xegl->delegate.cb_close(xegl->delegate.userdata);
        }
      }
    }
  }
  return 0;
}

/* Configuration event (eg resize).
 */
 
static int xegl_evt_configure(struct xegl *xegl,XConfigureEvent *evt) {
  int nw=evt->width,nh=evt->height;
  if ((nw!=xegl->w)||(nh!=xegl->h)) {
    xegl->w=nw;
    xegl->h=nh;
    if (xegl->delegate.cb_resize) {
      xegl->delegate.cb_resize(xegl->delegate.userdata,nw,nh);
    }
  }
  return 0;
}

/* Focus.
 */
 
static int xegl_evt_focus(struct xegl *xegl,XFocusInEvent *evt,int value) {
  if (value==xegl->focus) return 0;
  xegl->focus=value;
  if (xegl->delegate.cb_focus) {
    xegl->delegate.cb_focus(xegl->delegate.userdata,value);
  }
  return 0;
}

/* Process one event.
 */
 
static int xegl_receive_event(struct xegl *xegl,XEvent *evt) {
  if (!evt) return -1;
  switch (evt->type) {
  
    case KeyPress: return xegl_evt_key(xegl,&evt->xkey,1);
    case KeyRelease: return xegl_evt_key(xegl,&evt->xkey,0);
    case KeyRepeat: return xegl_evt_key(xegl,&evt->xkey,2);
    
    case ButtonPress: return xegl_evt_mbtn(xegl,&evt->xbutton,1);
    case ButtonRelease: return xegl_evt_mbtn(xegl,&evt->xbutton,0);
    case MotionNotify: return xegl_evt_mmotion(xegl,&evt->xmotion);
    case EnterNotify: return xegl_evt_mcrossing(xegl,&evt->xcrossing);
    case LeaveNotify: return xegl_evt_mcrossing(xegl,&evt->xcrossing);
    
    case ClientMessage: return xegl_evt_client(xegl,&evt->xclient);
    
    case ConfigureNotify: return xegl_evt_configure(xegl,&evt->xconfigure);
    
    case FocusIn: return xegl_evt_focus(xegl,&evt->xfocus,1);
    case FocusOut: return xegl_evt_focus(xegl,&evt->xfocus,0);
    
  }
  return 0;
}

/* Update.
 */
 
int xegl_update(struct xegl *xegl) {
  int evtc=XEventsQueued(xegl->dpy,QueuedAfterFlush);
  while (evtc-->0) {
    XEvent evt={0};
    XNextEvent(xegl->dpy,&evt);
    if ((evtc>0)&&(evt.type==KeyRelease)) {
      XEvent next={0};
      XNextEvent(xegl->dpy,&next);
      evtc--;
      if ((next.type==KeyPress)&&(evt.xkey.keycode==next.xkey.keycode)&&(evt.xkey.time>=next.xkey.time-XEGL_KEY_REPEAT_INTERVAL)) {
        evt.type=KeyRepeat;
        if (xegl_receive_event(xegl,&evt)<0) return -1;
      } else {
        if (xegl_receive_event(xegl,&evt)<0) return -1;
        if (xegl_receive_event(xegl,&next)<0) return -1;
      }
    } else {
      if (xegl_receive_event(xegl,&evt)<0) return -1;
    }
  }
  return 0;
}
