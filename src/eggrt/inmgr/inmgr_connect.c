#include "inmgr_internal.h"

/* Add a button at the end of (device->buttonv), assuming (srcbtnid) are in order.
 * For keyboards.
 */
 
static void inmgr_buttonv_append_key(struct inmgr_device *device,int srcbtnid,int dstbtnid) {
  if (device->buttonc>=device->buttona) {
    int na=device->buttona+32;
    if (na>INT_MAX/sizeof(struct inmgr_button)) return;
    void *nv=realloc(device->buttonv,sizeof(struct inmgr_button)*na);
    if (!nv) return;
    device->buttonv=nv;
    device->buttona=na;
  }
  struct inmgr_button *button=device->buttonv+device->buttonc++;
  memset(button,0,sizeof(struct inmgr_button));
  button->srcbtnid=srcbtnid;
  button->hidusage=srcbtnid;
  button->lo=0;
  button->hi=1;
  button->srclo=1;
  button->srchi=INT_MAX;
  button->dstbtnid=dstbtnid;
  if (dstbtnid&0xff000000) {
    button->mode=INMGR_BUTTON_MODE_SIGNAL;
  } else if (dstbtnid) {
    button->mode=INMGR_BUTTON_MODE_TWOSTATE;
  } else {
    button->mode=INMGR_BUTTON_MODE_NOOP;
  }
}

/* Build up (device->buttonv) from (tm->tmbtnv) by assuming they all exist.
 * For keyboards with an explicit map template.
 */
 
static void inmgr_device_apply_full_template(struct inmgr_device *device,const struct inmgr_tm *tm) {
  device->tmid=tm->tmid;
  const struct inmgr_tmbtn *tmbtn=tm->tmbtnv;
  int i=tm->tmbtnc;
  for (;i-->0;tmbtn++) inmgr_buttonv_append_key(device,tmbtn->srcbtnid,tmbtn->dstbtnid);
}

/* Build up (device->buttonv) for keyboards, with the default layout.
 */
 
static const int inmgr_prekb_04[]={
  INMGR_BTN_LEFT, // A
  0, // B
  INMGR_BTN_EAST, // C
  INMGR_BTN_RIGHT, // D
  INMGR_BTN_EAST, // E
  0,0,0,0,0,0,0,0,0,0,0, // F G H I J K L M N O P
  INMGR_BTN_WEST, // Q
  INMGR_BTN_NORTH, // R
  INMGR_BTN_DOWN, // S
  0,0, // T U 
  INMGR_BTN_NORTH, // V
  INMGR_BTN_UP, // W
  INMGR_BTN_WEST, // X
  0, // Y
  INMGR_BTN_SOUTH, // Z
  0,0,0,0,0,0,0,0,0,0, // 1 2 3 4 5 6 7 8 9 0
  INMGR_BTN_AUX1, // Enter
  INMGR_BTN_QUIT, // Escape
  INMGR_BTN_R2, // Backspace
  INMGR_BTN_L1, // Tab
  INMGR_BTN_SOUTH, // Space
  0,0, // Dash Equal
  INMGR_BTN_AUX3, // OpenBracket
  INMGR_BTN_AUX2, // CloseBracket
  INMGR_BTN_R1, // Backslash
  0,0,0, // Hash Semicolon Apostrophe
  INMGR_BTN_L2, // Grave
  0,0,0,0, // Comma Dot Slash CapsLock
  0,0,0,0,0,0,0,0,0,0,0,0, // F1 F2 F3 F4 F5 F6 F7 F8 F9 F10 F11 F12
  0,0, // Print ScrollLock
  INMGR_BTN_PAUSE, // Pause
  0,0,0,0,0,0, // Insert Home PageUp Delete End PageDown
  INMGR_BTN_RIGHT, // RightArrow
  INMGR_BTN_LEFT, // LeftArrow
  INMGR_BTN_DOWN, // DownArrow
  INMGR_BTN_UP, // UpArrow
  0, // NumLock
  INMGR_BTN_AUX1, // KPSlash
  INMGR_BTN_AUX2, // KPStar
  INMGR_BTN_AUX3, // KPDash
  INMGR_BTN_EAST, // KPPlus
  INMGR_BTN_WEST, // KPEnter
  INMGR_BTN_L2, // KP1
  INMGR_BTN_DOWN, // KP2
  INMGR_BTN_R2, // KP3
  INMGR_BTN_LEFT, // KP4
  INMGR_BTN_DOWN, // KP5
  INMGR_BTN_RIGHT, // KP6
  INMGR_BTN_L1, // KP7
  INMGR_BTN_UP, // KP8
  INMGR_BTN_R1, // KP9
  INMGR_BTN_SOUTH, // KP0
  INMGR_BTN_NORTH, // KPDot
};
static const int inmgr_prekb_e0[]={
  0,0,0,0,0,0,0,0, // LCtrl LShift LAlt LSuper RCtrl RShift RAlt RSuper
};

