#include "inmgr_internal.h"

/* Pick a playerid from some device that doesn't have one yet.
 * Happens on the first nonzero state change.
 */
 
static void inmgr_device_select_playerid(struct inmgr_device *device) {
  if (device->enable<=0) return;
  if (device->keyboard) {
    device->playerid=1;
  } else {
    int count_by_playerid[1+INMGR_PLAYER_LIMIT]={0};
    const struct inmgr_device *other=inmgr.devicev;
    int i=inmgr.devicec;
    for (;i-->0;other++) {
      if (other->keyboard) continue;
      if (other->enable<=0) continue;
      count_by_playerid[other->playerid]++;
    }
    device->playerid=1;
    for (i=2;i<=inmgr.playerc;i++) {
      if (count_by_playerid[device->playerid]>count_by_playerid[i]) device->playerid=i;
    }
  }
}

/* Update and cascade device's state.
 */
 
static void inmgr_device_update_state(struct inmgr_device *device,int btnid,int value) {
  if (btnid&0xffff0000) { // extbtn
    int extbtnp=inmgr_extbtnv_search(btnid);
    if (extbtnp>=0) {
      if (value==device->extbtnv[extbtnp]) return;
      device->extbtnv[extbtnp]=value;
      if (device->enable>0) {
        if (!device->playerid) {
          if (!value) return;
          inmgr_device_select_playerid(device);
        }
        inmgr.playerv[device->playerid].extbtnv[extbtnp]=value;
        inmgr.playerv[0].extbtnv[extbtnp]=value;
      }
    }
      
  } else if (value) { // twostate true
    if (device->state&btnid) return;
    device->state|=btnid;
    if (device->enable>0) {
      if (!device->playerid) inmgr_device_select_playerid(device);
      inmgr.playerv[device->playerid].state|=btnid;
      inmgr.playerv[0].state|=btnid;
    }
    
  } else { // twostate false
    if (!(device->state&btnid)) return;
    device->state&=~btnid;
    if ((device->enable>0)&&device->playerid) {
      inmgr.playerv[device->playerid].state&=~btnid;
      inmgr.playerv[0].state&=~btnid;
    }
  }
}

/* Apply source event to live button.
 */
 
