#include "inmgr_internal.h"

struct inmgr inmgr={0};

/* Quit.
 */
 
void inmgr_quit() {
  if (inmgr.devicev) {
    while (inmgr.devicec-->0) inmgr_device_cleanup(inmgr.devicev+inmgr.devicec);
    free(inmgr.devicev);
  }
  if (inmgr.tmv) {
    while (inmgr.tmc-->0) inmgr_tm_cleanup(inmgr.tmv+inmgr.tmc);
    free(inmgr.tmv);
  }
  if (inmgr.signalv) free(inmgr.signalv);
  if (inmgr.listenerv) free(inmgr.listenerv);
  memset(&inmgr,0,sizeof(struct inmgr));
}

/* Init.
 */
 
int inmgr_init() {
  if (inmgr.init) return -1;
  inmgr.init=1;
  inmgr.btnmask=0xffff;
  inmgr.playerc=1;
  inmgr.listenerid_next=1;
  if (inmgr_load()<0) {
    inmgr_quit();
    return -1;
  }
  return 0;
}

/* Set player count.
 */

int inmgr_set_player_count(int playerc) {
  if (!inmgr.init) return -1;
  if (playerc<1) playerc=1; else if (playerc>INMGR_PLAYER_LIMIT) playerc=INMGR_PLAYER_LIMIT;
  if (playerc==inmgr.playerc) return playerc;
  
  // Zero the entire player state, we're going to rebuild it from scratch.
  memset(inmgr.playerv,0,sizeof(inmgr.playerv));
  
  // Retain the first assignment to each playerid if it's in the new range. Zero all others.
  int count_by_playerid[1+INMGR_PLAYER_LIMIT]={0};
  struct inmgr_device *device=inmgr.devicev;
  int i=inmgr.devicec;
  for (;i-->0;device++) {
    if (device->keyboard) continue;
    if ((device->enable<=0)||(device->playerid>playerc)||count_by_playerid[device->playerid]) {
      device->playerid=0;
    } else {
      count_by_playerid[device->playerid]=1;
    }
  }
  
  // Any devices that now have a zero playerid, map them to the least popular playerid.
  for (device=inmgr.devicev,i=inmgr.devicec;i-->0;device++) {
    if (device->keyboard) {
      device->playerid=1;
    } else if (device->playerid) {
      // Still valid. No change.
    } else {
      int playerid=1,q=2;
      for (;q<=playerc;q++) if (count_by_playerid[q]<count_by_playerid[playerid]) playerid=q;
      device->playerid=playerid;
      count_by_playerid[playerid]++;
    }
    if (device->enable>0) { // Disabled devices do still map, but don't capture their state.
      inmgr.playerv[device->playerid].state|=device->state;
      inmgr.playerv[0].state|=device->state;
    }
    // Extended buttons stay zero, because we don't know which device has the latest value.
  }
  
  inmgr.playerc=playerc;
  return playerc;
}

int inmgr_get_player_count() {
  return inmgr.playerc;
}

/* Set button mask.
 */
 
int inmgr_set_button_mask(int mask) {
  if (!inmgr.init) return -1;
  inmgr.btnmask=mask&0xffff;
  return inmgr.btnmask;
}

/* Search extbtn registry.
 */
 
int inmgr_extbtnv_search(int btnid) {
  int lo=0,hi=inmgr.extbtnc;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
    int q=inmgr.extbtnv[ck].btnid;
         if (btnid<q) hi=ck;
    else if (btnid>q) lo=ck+1;
    else return ck;
  }
  return -lo-1;
}

/* Declare extbtn.
 */
 
int inmgr_set_extbtn(int btnid,int lo,int hi) {
  if (!inmgr.init) return -1;
  if (btnid&0xff000000) return -1;
  if (!(btnid&0x00ff0000)) return -1;
  int p=inmgr_extbtnv_search(btnid);
  if (p>=0) {
    inmgr.extbtnv[p].lo=lo;
    inmgr.extbtnv[p].hi=hi;
  } else {
    if (inmgr.extbtnc>=INMGR_EXTBTN_LIMIT) return -1;
    p=-p-1;
    struct inmgr_extbtn *extbtn=inmgr.extbtnv+p;
    memmove(extbtn+1,extbtn,sizeof(struct inmgr_extbtn)*(inmgr.extbtnc-p));
    inmgr.extbtnc++;
    extbtn->btnid=btnid;
    extbtn->lo=lo;
    extbtn->hi=hi;
  }
  return 0;
}

/* Search signal handlers.
 */
 
int inmgr_signalv_search(int btnid) {
  int lo=0,hi=inmgr.signalc;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
    int q=inmgr.signalv[ck].btnid;
         if (btnid<q) hi=ck;
    else if (btnid>q) lo=ck+1;
    else return ck;
  }
  return -lo-1;
}

void inmgr_signal(int btnid) {
  int p=inmgr_signalv_search(btnid);
  if (p<0) return;
  inmgr.signalv[p].cb();
}

/* Declare signal.
 */
 