static void inmgr_device_make_up_keyboard_map(struct inmgr_device *device) {
  const int *p;
  int i,btnid;
  for (p=inmgr_prekb_04,i=sizeof(inmgr_prekb_04)/sizeof(int),btnid=0x00070004;i-->0;p++,btnid++) inmgr_buttonv_append_key(device,btnid,*p);
  for (p=inmgr_prekb_e0,i=sizeof(inmgr_prekb_e0)/sizeof(int),btnid=0x000700e0;i-->0;p++,btnid++) inmgr_buttonv_append_key(device,btnid,*p);
}

/* Map a device button to a specific output button, with optional comment.
 */
 
int inmgr_button_remap(struct inmgr_button *button,int dstbtnid,const char *comment,int commentc) {

  button->dstbtnid=0;
  button->mode=INMGR_BUTTON_MODE_NOOP;
  if (!dstbtnid) return 0;
  
  int range=button->hi-button->lo+1;
  if (range<2) return -1;
  
  // Signals have their own mode, which works just like TWOSTATE.
  // Discard if not defined.
  if (dstbtnid&0xff000000) {
    if (inmgr_signalv_search(dstbtnid)<0) return -1;
    button->dstbtnid=dstbtnid;
    button->mode=INMGR_BUTTON_MODE_SIGNAL;
    button->srclo=button->lo+1;
    button->srchi=INT_MAX;
    return 0;
  }
  
  // Extbtn must be declared. Can be LINEAR or TWOSTATE.
  if (dstbtnid&0x00ff0000) {
    int extbtnp=inmgr_extbtnv_search(dstbtnid);
    if (extbtnp<0) return -1;
    int extrange=inmgr.extbtnv[extbtnp].hi-inmgr.extbtnv[extbtnp].lo+1;
    if (extrange>=3) {
      button->mode=INMGR_BUTTON_MODE_LINEAR;
      button->srclo=button->lo;
      button->srchi=button->hi;
    } else {
      button->mode=INMGR_BUTTON_MODE_TWOSTATE;
      button->srclo=button->lo+1;
      button->srchi=INT_MAX;
    }
    if (inmgr_comment_has_token(comment,commentc,"reverse",7)) {
      int tmp=button->srclo;
      button->srclo=button->srchi;
      button->srchi=tmp;
    }
    button->dstbtnid=dstbtnid;
    return 0;
  }
  
  // May map to the entire dpad, as a hat. That's its own mode. Range must be exactly 8.
  if (dstbtnid==INMGR_BTN_DPAD) {
    if (range!=8) return -1;
    button->dstbtnid=dstbtnid;
    button->mode=INMGR_BUTTON_MODE_HAT;
    button->srclo=button->lo;
    button->srcvalue=button->lo-1;
    return 0;
  }
  
  // Mapping to both sides of an axis creates a THREEWAY.
  if ((dstbtnid==INMGR_BTN_HORZ)||(dstbtnid==INMGR_BTN_VERT)) {
    if (range<3) return -1;
    button->dstbtnid=dstbtnid;
    button->mode=INMGR_BUTTON_MODE_THREEWAY;
    int mid=(button->lo+button->hi)>>1;
    int midlo=(button->lo+mid)>>1;
    int midhi=(button->hi+mid)>>1;
    if (midlo>=mid) midlo=mid-1;
    if (midhi<=mid) midhi=mid+1;
    if (inmgr_comment_has_token(comment,commentc,"reverse",7)) {
      button->srclo=midhi;
      button->srchi=midlo;
    } else {
      button->srclo=midlo;
      button->srchi=midhi;
    }
    return 0;
  }
  
  // Everything else is TWOSTATE.
  button->dstbtnid=dstbtnid;
  button->mode=INMGR_BUTTON_MODE_TWOSTATE;
  button->srclo=button->lo+1;
  button->srchi=INT_MAX;
  return 0;
}