static void inmgr_button_update(struct inmgr_device *device,struct inmgr_button *button,int srcvalue) {
  if (button->srcvalue==srcvalue) return;
  button->srcvalue=srcvalue;
  switch (button->mode) {
  
    case INMGR_BUTTON_MODE_SIGNAL: {
        int dstvalue=((srcvalue>=button->srclo)&&(srcvalue<=button->srchi))?1:0;
        if (dstvalue==button->dstvalue) return;
        button->dstvalue=dstvalue;
        if (dstvalue&&(device->enable>0)) inmgr_signal(button->dstbtnid);
      } break;
      
    case INMGR_BUTTON_MODE_TWOSTATE: {
        int dstvalue=((srcvalue>=button->srclo)&&(srcvalue<=button->srchi))?1:0;
        if (dstvalue==button->dstvalue) return;
        button->dstvalue=dstvalue;
        inmgr_device_update_state(device,button->dstbtnid,dstvalue);
      } break;
      
    case INMGR_BUTTON_MODE_THREEWAY: {
        int dstvalue,btnidlo,btnidhi;
        if (button->srclo<=button->srchi) {
          dstvalue=(srcvalue<=button->srclo)?-1:(srcvalue>=button->srchi)?1:0;
          btnidlo=(button->dstbtnid&(INMGR_BTN_LEFT|INMGR_BTN_UP));
          btnidhi=(button->dstbtnid&(INMGR_BTN_RIGHT|INMGR_BTN_DOWN));
        } else {
          dstvalue=(srcvalue<=button->srchi)?-1:(srcvalue>=button->srclo)?1:0;
          btnidhi=(button->dstbtnid&(INMGR_BTN_LEFT|INMGR_BTN_UP));
          btnidlo=(button->dstbtnid&(INMGR_BTN_RIGHT|INMGR_BTN_DOWN));
        }
        if (dstvalue==button->dstvalue) return;
        button->dstvalue=dstvalue;
        switch (dstvalue) {
          case -1: {
              inmgr_device_update_state(device,btnidlo,1);
              inmgr_device_update_state(device,btnidhi,0);
            } break;
          case 0: {
              inmgr_device_update_state(device,btnidlo,0);
              inmgr_device_update_state(device,btnidhi,0);
            } break;
          case 1: {
              inmgr_device_update_state(device,btnidlo,0);
              inmgr_device_update_state(device,btnidhi,1);
            } break;
        }
      } break;
      
    case INMGR_BUTTON_MODE_HAT: {
        srcvalue-=button->srclo;
        if (srcvalue==button->dstvalue) return;
        button->dstvalue=srcvalue;
        int dx=0,dy=0;
        switch (srcvalue) {
          case 7: case 6: case 5: dx=-1; break;
          case 1: case 2: case 3: dx=1; break;
        }
        switch (srcvalue) {
          case 7: case 0: case 1: dy=-1; break;
          case 5: case 4: case 3: dy=1; break;
        }
        inmgr_device_update_state(device,INMGR_BTN_LEFT ,(dx<0)?1:0);
        inmgr_device_update_state(device,INMGR_BTN_RIGHT,(dx>0)?1:0);
        inmgr_device_update_state(device,INMGR_BTN_UP   ,(dy<0)?1:0);
        inmgr_device_update_state(device,INMGR_BTN_DOWN ,(dy>0)?1:0);
      } break;
      
    case INMGR_BUTTON_MODE_LINEAR: {
        // LINEAR is only allowed for extbtn, because we need two ranges.
        int extbtnp=inmgr_extbtnv_search(button->dstbtnid);
        if (extbtnp<0) break;
        const struct inmgr_extbtn *extbtn=inmgr.extbtnv+extbtnp;
        int dstvalue=((srcvalue-button->srclo)*(extbtn->hi-extbtn->lo))/(button->srchi-button->srclo);
        inmgr_device_update_state(device,button->dstbtnid,dstvalue);
      } break;
  }
}

/* Event from drivers.
 */
 
void inmgr_event(int devid,int btnid,int value) {

  // Reject invalid events without mapping or broadcasting.
  // We'll grudingly allow negative (btnid) but never zero. (zero means connect/disconnect to our listeners)
  if (devid<1) return;
  if (!btnid) return;
  
  // If this is a mapped button, apply it.
  int state=0;
  struct inmgr_device *device=inmgr_device_by_devid(devid);
  if (device) {
    struct inmgr_button *button=inmgr_button_by_srcbtnid(device,btnid);
    if (button) {
      inmgr_button_update(device,button,value);
    }
    state=device->state;
  }
  
  // Broadcast to listeners.
  inmgr_broadcast(devid,btnid,value,state);
}

/* Premapped event from client.
 */
 
void inmgr_artificial_event(int playerid,int btnid,int value) {
  if ((playerid<0)||(playerid>inmgr.playerc)) return;
  if (btnid&0xff000000) { // signal
    if (value) inmgr_signal(btnid);
  } else if (btnid&0x00ff0000) { // extbtn
    int extbtnp=inmgr_extbtnv_search(btnid);
    if (extbtnp<0) return;
    if (inmgr.playerv[playerid].extbtnv[extbtnp]==value) return;
    inmgr.playerv[playerid].extbtnv[extbtnp]=value;
    inmgr.playerv[0].extbtnv[extbtnp]=value;
  } else if (btnid) { // twostate
    if (value) {
      if (inmgr.playerv[playerid].state&btnid) return;
      inmgr.playerv[playerid].state|=btnid;
      inmgr.playerv[0].state|=btnid;
    } else {
      if (!(inmgr.playerv[playerid].state&btnid)) return;
      inmgr.playerv[playerid].state&=~btnid;
      inmgr.playerv[0].state&=~btnid;
    }
  }
}

void inmgr_force_player_state(int playerid,int state) {
  if ((playerid<0)||(playerid>inmgr.playerc)) return;
  inmgr.playerv[playerid].state=state;
}
