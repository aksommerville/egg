#include "mshid_internal.h"

struct hostio_input *mshid_global=0;

/* Delete.
 */
 
static void _mshid_del(struct hostio_input *driver) {
  if (driver==mshid_global) mshid_global=0;
  if (DRIVER->devicev) {
    while (DRIVER->devicec-->0) {
      mshid_device_del(DRIVER->devicev[DRIVER->devicec]);
    }
    free(DRIVER->devicev);
  }
}

/* Init.
 */

static int _mshid_init(struct hostio_input *driver,const struct hostio_input_setup *setup) {
  if (mshid_global) return -1;
  mshid_global=driver;
  
  HWND window=mswm_get_window_handle();
  if (!window) return -1;
  RAWINPUTDEVICE devv[]={
    {
      .usUsagePage=0x01, // desktop
      .usUsage=0x04, // joystick
      .dwFlags=RIDEV_DEVNOTIFY|RIDEV_INPUTSINK,
      .hwndTarget=window,
    },
    {
      .usUsagePage=0x01, // desktop
      .usUsage=0x05, // game pad
      .dwFlags=RIDEV_DEVNOTIFY|RIDEV_INPUTSINK,
      .hwndTarget=window,
    },
    {
      .usUsagePage=0x01, // desktop
      .usUsage=0x06, // keyboard
      .dwFlags=RIDEV_DEVNOTIFY|RIDEV_INPUTSINK,
      .hwndTarget=window,
    },
    {
      .usUsagePage=0x01, // desktop
      .usUsage=0x07, // keypad
      .dwFlags=RIDEV_DEVNOTIFY|RIDEV_INPUTSINK,
      .hwndTarget=window,
    },
    {
      .usUsagePage=0x01, // desktop
      .usUsage=0x08, // multi-axis controller, whatever that means
      .dwFlags=RIDEV_DEVNOTIFY|RIDEV_INPUTSINK,
      .hwndTarget=window,
    },
  };
  if (!RegisterRawInputDevices(devv,sizeof(devv)/sizeof(RAWINPUTDEVICE),sizeof(RAWINPUTDEVICE))) {
    fprintf(stderr,"Failed to register for raw input.\n");
    return -1;
  }
  
  DRIVER->poll_soon=1;
  
  return 0;
}

/* Update.
 */

static int _mshid_update(struct hostio_input *driver) {
  if (DRIVER->poll_soon) {
    DRIVER->poll_soon=0;
    mshid_poll_connections();
  }
  return 0;
}

void mshid_poll_connections_later() {
  struct hostio_input *driver=mshid_global;
  if (!driver) return;
  DRIVER->poll_soon=1;
}

/* Devid by index.
 */
 
static int _mshid_devid_by_index(const struct hostio_input *driver,int p) {
  if (p<0) return 0;
  if (p>=DRIVER->devicec) return 0;
  return DRIVER->devicev[p]->devid;
}

/* Type definition.
 */

const struct hostio_input_type hostio_input_type_mshid={
  .name="mshid",
  .desc="Joystick input for Windows",
  .objlen=sizeof(struct hostio_input_mshid),
  .del=_mshid_del,
  .init=_mshid_init,
  .update=_mshid_update,
  .devid_by_index=_mshid_devid_by_index,
  .get_ids=_mshid_get_ids,
  .for_each_button=_mshid_list_buttons,
};

/* Device list.
 */
 
struct mshid_device *mshid_device_by_handle(struct hostio_input *driver,HANDLE h) {
  struct mshid_device **p=DRIVER->devicev;
  int i=DRIVER->devicec;
  for (;i-->0;p++) {
    if ((*p)->handle==h) return *p;
  }
  return 0;
}

struct mshid_device *mshid_device_by_devid(struct hostio_input *driver,int devid) {
  struct mshid_device **p=DRIVER->devicev;
  int i=DRIVER->devicec;
  for (;i-->0;p++) {
    if ((*p)->devid==devid) return *p;
  }
  return 0;
}

int mshid_add_device(struct hostio_input *driver,struct mshid_device *device) {
  if (DRIVER->devicec>=DRIVER->devicea) {
    int na=DRIVER->devicea+8;
    if (na>INT_MAX/sizeof(void*)) return -1;
    void *nv=realloc(DRIVER->devicev,sizeof(void*)*na);
    if (!nv) return -1;
    DRIVER->devicev=nv;
    DRIVER->devicea=na;
  }
  DRIVER->devicev[DRIVER->devicec++]=device;
  return 0;
}
