#include "evdev_internal.h"

/* Cleanup.
 */
 
static void evdev_device_cleanup(struct evdev_device *device) {
  if (device->fd>=0) close(device->fd);
  if (device->name) free(device->name);
}
 
static void _evdev_del(struct hostio_input *driver) {
  if (DRIVER->infd>=0) close(DRIVER->infd);
  if (DRIVER->devicev) {
    while (DRIVER->devicec-->0) evdev_device_cleanup(DRIVER->devicev+DRIVER->devicec);
    free(DRIVER->devicev);
  }
  if (DRIVER->pollfdv) free(DRIVER->pollfdv);
}

/* Init.
 */
 
static int _evdev_init(struct hostio_input *driver,const struct hostio_input_setup *setup) {
  if ((DRIVER->infd=inotify_init())<0) return -1;
  if (inotify_add_watch(DRIVER->infd,"/dev/input",IN_ATTRIB|IN_CREATE|IN_MOVED_TO)<0) return -1;
  DRIVER->rescan=1;
  return 0;
}

/* Grow pollfdv.
 */
 
static int evdev_pollfdv_require(struct hostio_input *driver,int nc) {
  if (nc<=DRIVER->pollfda) return 0;
  int na=(nc+8)&~7;
  if (na>INT_MAX/sizeof(struct evdev_device)) return -1;
  void *nv=realloc(DRIVER->pollfdv,sizeof(struct evdev_device)*na);
  if (!nv) return -1;
  DRIVER->pollfdv=nv;
  DRIVER->pollfda=na;
  return 0;
}

/* Update.
 */
 
static int _evdev_update(struct hostio_input *driver) {

  if (DRIVER->rescan) {
    DRIVER->rescan=0;
    if (evdev_read_directory(driver)<0) return -1;
  }
  
  int i=DRIVER->devicec;
  while (i-->0) {
    struct evdev_device *device=DRIVER->devicev+i;
    if (device->fd<0) {
      int devid=device->devid;
      evdev_device_cleanup(device);
      DRIVER->devicec--;
      memmove(device,device+1,sizeof(struct evdev_device)*(DRIVER->devicec-i));
      if (driver->delegate.cb_disconnect) driver->delegate.cb_disconnect(driver,devid);
    }
  }

  if (evdev_pollfdv_require(driver,1+DRIVER->devicec)<0) return -1;
  int pollfdc=0;
  if (DRIVER->infd>=0) {
    struct pollfd *pollfd=DRIVER->pollfdv+pollfdc++;
    pollfd->fd=DRIVER->infd;
    pollfd->events=POLLIN|POLLERR|POLLHUP;
    pollfd->revents=0;
  }
  struct evdev_device *device=DRIVER->devicev;
  for (i=DRIVER->devicec;i-->0;device++) {
    struct pollfd *pollfd=DRIVER->pollfdv+pollfdc++;
    pollfd->fd=device->fd;
    pollfd->events=POLLIN|POLLERR|POLLHUP;
    pollfd->revents=0;
  }
  if (pollfdc<1) return 0;

  if (poll(DRIVER->pollfdv,pollfdc,0)<=0) return 0;

  struct pollfd *pollfd=DRIVER->pollfdv;
  for (;pollfdc-->0;pollfd++) {
    if (!pollfd->revents) continue;
    if (pollfd->fd==DRIVER->infd) {
      evdev_update_inotify(driver);
    } else {
      struct evdev_device *device=evdev_device_by_fd(driver,pollfd->fd);
      if (device) {
        evdev_update_device(driver,device);
      }
    }
  }

  return 0;
}

/* Device list (private).
 */
 
struct evdev_device *evdev_device_by_devid(const struct hostio_input *driver,int devid) {
  int i=DRIVER->devicec;
  struct evdev_device *device=DRIVER->devicev;
  for (;i-->0;device++) if (device->devid==devid) return device;
  return 0;
}

struct evdev_device *evdev_device_by_fd(const struct hostio_input *driver,int fd) {
  int i=DRIVER->devicec;
  struct evdev_device *device=DRIVER->devicev;
  for (;i-->0;device++) if (device->fd==fd) return device;
  return 0;
}

struct evdev_device *evdev_device_by_kid(const struct hostio_input *driver,int kid) {
  int i=DRIVER->devicec;
  struct evdev_device *device=DRIVER->devicev;
  for (;i-->0;device++) if (device->kid==kid) return device;
  return 0;
}

struct evdev_device *evdev_device_add(struct hostio_input *driver) {
  int devid=hostio_input_devid_next();
  if (devid<1) return 0;
  if (DRIVER->devicec>=DRIVER->devicea) {
    int na=DRIVER->devicea+8;
    if (na>INT_MAX/sizeof(struct evdev_device)) return 0;
    void *nv=realloc(DRIVER->devicev,sizeof(struct evdev_device)*na);
    if (!nv) return 0;
    DRIVER->devicev=nv;
    DRIVER->devicea=na;
  }
  struct evdev_device *device=DRIVER->devicev+DRIVER->devicec++;
  memset(device,0,sizeof(struct evdev_device));
  device->fd=-1;
  device->devid=devid;
  return device;
}

/* Public device accessors.
 */
 
static int _evdev_devid_by_index(const struct hostio_input *driver,int p) {
  if (p<0) return 0;
  if (p>=DRIVER->devicec) return 0;
  return DRIVER->devicev[p].devid;
}

static const char *_evdev_get_ids(
  int *vid,int *pid,int *version,
  struct hostio_input *driver,
  int devid
) {
  struct evdev_device *device=evdev_device_by_devid(driver,devid);
  if (!device) return 0;
  if (vid) *vid=device->vid;
  if (pid) *pid=device->pid;
  if (version) *version=device->version;
  return device->name;
}

/* Drop device.
 * TODO hostio doesn't expose this. Should it?
 */
 
static void _evdev_device_disconnect(struct hostio_input *driver,int devid) {
  int i=DRIVER->devicec;
  struct evdev_device *device=DRIVER->devicev+i-1;
  for (;i-->0;device--) {
    if (device->devid!=devid) continue;
    evdev_device_cleanup(device);
    DRIVER->devicec--;
    memmove(device,device+1,sizeof(struct evdev_device)*(DRIVER->devicec-i));
    if (driver->delegate.cb_disconnect) driver->delegate.cb_disconnect(driver,devid);
    return;
  }
}

/* Enumerate device buttons.
 */
 
static int _evdev_list_buttons(
  struct hostio_input *driver,
  int devid,
  int (*cb)(int btnid,int hidusage,int lo,int hi,int value,void *userdata),
  void *userdata
) {
  struct evdev_device *device=evdev_device_by_devid(driver,devid);
  if (!device) return 0;
  return evdev_list_buttons(device->fd,cb,userdata);
}

/* Type definition.
 */
 
const struct hostio_input_type hostio_input_type_evdev={
  .name="evdev",
  .desc="Linux general input.",
  .objlen=sizeof(struct hostio_input_evdev),
  .del=_evdev_del,
  .init=_evdev_init,
  .update=_evdev_update,
  .devid_by_index=_evdev_devid_by_index,
  .get_ids=_evdev_get_ids,
  .list_buttons=_evdev_list_buttons,
};
