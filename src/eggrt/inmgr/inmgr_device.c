#include "inmgr_internal.h"

/* Cleanup.
 */
 
void inmgr_device_cleanup(struct inmgr_device *device) {
  if (device->name) free(device->name);
  if (device->buttonv) free(device->buttonv);
}

/* Search device list.
 */
 
int inmgr_devicev_search(int devid) {
  int lo=0,hi=inmgr.devicec;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
    int q=inmgr.devicev[ck].devid;
         if (devid<q) hi=ck;
    else if (devid>q) lo=ck+1;
    else return ck;
  }
  return -lo-1;
}

struct inmgr_device *inmgr_device_by_devid(int devid) {
  int p=inmgr_devicev_search(devid);
  if (p<0) return 0;
  return inmgr.devicev+p;
}

/* Search button list in device.
 */
 
int inmgr_device_buttonv_search(const struct inmgr_device *device,int srcbtnid) {
  int lo=0,hi=device->buttonc;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
    int q=device->buttonv[ck].srcbtnid;
         if (srcbtnid<q) hi=ck;
    else if (srcbtnid>q) lo=ck+1;
    else return ck;
  }
  return -lo-1;
}

struct inmgr_button *inmgr_button_by_srcbtnid(const struct inmgr_device *device,int srcbtnid) {
  int p=inmgr_device_buttonv_search(device,srcbtnid);
  if (p<0) return 0;
  return device->buttonv+p;
}

/* Public device list inspection.
 */

int inmgr_devid_by_index(int p) {
  if (p<0) return 0;
  if (p>=inmgr.devicec) return 0;
  return inmgr.devicev[p].devid;
}

const char *inmgr_get_device_id(int *vid,int *pid,int *version,int devid) {
  const struct inmgr_device *device=inmgr_device_by_devid(devid);
  if (!device) return 0;
  if (vid) *vid=device->vid;
  if (pid) *pid=device->pid;
  if (version) *version=device->version;
  return device->name;
}

int inmgr_get_device_button(int *hidusage,int *lo,int *hi,int *value,int devid,int p) {
  if (p<0) return 0;
  struct inmgr_device *device=inmgr_device_by_devid(devid);
  if (!device) return 0;
  if (p>=device->buttonc) return 0;
  const struct inmgr_button *button=device->buttonv+p;
  if (hidusage) *hidusage=button->hidusage;
  if (lo) *lo=button->lo;
  if (hi) *hi=button->hi;
  if (value) *value=button->srcvalue;
  return button->srcbtnid;
}

int inmgr_get_dstbtnid(int devid,int srcbtnid) {
  struct inmgr_device *device=inmgr_device_by_devid(devid);
  if (!device) return 0;
  struct inmgr_button *button=inmgr_button_by_srcbtnid(device,srcbtnid);
  if (!button) return 0;
  return button->dstbtnid;
}

int inmgr_playerid_for_devid(int devid) {
  struct inmgr_device *device=inmgr_device_by_devid(devid);
  if (!device) return 0;
  return device->playerid;
}

/* Set name.
 */
 
int inmgr_device_set_name(struct inmgr_device *device,const char *src,int srcc) {
  if (!device) return -1;
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  char *nv=0;
  if (srcc) {
    if (!(nv=malloc(srcc+1))) return -1;
    memcpy(nv,src,srcc);
    nv[srcc]=0;
  }
  if (device->name) free(device->name);
  device->name=nv;
  device->namec=srcc;
  return 0;
}
