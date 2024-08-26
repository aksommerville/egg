/* evdev_hostio_glue.c
 * Orthogonal to the rest of evdev; you can drop this file cold if not using hostio.
 */
 
#include "evdev_internal.h" // Could do with the public interface, but it's more efficient with the internal.
#include "opt/hostio/hostio_input.h"

/* Instance definition.
 */
 
struct hostio_input_evdev {
  struct hostio_input hdr;
  struct evdev *evdev;
};

#define DRIVER ((struct hostio_input_evdev*)driver)

/* Delete.
 */
 
static void _evdev_del(struct hostio_input *driver) {
  evdev_del(DRIVER->evdev);
}

/* Callback wrappers.
 */
 
static void _evdev_cb_connect(struct evdev *evdev,struct evdev_device *device) {
  struct hostio_input *driver=evdev->delegate.userdata;
  if (!(device->devid=hostio_input_devid_next())) {
    evdev_device_disconnect(evdev,device);
  } else {
    driver->delegate.cb_connect(driver,device->devid);
  }
}

static void _evdev_cb_disconnect(struct evdev *evdev,struct evdev_device *device) {
  struct hostio_input *driver=evdev->delegate.userdata;
  driver->delegate.cb_disconnect(driver,device->devid);
}
  
static void _evdev_cb_button(struct evdev *evdev,struct evdev_device *device,int type,int code,int value) {
  struct hostio_input *driver=evdev->delegate.userdata;
  driver->delegate.cb_button(driver,device->devid,(type<<16)|code,value);
}

/* Init.
 */
 
static int _evdev_init(struct hostio_input *driver,const struct hostio_input_setup *setup) {
  struct evdev_delegate delegate={
    .userdata=driver,
    .cb_connect=_evdev_cb_connect,
    .cb_disconnect=_evdev_cb_disconnect,
    .cb_button=_evdev_cb_button,
  };
  if (!driver->delegate.cb_connect) delegate.cb_connect=0;
  if (!driver->delegate.cb_disconnect) delegate.cb_disconnect=0;
  if (!driver->delegate.cb_button) delegate.cb_button=0;
  if (!(DRIVER->evdev=evdev_new(setup->path,&delegate))) return -1;
  return 0;
}

/* Update.
 */

static int _evdev_update(struct hostio_input *driver) {
  return evdev_update(DRIVER->evdev);
}

/* Device by index.
 */
  
static int _evdev_devid_by_index(const struct hostio_input *driver,int p) {
  if ((p<0)||(p>=DRIVER->evdev->devicec)) return 0;
  return DRIVER->evdev->devicev[p]->devid;
}

/* Disconnect device.
 */
 
static void _evdev_disconnect(struct hostio_input *driver,int devid) {
  struct evdev_device *device=evdev_device_by_devid(DRIVER->evdev,devid);
  if (!device) return;
  evdev_device_disconnect(DRIVER->evdev,device);
}

/* Get device IDs.
 */
 
static const char *_evdev_get_ids(int *vid,int *pid,int *version,struct hostio_input *driver,int devid) {
  struct evdev_device *device=evdev_device_by_devid(DRIVER->evdev,devid);
  if (!device) return 0;
  if (vid) *vid=device->vid;
  if (pid) *pid=device->pid;
  if (version) *version=device->version;
  if (device->name) return device->name;
  return "";
}

/* Iterate buttons.
 */
 
struct evdev_for_each_button_context {
  int (*cb)(int btnid,int hidusage,int lo,int hi,int value,void *userdata);
  void *userdata;
};

static int _evdev_cb_for_each_button(
  int type,int code,int hidusage,int lo,int hi,int value,void *userdata
) {
  struct evdev_for_each_button_context *ctx=userdata;
  return ctx->cb((type<<16)|code,hidusage,lo,hi,value,ctx->userdata);
}

static int _evdev_for_each_button(
  struct hostio_input *driver,
  int devid,
  int (*cb)(int btnid,int hidusage,int lo,int hi,int value,void *userdata),
  void *userdata
) {
  struct evdev_device *device=evdev_device_by_devid(DRIVER->evdev,devid);
  if (!device) return 0;
  struct evdev_for_each_button_context ctx={.cb=cb,.userdata=userdata};
  return evdev_device_for_each_button(device,_evdev_cb_for_each_button,&ctx);
}

/* Type definition.
 */
 
const struct hostio_input_type hostio_input_type_evdev={
  .name="evdev",
  .desc="General input for Linux.",
  .objlen=sizeof(struct hostio_input_evdev),
  .appointment_only=0,
  .del=_evdev_del,
  .init=_evdev_init,
  .update=_evdev_update,
  .devid_by_index=_evdev_devid_by_index,
  .disconnect=_evdev_disconnect,
  .get_ids=_evdev_get_ids,
  .for_each_button=_evdev_for_each_button,
};
