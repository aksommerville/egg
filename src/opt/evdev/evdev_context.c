#include "evdev_internal.h"

/* Delete.
 */

void evdev_del(struct evdev *evdev) {
  if (!evdev) return;
  if (evdev->devicev) {
    while (evdev->devicec-->0) evdev_device_del(evdev->devicev[evdev->devicec]);
    free(evdev->devicev);
  }
  if (evdev->inofd>=0) close(evdev->inofd);
  if (evdev->path) free(evdev->path);
  if (evdev->pollfdv) free(evdev->pollfdv);
  free(evdev);
}

/* New.
 */

struct evdev *evdev_new(
  const char *path,
  const struct evdev_delegate *delegate
) {
  if (!path||!path[0]) path="/dev/input";
  struct evdev *evdev=calloc(1,sizeof(struct evdev));
  if (!evdev) return 0;
  
  evdev->inofd=-1;
  evdev->rescan=1;
  if (delegate) evdev->delegate=*delegate;
  
  if (!(evdev->path=strdup(path))) {
    evdev_del(evdev);
    return 0;
  }
  if ((evdev->inofd=inotify_init())>=0) {
    inotify_add_watch(evdev->inofd,evdev->path,IN_ATTRIB|IN_CREATE|IN_MOVED_TO);
  }
  
  return evdev;
}

/* Trivial accessors.
 */

void *evdev_get_userdata(const struct evdev *evdev) {
  if (!evdev) return 0;
  return evdev->delegate.userdata;
}

/* Access to device list.
 */
 
struct evdev_device *evdev_device_by_index(const struct evdev *evdev,int p) {
  if (p<0) return 0;
  if (!evdev) return 0;
  if (p>=evdev->devicec) return 0;
  return evdev->devicev[p];
}

struct evdev_device *evdev_device_by_devid(const struct evdev *evdev,int devid) {
  struct evdev_device **p=evdev->devicev;
  int i=evdev->devicec;
  for (;i-->0;p++) if ((*p)->devid==devid) return *p;
  return 0;
}

struct evdev_device *evdev_device_by_fd(const struct evdev *evdev,int fd) {
  struct evdev_device **p=evdev->devicev;
  int i=evdev->devicec;
  for (;i-->0;p++) if ((*p)->fd==fd) return *p;
  return 0;
}

struct evdev_device *evdev_device_by_kid(const struct evdev *evdev,int kid) {
  struct evdev_device **p=evdev->devicev;
  int i=evdev->devicec;
  for (;i-->0;p++) if ((*p)->kid==kid) return *p;
  return 0;
}

/* Add device (privateish).
 */
 
int evdev_add_device(struct evdev *evdev,struct evdev_device *device) {
  if (!evdev||!device) return -1;
  if (evdev->devicec>=evdev->devicea) {
    int na=evdev->devicea+16;
    if (na>INT_MAX/sizeof(void*)) return -1;
    void *nv=realloc(evdev->devicev,sizeof(void*)*na);
    if (!nv) return -1;
    evdev->devicev=nv;
    evdev->devicea=na;
  }
  evdev->devicev[evdev->devicec++]=device;
  return 0;
}

/* Disconnect device.
 */
 
void evdev_device_disconnect(struct evdev *evdev,struct evdev_device *device) {
  if (!evdev||!device) return;
  int p=0; for (;p<evdev->devicec;p++) {
    if (evdev->devicev[p]==device) {
      evdev->devicec--;
      memmove(evdev->devicev+p,evdev->devicev+p+1,sizeof(void*)*(evdev->devicec-p));
      evdev_device_del(device);
      return;
    }
  }
}
