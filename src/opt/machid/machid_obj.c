#include "machid_internal.h"

/* Delete context.
 */

static void machid_dev_cleanup(struct machid_dev *dev) {
  if (dev->name) free(dev->name);
  if (dev->btnv) free(dev->btnv);
  if (dev->obj) IOHIDDeviceClose(dev->obj,0);
}
 
void machid_del(struct machid *machid) {
  if (!machid) return;

  if (machid->hidmgr) {
    IOHIDManagerUnscheduleFromRunLoop(machid->hidmgr,CFRunLoopGetCurrent(),MACHID_RUNLOOP_MODE);
    IOHIDManagerClose(machid->hidmgr,0);
  }
  
  if (machid->devv) {
    while (machid->devc-->0) machid_dev_cleanup(machid->devv+machid->devc);
    free(machid->devv);
  }
  
  free(machid);
}

/* Get integer field from IOKit device.
 */

static int machid_iokit_get_int(IOHIDDeviceRef dev,CFStringRef k) {
  CFNumberRef vobj=IOHIDDeviceGetProperty(dev,k);
  if (!vobj) return 0;
  if (CFGetTypeID(vobj)!=CFNumberGetTypeID()) return 0;
  int v=0;
  CFNumberGetValue(vobj,kCFNumberIntType,&v);
  return v;
}

/* DeviceMatching.
 */

static void machid_cb_DeviceMatching(void *context,IOReturn result,void *sender,IOHIDDeviceRef obj) {
  struct machid *machid=context;

  if (machid_get_device_by_IOHIDDeviceRef(machid,obj)) return;
  
  int vid=machid_iokit_get_int(obj,CFSTR(kIOHIDVendorIDKey));
  int pid=machid_iokit_get_int(obj,CFSTR(kIOHIDProductIDKey));
  int usagepage=machid_iokit_get_int(obj,CFSTR(kIOHIDPrimaryUsagePageKey));
  int usage=machid_iokit_get_int(obj,CFSTR(kIOHIDPrimaryUsageKey));

  if (!machid->delegate.filter(machid,vid,pid,usagepage,usage)) {
    IOHIDDeviceClose(obj,0);
    return;
  }

  int devid=machid->delegate.devid_next(machid);
  if (devid<=0) {
    IOHIDDeviceClose(obj,0);
    return;
  }

  int p=machid_devv_search(machid,devid);
  if (p>=0) {
    IOHIDDeviceClose(obj,0);
    return;
  }
  p=-p-1;

  struct machid_dev *dev=machid_devv_insert(machid,p,devid);
  if (!dev) {
    IOHIDDeviceClose(obj,0);
    return;
  }

  dev->obj=obj;
  dev->vid=vid;
  dev->pid=pid;
  machid_dev_setup(machid,dev);

  machid->delegate.connect(machid,devid);
}

/* DeviceRemoval.
 */

static void machid_cb_DeviceRemoval(void *context,IOReturn result,void *sender,IOHIDDeviceRef obj) {
  struct machid *machid=context;
  struct machid_dev *dev=machid_get_device_by_IOHIDDeviceRef(machid,obj);
  if (!dev) return;
  int p=dev-machid->devv;
  if ((p<0)||(p>=machid->devc)) return;

  int devid=dev->devid;
  machid_dev_cleanup(dev);
  machid->devc--;
  memmove(dev,dev+1,sizeof(struct machid_dev)*(machid->devc-p));
  machid->delegate.disconnect(machid,devid);
}

/* InputValue.
 */

static void machid_cb_InputValue(void *context,IOReturn result,void *sender,IOHIDValueRef value) {
  struct machid *machid=context;
  
  IOHIDElementRef element=IOHIDValueGetElement(value);
  int btnid=IOHIDElementGetCookie(element);
  if (btnid<0) return;
  IOHIDDeviceRef obj=IOHIDElementGetDevice(element);
  if (!obj) return;
  struct machid_dev *dev=machid_get_device_by_IOHIDDeviceRef(machid,obj);
  if (!dev) return;
  struct machid_btn *btn=machid_dev_btnv_get(dev,btnid);
  if (!btn) return;

  CFIndex v=IOHIDValueGetIntegerValue(value);
  if (v==btn->value) return;
  btn->value=v;
  machid->delegate.button(machid,dev->devid,btnid,v);
}

/* Default callbacks.
 * Only devid_next and filter are at all interesting.
 */

static int machid_devid_next_default(struct machid *machid) {
  if (machid->devid_next>=INT_MAX) return -1;
  return machid->devid_next++;
}

static int machid_filter_default(struct machid *machid,int vid,int pid,int page,int usage) {
  if (page==0x0001) {
    if (usage==0x0004) return 1; // Joystick
    if (usage==0x0005) return 1; // Game Pad
  }
  return 0;
}

static void machid_connect_default(struct machid *machid,int devid) {}
static void machid_disconnect_default(struct machid *machid,int devid) {}
static void machid_button_default(struct machid *machid,int devid,int btnid,int value) {}

/* New context.
 */
    
struct machid *machid_new(const struct machid_delegate *delegate) {
  struct machid *machid=calloc(1,sizeof(struct machid));
  if (!machid) return 0;