/* Every button in (device->buttonv) which is also named in (tm->tmbtnv), work out its exact mapping details.
 * Others are left untouched, presumably they started with mode==0 (NOOP).
 * This is the normal hello-device case.
 */
 
static void inmgr_device_apply_applicable_template(struct inmgr_device *device,const struct inmgr_tm *tm) {
  device->tmid=tm->tmid;
  struct inmgr_button *button=device->buttonv;
  int i=device->buttonc;
  for (;i-->0;button++) {
    const struct inmgr_tmbtn *tmbtn=inmgr_tmbtn_for_srcbtnid(tm,button->srcbtnid);
    if (!tmbtn) continue;
    inmgr_button_remap(button,tmbtn->dstbtnid,tmbtn->comment,tmbtn->commentc);
  }
}

/* Choose mapping for a button.
 * (mode) and (dstbtnid) are initially zero. We set them if we choose to.
 */
 
static void inmgr_button_make_up_generic_map(
  const struct inmgr_device *device,
  struct inmgr_button *button,
  const int *count_by_bit/*16*/,
  const int *count_by_extbtn/*INMGR_EXTBTN_LIMIT*/
) {

  /* Determine range on the input side, and abort if invalid.
   */
  int range=button->hi-button->lo+1;
  if (range<2) return;

  /* Range of 8 can only be a hat (and vice-versa).
   */
  if (range==8) {
    button->mode=INMGR_BUTTON_MODE_HAT;
    button->dstbtnid=INMGR_BTN_DPAD;
    button->srclo=button->lo;
    button->srcvalue=button->lo-1;
    return;
  }
  
  /* Limit values on opposite sides of zero, must be a signed axis.
   * Likewise if the range is at least three, and initial value is between them.
   */
  if (
    ((button->lo<0)&&(button->hi>0))||
    ((button->srcvalue>button->lo)&&(button->srcvalue<button->hi))
  ) {
    int extp;
    switch (button->hidusage) {
      case 0x00010030: case 0x00010040: if ((extp=inmgr_extbtnv_search(INMGR_BTN_LX))>=0) button->dstbtnid=INMGR_BTN_LX; else button->dstbtnid=INMGR_BTN_HORZ; break;
      case 0x00010031: case 0x00010041: if ((extp=inmgr_extbtnv_search(INMGR_BTN_LY))>=0) button->dstbtnid=INMGR_BTN_LY; else button->dstbtnid=INMGR_BTN_VERT; break;
      case 0x00010033: case 0x00010043: if ((extp=inmgr_extbtnv_search(INMGR_BTN_RX))>=0) button->dstbtnid=INMGR_BTN_RX; else button->dstbtnid=INMGR_BTN_HORZ; break;
      case 0x00010034: case 0x00010044: if ((extp=inmgr_extbtnv_search(INMGR_BTN_RY))>=0) button->dstbtnid=INMGR_BTN_RY; else button->dstbtnid=INMGR_BTN_VERT; break;
      default: { // No guidance from HID on which axis to use. Use whichever dpad axis has the fewest assignments so far. Ties break horizontal.
          int xc=count_by_bit[0]+count_by_bit[1];
          int yc=count_by_bit[2]+count_by_bit[3];
          if (xc<=yc) button->dstbtnid=INMGR_BTN_HORZ;
          else button->dstbtnid=INMGR_BTN_VERT;
        }
    }
    if (button->dstbtnid&0x00ff0000) {
      int dstrange=inmgr.extbtnv[extp].hi-inmgr.extbtnv[extp].lo+1;
      if (dstrange>3) {
        button->mode=INMGR_BUTTON_MODE_LINEAR;
        button->srclo=button->lo;
        button->srchi=button->hi;
      } else {
        button->mode=INMGR_BUTTON_MODE_THREEWAY;
      }
    } else {
      button->mode=INMGR_BUTTON_MODE_THREEWAY;
    }
    if (button->mode==INMGR_BUTTON_MODE_THREEWAY) {
      int mid=(button->lo+button->hi)>>1;
      int midlo=(button->lo+mid)>>1;
      int midhi=(button->hi+mid)>>1;
      if (midlo>=mid) midlo=mid-1;
      if (midhi<=mid) midhi=mid+1;
      button->srclo=midlo;
      button->srchi=midhi;
    }
    return;
  }
  
  /* We could look deeper at (hidusage), but in my experience it's pretty unreliable.
   * Better to just pick willy-nilly at this point.
   * Assign to any twostate button in the global button mask, whichever has the lowest count so far, ties breaking low.
   */
  int mask=1,ix=0,pref=-1;
  for (;ix<15;ix++,mask<<=1) { // sic 15; don't include CD.
    if (!(inmgr.btnmask&mask)) continue;
    if ((pref<0)||(count_by_bit[ix]<count_by_bit[pref])) pref=ix;
  }
  if (pref<0) return; // Oops. Is (btnmask) empty or something?
  button->dstbtnid=1<<pref;
  button->mode=INMGR_BUTTON_MODE_TWOSTATE;
  button->srclo=button->lo+1;
  button->srchi=INT_MAX;
}

