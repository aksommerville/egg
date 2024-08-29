#include "inmgr_internal.h"

/* Fire signal.
 */
 
void inmgr_signal(struct inmgr *inmgr,int sigid) {
  switch (sigid) {
    case EGG_SIGNAL_QUIT: eggrt.terminate++; break;
    case EGG_SIGNAL_FULLSCREEN: hostio_toggle_fullscreen(eggrt.hostio); break;
    case EGG_SIGNAL_PAUSE: eggrt.debugger=eggrt.debugger?0:1; eggrt.debugstep=0; break;
    case EGG_SIGNAL_STEP: if (eggrt.debugger) eggrt.debugstep=1; break;
    case EGG_SIGNAL_SAVESTATE: fprintf(stderr,"TODO EGG_SIGNAL_SAVESTATE [%s:%d]\n",__FILE__,__LINE__); break;
    case EGG_SIGNAL_LOADSTATE: fprintf(stderr,"TODO EGG_SIGNAL_LOADSTATE [%s:%d]\n",__FILE__,__LINE__); break;
    case EGG_SIGNAL_SCREENCAP: fprintf(stderr,"TODO EGG_SIGNAL_SCREENCAP [%s:%d]\n",__FILE__,__LINE__); break;
    case EGG_SIGNAL_CONFIGIN: egg_input_configure(); break;
  }
}

/* Set player button.
 */
 
void inmgr_set_button(struct inmgr *inmgr,int playerid,int btnid,int value) {
  if (!btnid) return;
  if (btnid&EGG_SIGNAL_BIT) {
    if (value) inmgr_signal(inmgr,btnid);
    return;
  }
  if ((playerid>=0)&&(playerid<inmgr->playerc)) {
    if (value) {
      if (inmgr->playerv[playerid]&btnid) return;
      inmgr->playerv[playerid]|=btnid;
    } else {
      if (!(inmgr->playerv[playerid]&btnid)) return;
      inmgr->playerv[playerid]&=~btnid;
    }
  }
  if (playerid) inmgr_set_button(inmgr,0,btnid,value);
}

/* Choose playerid for a new device.
 * We are as lazy as possible about this: Playerid is chosen when we receive a nonzero state change.
 * (it doesn't happen at the moment of connection, as you might expect).
 * That is actually very important: Devices get connected willy-nilly, but the first one the user touches should be "player 1".
 * Note that keyboard does not participate in this. It is always player 1, and the first joystick is also player 1.
 */
 
static int inmgr_choose_playerid(const struct inmgr *inmgr) {
  if (inmgr->playerc<=2) return 1;
  int count_by_playerid[INMGR_PLAYER_LIMIT]={0};
  int i=inmgr->devicec; while (i-->0) {
    const struct inmgr_device *device=inmgr->devicev[i];
    if ((device->playerid>=0)&&(device->playerid<inmgr->playerc)) {
      count_by_playerid[device->playerid]++;
    }
  }
  int playerid=1,bestcount=count_by_playerid[1];
  for (i=2;i<inmgr->playerc;i++) {
    if (count_by_playerid[i]<bestcount) {
      playerid=i;
      bestcount=count_by_playerid[i];
    }
  }
  return playerid;
}

/* Receive button event, either keyboard or joystick.
 */
 
static void inmgr_srcbutton(struct inmgr *inmgr,int devid,int srcbtnid,int value) {
  struct inmgr_device *device=inmgr_device_by_devid(inmgr,devid);
  if (!device) return;
  int p=inmgr_device_buttonv_search(device,srcbtnid);
  if (p<0) return;
  struct inmgr_device_button *button=device->buttonv+p;
  for (;(p<device->buttonc)&&(button->srcbtnid==srcbtnid);p++,button++) {
    if (button->srcvalue==value) continue;
    button->srcvalue=value;
    
    // Hats are special.
    if (button->dstbtnid==EGG_BTN_DPAD) {
      int dstvalue=value-button->srclo;
      button->dstvalue=dstvalue;
      int x=0,y=0;
      switch (dstvalue) {
        case 7: case 0: case 1: y=-1; break;
        case 5: case 4: case 3: y=1; break;
      }
      switch (dstvalue) {
        case 7: case 6: case 5: x=-1; break;
        case 1: case 2: case 3: x=1; break;
      }
      if (!device->playerid) {
        device->playerid=inmgr_choose_playerid(inmgr);
        inmgr_set_button(inmgr,device->playerid,EGG_BTN_CD,1);
      }
      inmgr_set_button(inmgr,device->playerid,EGG_BTN_LEFT,(x<0));
      inmgr_set_button(inmgr,device->playerid,EGG_BTN_RIGHT,(x>0));
      inmgr_set_button(inmgr,device->playerid,EGG_BTN_UP,(y<0));
      inmgr_set_button(inmgr,device->playerid,EGG_BTN_DOWN,(y>0));
      device->state&=~EGG_BTN_DPAD;
      if (x<0) device->state|=EGG_BTN_LEFT; else if (x>0) device->state|=EGG_BTN_RIGHT;
      if (y<0) device->state|=EGG_BTN_UP; else if (y>0) device->state|=EGG_BTN_DOWN;
      continue;
    }
    
    // The rest are generic, a simple range test.
    int dstvalue=((value>=button->srclo)&&(value<=button->srchi))?1:0;
    if (button->dstvalue==dstvalue) continue;
    button->dstvalue=dstvalue;
    if (button->dstbtnid&EGG_SIGNAL_BIT) {
      if (dstvalue) inmgr_signal(inmgr,button->dstbtnid);
    } else {
      if (dstvalue) {
        device->state|=button->dstbtnid;
        if (!device->playerid) {
          device->playerid=inmgr_choose_playerid(inmgr);
          inmgr_set_button(inmgr,device->playerid,EGG_BTN_CD,1);
          device->state|=EGG_BTN_CD;
        }
      } else {
        device->state&=~button->dstbtnid;
      }
      inmgr_set_button(inmgr,device->playerid,button->dstbtnid,dstvalue);
    }
  }
}

