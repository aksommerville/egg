#include "machid_internal.h"
#include "opt/hostio/hostio_input.h"
#include <stdio.h>

/* Instance definition.
 */

struct hostio_input_machid {
  struct hostio_input hdr;
  struct machid *machid;
};

#define DRIVER ((struct hostio_input_machid*)driver)

/* Cleanup.
 */

static void _machid_del(struct hostio_input *driver) {
  machid_del(DRIVER->machid);
}

/* Callback glue.
 */

static int _machid_cb_devid_next(struct machid *machid) {
  return hostio_input_devid_next();
}

static void _machid_cb_connect(struct machid *machid,int devid) {
  struct hostio_input *driver=machid_get_userdata(machid);
  if (driver->delegate.cb_connect) driver->delegate.cb_connect(driver->delegate.userdata,devid);
}

static void _machid_cb_disconnect(struct machid *machid,int devid) {
  struct hostio_input *driver=machid_get_userdata(machid);
  if (driver->delegate.cb_disconnect) driver->delegate.cb_disconnect(driver->delegate.userdata,devid);
}

static void _machid_cb_button(struct machid *machid,int devid,int btnid,int value) {
  struct hostio_input *driver=machid_get_userdata(machid);
  if (driver->delegate.cb_button) driver->delegate.cb_button(driver->delegate.userdata,devid,btnid,value);
}

/* Init.
 */

static int _machid_init(struct hostio_input *driver,const struct hostio_input_setup *setup) {
  struct machid_delegate delegate={
    .userdata=driver,
    .devid_next=_machid_cb_devid_next,
    .connect=_machid_cb_connect,
    .disconnect=_machid_cb_disconnect,
    .button=_machid_cb_button,
  };
  if (!(DRIVER->machid=machid_new(&delegate))) return -1;
  return 0;
}

/* Update.
 */

static int _machid_update(struct hostio_input *driver) {
  return machid_update(DRIVER->machid,0.0f);
}

/* Device properties.
 */

static int _machid_devid_by_index(const struct hostio_input *driver,int devp) {
  if (devp<0) return 0;
  if (devp>=DRIVER->machid->devc) return 0;
  return DRIVER->machid->devv[devp].devid;
}

static const char *_machid_get_ids(int *vid,int *pid,int *version,struct hostio_input *driver,int devid) {
  const char *name=machid_get_ids(vid,pid,DRIVER->machid,devid);
  if (version) *version=0; // TODO machid device version, should just be a matter of naming it
  return name;
}

static int _machid_list_buttons(
  struct hostio_input *driver,
  int devid,
  int (*cb)(int btnid,int usage,int lo,int hi,int value,void *userdata),
  void *userdata
) {
  // lucky! machid has exactly the same shape as hostio for this.
  // (well, not all that big a coincidence, they're written by the same guy about six months apart...)
  return machid_enumerate(DRIVER->machid,devid,cb,userdata);
}

/* Type definition.
 */

const struct hostio_input_type hostio_input_type_machid={
  .name="machid",
  .desc="Joystick input for MacOS",
  .objlen=sizeof(struct hostio_input_machid),
  .del=_machid_del,
  .init=_machid_init,
  .update=_machid_update,
  .devid_by_index=_machid_devid_by_index,
  .get_ids=_machid_get_ids,
  .for_each_button=_machid_list_buttons,
};