/* Set destinations in (device->buttonv) using voodoo.
 * The fallback case for unconfigured devices, excluding keyboards.
 */

static void inmgr_device_make_up_generic_map(struct inmgr_device *device) {

  /* First assess any mappings we already have.
   * There should be none, but I'll allow that we could map selected buttons before this point.
   * Also count buttons on HID page 7. The device might be a keyboard that got reported generically.
   */
  int count_by_bit[16]={0};
  int count_by_extbtn[INMGR_EXTBTN_LIMIT]={0};
  int page7c=0;
  struct inmgr_button *button=device->buttonv;
  int i=device->buttonc;
  for (;i-->0;button++) {
    if ((button->hidusage&0xffff0000)==0x00070000) page7c++;
    if (button->dstbtnid&0xff000000) ; // Ignore signals.
    else if (button->dstbtnid&0x00ff0000) { // extbtn
      int p=inmgr_extbtnv_search(button->dstbtnid);
      if (p>=0) count_by_extbtn[p]++;
    } else if (button->dstbtnid) { // bits
      int mask=1,ix=0;
      for (;ix<16;ix++,mask<<=1) {
        if (button->dstbtnid&mask) count_by_bit[ix]++;
      }
    }
  }
  
  /* If there's at least 40 buttons on HID page 7, abort and call it a keyboard instead.
   */
  if (page7c>=40) {
    device->buttonc=0;
    inmgr_device_make_up_keyboard_map(device);
    return;
  }
  
  /* Run through the buttons again and select an output for each.
   */
  for (button=device->buttonv,i=device->buttonc;i-->0;button++) {
  
    // Allow that they could have been mapped at some earlier stage (they won't).
    if (button->dstbtnid) continue;
    
    // Call out for the proper decision making, then update our transient bookkeeping.
    inmgr_button_make_up_generic_map(device,button,count_by_bit,count_by_extbtn);
    if (button->dstbtnid&0xff000000) ;
    else if (button->dstbtnid&0x00ff0000) {
      int p=inmgr_extbtnv_search(button->dstbtnid);
      if (p>=0) count_by_extbtn[p]++;
    } else if (button->dstbtnid) {
      int mask=1,ix=0;
      for (;ix<16;ix++,mask<<=1) {
        if (button->dstbtnid&mask) count_by_bit[ix]++;
      }
    }
  }
}

/* Nonzero if the device has what appears to be a usable set of mappings.
 */
 
static int inmgr_device_usable(const struct inmgr_device *device) {
  int cap=0;
  const struct inmgr_button *button=device->buttonv;
  int i=device->buttonc;
  for (;i-->0;button++) {
    if (button->dstbtnid&0xffff0000) continue; // Ignore signals and extbtn.
    cap|=button->dstbtnid;
  }
  // Let's say we need the full dpad and at least one thumb button, doesn't matter which.
  // This is completely arbitrary.
  if ((cap&INMGR_BTN_DPAD)!=INMGR_BTN_DPAD) return 0;
  if (!(cap&(INMGR_BTN_SOUTH|INMGR_BTN_WEST|INMGR_BTN_EAST|INMGR_BTN_NORTH))) return 0;
  return 1;
}