/* Incoming button events.
 */
 
void inmgr_event_key(struct inmgr *inmgr,int keycode,int value) {
  inmgr_srcbutton(inmgr,inmgr->syskbd,keycode,value);
}
 
void inmgr_event_button(struct inmgr *inmgr,struct hostio_input *driver,int devid,int btnid,int value) {
  //fprintf(stderr,"%d.%#.8x=%d\n",devid,btnid,value);
  inmgr_srcbutton(inmgr,devid,btnid,value);
}

/* Recheck all carrier-detect bits.
 * Call this whenever a device disconnects, after tearing down its mappings.
 * If a player state ends up without CD, we also zero the rest of it.
 */
 
static void inmgr_check_cd(struct inmgr *inmgr) {
  if (inmgr->playerc<1) return;
  int i,*v;
  for (i=inmgr->playerc,v=inmgr->playerv;i-->0;v++) (*v)&=~EGG_BTN_CD;
  
  if (inmgr->syskbd&&(inmgr->playerc>=2)) inmgr->playerv[1]|=EGG_BTN_CD;
  for (i=inmgr->devicec;i-->0;) {
    struct inmgr_device *device=inmgr->devicev[i];
    if ((device->playerid>0)&&(device->playerid<inmgr->playerc)) {
      device->state|=EGG_BTN_CD;
      inmgr->playerv[device->playerid]|=EGG_BTN_CD;
    }
  }
  
  for (i=inmgr->playerc,v=inmgr->playerv;i-->0;v++) {
    if ((*v)&EGG_BTN_CD) inmgr->playerv[0]|=EGG_BTN_CD;
    else *v=0;
  }
}

/* If there is a device for this devid, remove it and also propogate removal of its state, minus CD.
 * Caller should refresh all CD from scratch after.
 */
 
static void inmgr_drop_device_by_devid(struct inmgr *inmgr,int devid) {
  int p=inmgr_devicev_search(inmgr,devid);
  if (p<0) return;
  struct inmgr_device *device=inmgr->devicev[p];
  inmgr->devicec--;
  memmove(inmgr->devicev+p,inmgr->devicev+p+1,sizeof(void*)*(inmgr->devicec-p));
  if (device->playerid) {
    int state=(device->state&~EGG_BTN_CD);
    if (state) {
      int i=1; for (;i<=state;i<<=1) if (state&i) inmgr_set_button(inmgr,device->playerid,i,0);
    }
  }
  inmgr_device_del(device);
}

/* Connect new device.
 * Returns a WEAK reference to the newly-listed device object on success.
 */
 
static struct inmgr_device *inmgr_welcome_device(
  struct inmgr *inmgr,
  int vid,int pid,int version,
  const char *name,int namec,
  struct hostio_input *driver,
  int devid
) {
  if (!name) namec=0; else if (namec<0) { namec=0; while (name[namec]) namec++; }
  
  /* Create and register the device.
   */
  struct inmgr_device *device=inmgr_devicev_spawn(inmgr,devid);
  if (!device) return 0;
  
  /* Check templates in order. Apply the first that matches.
   */
  int i=0; for (;i<inmgr->tmc;i++) {
    struct inmgr_tm *tm=inmgr->tmv[i];
    if (inmgr_tm_match(tm,vid,pid,version,name,namec)) {
      if (inmgr_tm_apply(inmgr,device,tm,driver)<0) {
        inmgr_drop_device_by_devid(inmgr,devid);
        return 0;
      }
      return device;
    }
  }
  
  /* Synthesize and apply a template.
   */
  struct inmgr_tm *tm=inmgr_tmv_synthesize(inmgr,driver,devid,vid,pid,version,name,namec);
  if (inmgr_tm_apply(inmgr,device,tm,driver)<0) {
    inmgr_drop_device_by_devid(inmgr,devid);
    return 0;
  }
  
  return device;
}

/* Connect or disconnect system keyboard.
 */
 
int inmgr_enable_system_keyboard(struct inmgr *inmgr,int enable) {
  if (enable) {
    if (inmgr->syskbd) return 0;
    if ((inmgr->syskbd=hostio_input_devid_next())<1) {
      inmgr->syskbd=0;
      return -1;
    }
    struct inmgr_device *device=inmgr_welcome_device(inmgr,0,0,0,"System Keyboard",15,0,inmgr->syskbd);
    if (!device) {
      inmgr->syskbd=0;
      return -1;
    }
  } else {
    inmgr_drop_device_by_devid(inmgr,inmgr->syskbd);
    inmgr->syskbd=0;
    inmgr_check_cd(inmgr);
  }
  return 0;
}

/* Connect joystick.
 */
 
void inmgr_event_connect(struct inmgr *inmgr,struct hostio_input *driver,int devid) {
  int vid=0,pid=0,version=0;
  const char *name=0;
  if (driver&&driver->type->get_ids) {
    name=driver->type->get_ids(&vid,&pid,&version,driver,devid);
  }
  struct inmgr_device *device=inmgr_welcome_device(inmgr,vid,pid,version,name,-1,driver,devid);
  if (!device) return;
}

/* Disconnect joystick.
 */
 
void inmgr_event_disconnect(struct inmgr *inmgr,struct hostio_input *driver,int devid) {
  inmgr_drop_device_by_devid(inmgr,devid);
  inmgr_check_cd(inmgr);
}
