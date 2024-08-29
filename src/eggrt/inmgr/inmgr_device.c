#include "inmgr_internal.h"

/* Delete.
 */
 
void inmgr_device_del(struct inmgr_device *device) {
  if (!device) return;
  if (device->buttonv) free(device->buttonv);
  free(device);
}

/* Get by devid.
 */
 
struct inmgr_device *inmgr_device_by_devid(const struct inmgr *inmgr,int devid) {
  int p=inmgr_devicev_search(inmgr,devid);
  if (p<0) return 0;
  return inmgr->devicev[p];
}

int inmgr_devicev_search(const struct inmgr *inmgr,int devid) {
  int lo=0,hi=inmgr->devicec;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
    const struct inmgr_device *q=inmgr->devicev[ck];
         if (devid<q->devid) hi=ck;
    else if (devid>q->devid) lo=ck+1;
    else return ck;
  }
  return -lo-1;
}

/* Spawn new device.
 */
 
struct inmgr_device *inmgr_devicev_spawn(struct inmgr *inmgr,int devid) {
  if (inmgr->devicec>=inmgr->devicea) {
    int na=inmgr->devicea+16;
    if (na>INT_MAX/sizeof(void*)) return 0;
    void *nv=realloc(inmgr->devicev,sizeof(void*)*na);
    if (!nv) return 0;
    inmgr->devicev=nv;
    inmgr->devicea=na;
  }
  int p=inmgr_devicev_search(inmgr,devid);
  if (p>=0) return 0;
  p=-p-1;
  struct inmgr_device *device=calloc(1,sizeof(struct inmgr_device));
  if (!device) return 0;
  device->devid=devid;
  memmove(inmgr->devicev+p+1,inmgr->devicev+p,sizeof(void*)*(inmgr->devicec-p));
  inmgr->devicec++;
  inmgr->devicev[p]=device;
  return device;
}

/* Search buttons in device.
 */

int inmgr_device_buttonv_search(const struct inmgr_device *device,int srcbtnid) {
  if (!device) return -1;
  int lo=0,hi=device->buttonc;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
    const struct inmgr_device_button *button=device->buttonv+ck;
         if (srcbtnid<button->srcbtnid) hi=ck;
    else if (srcbtnid>button->srcbtnid) lo=ck+1;
    else {
      while ((ck>lo)&&(button[-1].srcbtnid==srcbtnid)) { ck--; button--; }
      return ck;
    }
  }
  return -lo-1;
}