/* Disconnect device.
 */
 
void inmgr_disconnect(int devid) {
  int devp=inmgr_devicev_search(devid);
  if (devp<0) return;
  int playerid=0,state=0;
  struct inmgr_device *device=inmgr.devicev+devp;
  if (device->enable>0) {
    playerid=device->playerid;
    state=device->state;
  }
  inmgr_device_cleanup(device);
  inmgr.devicec--;
  memmove(device,device+1,sizeof(struct inmgr_device)*(inmgr.devicec-devp));
  if (playerid) {
    inmgr.playerv[playerid].state&=~state;
    inmgr.playerv[0].state&=~state;
    memset(inmgr.playerv[playerid].extbtnv,0,sizeof(int)*inmgr.extbtnc);
    memset(inmgr.playerv[0].extbtnv,0,sizeof(int)*inmgr.extbtnc);
    if (state&INMGR_BTN_CD) { // Restore CD if there's any other device mapped.
      int i=inmgr.devicec;
      for (device=inmgr.devicev;i-->0;device++) {
        if (device->enable<=0) continue;
        inmgr.playerv[device->playerid].state|=INMGR_BTN_CD;
        inmgr.playerv[0].state|=INMGR_BTN_CD;
      }
    }
  }
  inmgr_broadcast(devid,0,0,0);
}

/* Connect keyboard, one-shot.
 */
 
void inmgr_connect_keyboard(int devid) {

  /* Create the new device, or fail if it already exists or invalid devid.
   */
  if (devid<1) return;
  int devp=inmgr_devicev_search(devid);
  if (devp>=0) return;
  devp=-devp-1;
  if (inmgr.devicec>=inmgr.devicea) {
    int na=inmgr.devicea+16;
    if (na>INT_MAX/sizeof(struct inmgr_device)) return;
    void *nv=realloc(inmgr.devicev,sizeof(struct inmgr_device)*na);
    if (!nv) return;
    inmgr.devicev=nv;
    inmgr.devicea=na;
  }
  struct inmgr_device *device=inmgr.devicev+devp;
  memmove(device+1,device,sizeof(struct inmgr_device)*(inmgr.devicec-devp));
  inmgr.devicec++;
  memset(device,0,sizeof(struct inmgr_device));
  
  /* Initialize a few header things.
   * Keyboards are mapped and ready instantly, since they can only map to player one.
   */
  device->devid=devid;
  device->keyboard=1;
  device->enable=1;
  device->ready=1;
  device->playerid=1;
  inmgr_device_set_name(device,"System Keyboard",15);
  device->state=INMGR_BTN_CD;
  inmgr.playerv[1].state|=INMGR_BTN_CD;
  inmgr.playerv[0].state|=INMGR_BTN_CD;
  
  /* Find the appropriate template and apply it blindly.
   * We do not query device caps for keyboards.
   */
  struct inmgr_tm *tm=inmgr_tm_for_keyboard();
  if (tm) {
    inmgr_device_apply_full_template(device,tm);
  } else {
    inmgr_device_make_up_keyboard_map(device);
    inmgr_tm_synthesize_from_device(device);
    inmgr_signal(INMGR_BTN_AUTOMAPPED);
  }
  
  inmgr_broadcast(devid,0,1,device->state);
}

/* Begin connection of new device.
 */

