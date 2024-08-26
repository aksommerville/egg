#include "evdev_internal.h"

/* Delete.
 */
 
void evdev_device_del(struct evdev_device *device) {
  if (!device) return;
  if (device->fd>=0) close(device->fd);
  if (device->name) free(device->name);
  free(device);
}

/* New.
 */
 
struct evdev_device *evdev_device_new() {
  struct evdev_device *device=calloc(1,sizeof(struct evdev_device));
  if (!device) return 0;
  device->fd=-1;
  return device;
}

/* Trivial accessors.
 */
 
int evdev_device_get_devid(const struct evdev_device *device) {
  if (!device) return 0;
  return device->devid;
}

void evdev_device_set_devid(struct evdev_device *device,int devid) {
  if (!device) return;
  device->devid=devid;
}

const char *evdev_device_get_ids(int *vid,int *pid,int *version,const struct evdev_device *device) {
  if (!device) return 0;
  if (vid) *vid=device->vid;
  if (pid) *pid=device->pid;
  if (version) *version=device->version;
  if (device->name) return device->name;
  return "";
}

/* Acquire IDs.
 */
 
int evdev_device_acquire_ids(struct evdev_device *device) {
  
  struct input_id id={0};
  if (ioctl(device->fd,EVIOCGID,&id)<0) return -1;
  device->vid=id.vendor;
  device->pid=id.product;
  device->version=id.version;
  
  char name[256];
  if (ioctl(device->fd,EVIOCGNAME(sizeof(name)),name)<0) return -1;
  name[sizeof(name)-1]=0;
  int namec=0; while (name[namec]) namec++;
  if (device->name) free(device->name);
  if (!(device->name=malloc(namec+1))) return -1;
  memcpy(device->name,name,namec+1);
  
  return 0;
}

/* Update.
 */

int evdev_device_update(struct evdev *evdev,struct evdev_device *device) {
  if (device->fd<0) return -1;
  struct input_event buf[32];
  int eventc=read(device->fd,buf,sizeof(buf));
  if (eventc<=0) {
    if (evdev->delegate.cb_disconnect) evdev->delegate.cb_disconnect(evdev,device);
    evdev_device_disconnect(evdev,device);
    return 0;
  }
  if (evdev->delegate.cb_button) {
    eventc/=sizeof(struct input_event);
    const struct input_event *event=buf;
    for (;eventc-->0;event++) {
      if (event->type==EV_SYN) continue;
      if (event->type==EV_MSC) continue;
      evdev->delegate.cb_button(evdev,device,event->type,event->code,event->value);
    }
  }
  return 0;
}

/* Iterate buttons.
 */
 
static void evdev_prepbtn_key(int *lo,int *hi,int *value,int fd,int code) {
  *lo=0;
  *hi=1;
  *value=0;
}

static void evdev_prepbtn_rel(int *lo,int *hi,int *value,int fd,int code) {
  *lo=-32767;
  *hi=32767;
  *value=0;
}

static void evdev_prepbtn_abs(int *lo,int *hi,int *value,int fd,int code) {
  struct input_absinfo ai={0};
  if (ioctl(fd,EVIOCGABS(code),&ai)<0) {
    *lo=-32767;
    *hi=32767;
    *value=0;
  } else {
    *lo=ai.minimum;
    *hi=ai.maximum;
    *value=ai.value;
  }
}
 
static int evdev_for_each_bit(
  struct evdev_device *device,
  int type,int codec,
  void (*prepbtn)(int *lo,int *hi,int *value,int fd,int code),
  int (*cb)(int type,int code,int hidusage,int lo,int hi,int value,void *userdata),
  void *userdata
) {
  uint8_t bits[256]={0};
  int bitsc=ioctl(device->fd,EVIOCGBIT(type,sizeof(bits)),bits);
  if ((bitsc<1)||(bitsc>sizeof(bits))) return 0;
  int major=0;
  for (;major<bitsc;major++) {
    uint8_t sub=bits[major];
    if (!sub) continue;
    uint8_t mask=1;
    int minor=0;
    for (;minor<8;minor++,mask<<=1) {
      if (!(sub&mask)) continue;
      int code=(major<<3)|minor;
      int err,lo,hi,value;
      prepbtn(&lo,&hi,&value,device->fd,code);
      int hidusage=evdev_guess_hidusage(type,code);
      if (err=cb(type,code,hidusage,lo,hi,value,userdata)) return err;
    }
  }
  return 0;
}
 
int evdev_device_for_each_button(
  struct evdev_device *device,
  int (*cb)(int type,int code,int hidusage,int lo,int hi,int value,void *userdata),
  void *userdata
) {
  int err;
  if (err=evdev_for_each_bit(device,EV_KEY,KEY_CNT,evdev_prepbtn_key,cb,userdata)) return err;
  if (err=evdev_for_each_bit(device,EV_REL,REL_CNT,evdev_prepbtn_rel,cb,userdata)) return err;
  if (err=evdev_for_each_bit(device,EV_ABS,ABS_CNT,evdev_prepbtn_abs,cb,userdata)) return err;
  return 0;
 }