  if (delegate) machid->delegate=*delegate;
  if (!machid->delegate.devid_next) machid->delegate.devid_next=machid_devid_next_default;
  if (!machid->delegate.filter) machid->delegate.filter=machid_filter_default;
  if (!machid->delegate.connect) machid->delegate.connect=machid_connect_default;
  if (!machid->delegate.disconnect) machid->delegate.disconnect=machid_disconnect_default;
  if (!machid->delegate.button) machid->delegate.button=machid_button_default;
  
  if (!(machid->hidmgr=IOHIDManagerCreate(kCFAllocatorDefault,0))) {
    machid_del(machid);
    return 0;
  }

  IOHIDManagerRegisterDeviceMatchingCallback(machid->hidmgr,machid_cb_DeviceMatching,machid);
  IOHIDManagerRegisterDeviceRemovalCallback(machid->hidmgr,machid_cb_DeviceRemoval,machid);
  IOHIDManagerRegisterInputValueCallback(machid->hidmgr,machid_cb_InputValue,machid);

  IOHIDManagerSetDeviceMatching(machid->hidmgr,0); // match every HID

  IOHIDManagerScheduleWithRunLoop(machid->hidmgr,CFRunLoopGetCurrent(),MACHID_RUNLOOP_MODE);
  
  /* This fails -536870174 on my iMac, but it works fine if we just don't call.
   * Confirm it works on the MacBook without this.
   * ...it does not; the MacBook needs this bit.
   */
  int err=IOHIDManagerOpen(machid->hidmgr,0);
  if (err==-536870174) {
    fprintf(stderr,"IOHIDManagerOpen failed but proceeding anyway.\n");
  } else if (err<0) {
    fprintf(stderr,"IOHIDManagerOpen: %d\n",err);
    machid_del(machid);
    return 0;
  }
  /**/

  return machid;
}

/* Update.
 */

int machid_update(struct machid *machid,double timeout) {
  if (!machid) return -1;
  CFRunLoopRunInMode(MACHID_RUNLOOP_MODE,timeout,0);
  if (machid->error) {
    int result=machid->error;
    machid->error=0;
    return result;
  }
  return 0;
}

/* Public accessors.
 */
 
void *machid_get_userdata(const struct machid *machid) {
  if (!machid) return 0;
  return machid->delegate.userdata;
}

const char *machid_get_ids(int *vid,int *pid,struct machid *machid,int devid) {
  struct machid_dev *device=machid_get_device_by_id(machid,devid);
  if (!device) return 0;
  if (vid) *vid=device->vid;
  if (pid) *pid=device->pid;
  if (device->name) return device->name;
  return "";
}

int machid_enumerate(
  struct machid *machid,
  int devid,
  int (*cb)(int btnid,int usage,int lo,int hi,int value,void *userdata),
  void *userdata
) {
  if (!cb) return -1;
  struct machid_dev *device=machid_get_device_by_id(machid,devid);
  if (!device) return 0;
  const struct machid_btn *btn=device->btnv;
  int i=device->btnc,err;
  for (;i-->0;btn++) {
    if (err=cb(btn->btnid,btn->usage,btn->lo,btn->hi,btn->value,userdata)) return err;
  }
  return 0;
}

/* Private device list accessors.
 */

int machid_devv_search(const struct machid *machid,int devid) {
  if (!machid) return -1;
  int lo=0,hi=machid->devc;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
    const struct machid_dev *dev=machid->devv+ck;
         if (devid<dev->devid) hi=ck;
    else if (devid>dev->devid) lo=ck+1;
    else return ck;
  }
  return -lo-1;
}

struct machid_dev *machid_devv_insert(struct machid *machid,int p,int devid) {
  if ((p<0)||(p>machid->devc)) return 0;
  if (p&&(devid<=machid->devv[p-1].devid)) return 0;
  if ((p<machid->devc)&&(devid=machid->devv[p].devid)) return 0;

  if (machid->devc>=machid->deva) {
    int na=machid->deva+8;
    if (na>INT_MAX/sizeof(struct machid_dev)) return 0;
    void *nv=realloc(machid->devv,sizeof(struct machid_dev)*na);
    if (!nv) return 0;
    machid->devv=nv;
    machid->deva=na;
  }

  struct machid_dev *dev=machid->devv+p;
  memmove(dev+1,dev,sizeof(struct machid_dev)*(machid->devc-p));
  machid->devc++;
  memset(dev,0,sizeof(struct machid_dev));
  dev->devid=devid;

  return dev;
}
 
struct machid_dev *machid_get_device_by_id(const struct machid *machid,int devid) {
  int p=machid_devv_search(machid,devid);
  if (p<0) return 0;
  return machid->devv+p;
}

struct machid_dev *machid_get_device_by_IOHIDDeviceRef(const struct machid *machid,IOHIDDeviceRef obj) {
  if (!machid) return 0;
  struct machid_dev *dev=machid->devv;
  int i=machid->devc;
  for (;i-->0;dev++) {
    if (dev->obj==obj) return dev;
  }
  return 0;
}