void inmgr_connect_begin(int devid,int vid,int pid,int version,const char *name,int namec) {

  /* Validate devid and create the new device instance.
   */
  if (devid<1) return;
  int devp=inmgr_devicev_search(devid);
  if (devp>=0) return;
  devp=-devp-1;
  if (inmgr.devicec>=inmgr.devicea) {
    int na=inmgr.devicea+16;
    if (na>INT_MAX/sizeof(struct inmgr_device)) return;
    void *nv=realloc(inmgr.devicev,sizeof(struct inmgr_device)*na);
    if (!nv) return;
    inmgr.devicev=nv;
    inmgr.devicea=na;
  }
  struct inmgr_device *device=inmgr.devicev+devp;
  memmove(device+1,device,sizeof(struct inmgr_device)*(inmgr.devicec-devp));
  inmgr.devicec++;
  memset(device,0,sizeof(struct inmgr_device));
  
  /* Initialize.
   */
  device->devid=devid;
  device->vid=vid;
  device->pid=pid;
  device->version=version;
  inmgr_device_set_name(device,name,namec);
  
  /* Leave (ready==0) and (enable==0) until we've mapped.
   */
}

/* Receive device capability.
 * This is dumb, just record the caps so we have them at inmgr_connect_end.
 * Discard obviously-invalid ones.
 */
 
void inmgr_connect_more(int devid,int btnid,int hidusage,int lo,int hi,int value) {
  if (lo>=hi) return;
  struct inmgr_device *device=inmgr_device_by_devid(devid);
  if (!device||device->ready) return;
  int btnp=inmgr_device_buttonv_search(device,btnid);
  if (btnp>=0) return; // Duplicate. Keep the first one.
  btnp=-btnp-1;
  if (device->buttonc>=device->buttona) {
    int na=device->buttona+16;
    if (na>INT_MAX/sizeof(struct inmgr_button)) return;
    void *nv=realloc(device->buttonv,sizeof(struct inmgr_button)*na);
    if (!nv) return;
    device->buttonv=nv;
    device->buttona=na;
  }
  struct inmgr_button *button=device->buttonv+btnp;
  memmove(button+1,button,sizeof(struct inmgr_button)*(device->buttonc-btnp));
  device->buttonc++;
  memset(button,0,sizeof(struct inmgr_button));
  button->srcbtnid=btnid;
  button->srcvalue=value;
  button->hidusage=hidusage;
  button->lo=lo;
  button->hi=hi;
}

/* Finish connection of new device.
 */
 
void inmgr_connect_end(int devid) {
  struct inmgr_device *device=inmgr_device_by_devid(devid);
  if (!device||device->ready) return;
  device->ready=1;
  struct inmgr_tm *tm=inmgr_tm_for_device(device);
  if (tm) {
    inmgr_device_apply_applicable_template(device,tm);
  } else {
    inmgr_device_make_up_generic_map(device);
    inmgr_tm_synthesize_from_device(device);
    inmgr_signal(INMGR_BTN_AUTOMAPPED);
  }
  if (inmgr_device_usable(device)) {
    device->enable=1;
  }
  inmgr_broadcast(devid,0,1,device->state);
}

/* Manually modify one button mapping.
 */

int inmgr_remap_button(int devid,int srcbtnid,int dstbtnid) {
  if (!srcbtnid) return -1;
  if (dstbtnid==INMGR_BTN_CD) return -1;

  struct inmgr_device *device=inmgr_device_by_devid(devid);
  if (!device) return -1;
  struct inmgr_button *button;
  int btnp=inmgr_device_buttonv_search(device,srcbtnid);
  if (btnp>=0) {
    button=device->buttonv+btnp;
  } else {
    btnp=-btnp-1;
    if (device->buttonc>=device->buttona) {
      int na=device->buttona+16;
      if (na>INT_MAX/sizeof(struct inmgr_button)) return -1;
      void *nv=realloc(device->buttonv,sizeof(struct inmgr_button)*na);
      if (!nv) return -1;
      device->buttonv=nv;
      device->buttona=na;
    }
    button=device->buttonv+btnp;
    memmove(button+1,button,sizeof(struct inmgr_button)*(device->buttonc-btnp));
    device->buttonc++;
    memset(button,0,sizeof(struct inmgr_button));
    button->srcbtnid=srcbtnid;
  }
  inmgr_button_remap(button,dstbtnid,0,0);
  
  if (device->tmid) {
    struct inmgr_tm *tm=inmgr.tmv;
    int i=inmgr.tmc;
    for (;i-->0;tm++) {
      if (device->tmid==tm->tmid) {
        inmgr_tmbtn_intern(tm,srcbtnid,dstbtnid);
      }
    }
  }
  
  return 0;
}