int inmgr_set_signal(int btnid,void (*cb)()) {
  if (!inmgr.init) return -1;
  if (!(btnid&0xff000000)) return -1;
  if (!cb) return -1;
  int p=inmgr_signalv_search(btnid);
  if (p>=0) {
    inmgr.signalv[p].cb=cb;
  } else {
    p=-p-1;
    if (inmgr.signalc>=inmgr.signala) {
      int na=inmgr.signala+16;
      if (na>INT_MAX/sizeof(struct inmgr_signal)) return -1;
      void *nv=realloc(inmgr.signalv,sizeof(struct inmgr_signal)*na);
      if (!nv) return -1;
      inmgr.signalv=nv;
      inmgr.signala=na;
    }
    struct inmgr_signal *signal=inmgr.signalv+p;
    memmove(signal+1,signal,sizeof(struct inmgr_signal)*(inmgr.signalc-p));
    inmgr.signalc++;
    signal->btnid=btnid;
    signal->cb=cb;
  }
  return 0;
}

/* Get player state.
 */

int inmgr_get_player(int playerid) {
  if ((playerid<0)||(playerid>inmgr.playerc)) return 0;
  return inmgr.playerv[playerid].state;
}

int inmgr_get_button(int playerid,int btnid) {
  if ((playerid<0)||(playerid>inmgr.playerc)) return 0;
  int extbtnp=inmgr_extbtnv_search(btnid);
  if (extbtnp<0) return 0;
  return inmgr.playerv[playerid].extbtnv[extbtnp];
}

/* Enable/disable device.
 */

void inmgr_device_enable(int devid,int enable) {
  struct inmgr_device *device=inmgr_device_by_devid(devid);
  if (!device) return;
  if (enable) {
    if (device->enable>0) return;
    device->enable=1;
    inmgr.playerv[device->playerid].state|=device->state;
    inmgr.playerv[0].state|=device->state;
    // extbtn do not immediately sync.
  } else {
    if (device->enable<=0) return;
    device->enable=0;
    inmgr.playerv[device->playerid].state=0;
    inmgr.playerv[0].state=0;
    memset(inmgr.playerv[device->playerid].extbtnv,0,sizeof(int)*inmgr.extbtnc);
    memset(inmgr.playerv[0].extbtnv,0,sizeof(int)*inmgr.extbtnc);
    const struct inmgr_device *other=inmgr.devicev;
    int i=inmgr.devicec;
    for (;i-->0;other++) {
      if (other->enable<=0) continue;
      inmgr.playerv[other->playerid].state|=other->state;
      inmgr.playerv[0].state|=other->state;
    }
  }
}

/* Raw event listeners.
 */

int inmgr_listen(void (*cb)(int devid,int btnid,int value,int state,void *userdata),void *userdata) {
  if (!inmgr.init) return -1;
  if (!cb) return -1;
  if (inmgr.listenerid_next>=INT_MAX) return -1;
  if (inmgr.listenerc>=inmgr.listenera) {
    int na=inmgr.listenera+8;
    if (na>INT_MAX/sizeof(struct inmgr_listener)) return -1;
    void *nv=realloc(inmgr.listenerv,sizeof(struct inmgr_listener)*na);
    if (!nv) return -1;
    inmgr.listenerv=nv;
    inmgr.listenera=na;
  }
  struct inmgr_listener *listener=inmgr.listenerv+inmgr.listenerc++;
  listener->listenerid=inmgr.listenerid_next++;
  listener->cb=cb;
  listener->userdata=userdata;
  return listener->listenerid;
}

void inmgr_unlisten(int listenerid) {
  int i=inmgr.listenerc;
  struct inmgr_listener *listener=inmgr.listenerv+i-1;
  for (;i-->0;listener--) {
    if (listener->listenerid==listenerid) {
      inmgr.listenerc--;
      memmove(listener,listener+1,sizeof(struct inmgr_listener)*(inmgr.listenerc-i));
      return;
    }
  }
}

void inmgr_broadcast(int devid,int btnid,int value,int state) {
  // Important to run backward: Listeners are allowed to remove themselves during the callback.
  int i=inmgr.listenerc;
  struct inmgr_listener *listener=inmgr.listenerv+i-1;
  for (;i-->0;listener--) {
    listener->cb(devid,btnid,value,state,listener->userdata);
  }
}

/* "Override all", a cheesy hack for Egg incfg.
 */
 
int inmgr_override_all(void (*cb)(int devid,int btnid,int value,int state,void *userdata),void *userdata) {
  if (cb) {
    if (inmgr.override_listenerid>0) return -1; // One at a time, please.
    if ((inmgr.override_listenerid=inmgr_listen(cb,userdata))<=0) return -1;
    struct inmgr_device *device=inmgr.devicev;
    int i=inmgr.devicec;
    for (;i-->0;device++) {
      if (device->enable==1) device->enable=-1;
    }
  } else {
    if (inmgr.override_listenerid<=0) return 0;
    inmgr_unlisten(inmgr.override_listenerid);
    inmgr.override_listenerid=0;
    struct inmgr_device *device=inmgr.devicev;
    int i=inmgr.devicec;
    for (;i-->0;device++) {
      if (device->enable<0) device->enable=1;
    }
  }
  return 0;
}
