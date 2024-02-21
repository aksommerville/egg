#include "evdev_internal.h"

/* Try connecting to file if it does look like evdev and is not already open etc.
 */
 
static int evdev_try_file(struct hostio_input *driver,const char *path) {

  const char *base=path;
  int i=0; for (;path[i];i++) {
    if (path[i]=='/') base=path+i+1;
  }
  if (memcmp(base,"event",5)) return 0;
  int kid=0;
  for (i=5;base[i];i++) {
    if ((base[i]<'0')||(base[i]>'9')) return 0;
    kid*=10;
    kid+=base[i]-'0';
  }
  if (evdev_device_by_kid(driver,kid)) return 0;
  
  int fd=open(path,O_RDONLY);
  if (fd<0) return 0;
  
  int grab=1;
  ioctl(fd,EVIOCGRAB,&grab);
  
  struct input_id iid={0};
  if (ioctl(fd,EVIOCGID,&iid)<0) {
    close(fd);
    return 0;
  }
  
  char nametmp[128];
  int nametmpc=ioctl(fd,EVIOCGNAME(sizeof(nametmp)),nametmp);
  if (nametmpc<0) {
    close(fd);
    return 0;
  }
  if ((nametmpc<1)||(nametmpc>sizeof(nametmp))) {
    nametmpc=snprintf(nametmp,sizeof(nametmp),"%04x:%04x",iid.vendor,iid.product);
  }
  
  struct evdev_device *device=evdev_device_add(driver);
  if (!device) {
    close(fd);
    return 0;
  }
  device->fd=fd;
  device->kid=kid;
  device->vid=iid.vendor;
  device->pid=iid.product;
  device->version=iid.version;
  if (device->name=malloc(nametmpc+1)) {
    memcpy(device->name,nametmp,nametmpc);
    device->name[nametmpc]=0;
  }
  
  if (driver->delegate.cb_connect) {
    driver->delegate.cb_connect(driver,device->devid);
  }
  return 0;
}

/* Scan directory for device.
 * This should only happen once, on the first update.
 * But we could re-poke it any time.
 */
 
int evdev_read_directory(struct hostio_input *driver) {
  DIR *dir=opendir("/dev/input");
  if (!dir) return -1;
  struct dirent *de;
  while (de=readdir(dir)) {
    if (de->d_type!=DT_CHR) continue;
    if (memcmp(de->d_name,"event",5)) continue;
    char path[128];
    int pathc=snprintf(path,sizeof(path),"/dev/input/%s",de->d_name);
    if ((pathc<1)||(pathc>=sizeof(path))) continue;
    evdev_try_file(driver,path);
  }
  closedir(dir);
  return 0;
}

/* Update inotify.
 * Clean up if anything goes wrong.
 */

void evdev_update_inotify(struct hostio_input *driver) {
  char buf[1024];
  int bufc=read(DRIVER->infd,buf,sizeof(buf));
  if (bufc<=0) {
    fprintf(stderr,"evdev:WARNING: Lost connection to inotify. Will not notice new joystick connections.\n");
    close(DRIVER->infd);
    DRIVER->infd=-1;
    return;
  }
  int bufp=0;
  while (1) {
    if (bufp>bufc-sizeof(struct inotify_event)) break;
    struct inotify_event *event=(struct inotify_event*)(buf+bufp);
    bufp+=sizeof(struct inotify_event);
    if (bufp>bufc-event->len) break;
    const char *base=buf+bufp;
    bufp+=event->len;
    int basec=0;
    while (basec&&!base[basec-1]) basec--;
    if ((basec<6)||memcmp(base,"event",5)) continue;
    char path[128];
    int pathc=snprintf(path,sizeof(path),"/dev/input/%.*s",basec,base);
    if ((pathc<1)||(pathc>=sizeof(path))) return;
    evdev_try_file(driver,path);
  }
}

/* Update one device.
 * Clean up if anything goes wrong.
 */
 
void evdev_update_device(struct hostio_input *driver,struct evdev_device *device) {
  struct input_event eventv[16];
  int eventc=read(device->fd,eventv,sizeof(eventv));
  if (eventc<=0) {
    close(device->fd);
    device->fd=-1;
    return;
  }
  eventc/=sizeof(struct input_event);
  struct input_event *event=eventv;
  for (;eventc-->0;event++) {
    switch (event->type) {
      case EV_KEY:
      case EV_ABS:
      case EV_REL:
      case EV_SW: {
          int btnid=(event->type<<16)|event->code;
          driver->delegate.cb_button(driver,device->devid,btnid,event->value);
        } break;
    }
  }
}

/* List buttons for a device.
 */

int evdev_list_buttons(
  int fd,
  int (*cb)(int btnid,int hidusage,int lo,int hi,int value,void *userdata),
  void *userdata
) {
  uint8_t bits[128];
  int bitc,err;
  
  if ((bitc=ioctl(fd,EVIOCGBIT(EV_KEY,sizeof(bits)),bits))>0) {
    int major=0; for (;major<bitc;major++) {
      if (!bits[major]) continue;
      int minor=0; for (;minor<8;minor++) {
        if (!(bits[major]&(1<<minor))) continue;
        int btnid=(EV_KEY<<16)|(major<<3)|minor;
        int usage=evdev_guess_hid_usage(btnid);
        if (err=cb(btnid,usage,0,1,0,userdata)) return err;
      }
    }
  }
  
  if ((bitc=ioctl(fd,EVIOCGBIT(EV_ABS,sizeof(bits)),bits))>0) {
    int major=0; for (;major<bitc;major++) {
      if (!bits[major]) continue;
      int minor=0; for (;minor<8;minor++) {
        if (!(bits[major]&(1<<minor))) continue;
        int btnid=(major<<3)|minor;
        struct input_absinfo absinfo={0};
        if (ioctl(fd,EVIOCGABS(btnid),&absinfo)<0) continue;
        btnid|=(EV_ABS<<16);
        int usage=evdev_guess_hid_usage(btnid);
        if (err=cb(btnid,usage,absinfo.minimum,absinfo.maximum,absinfo.value,userdata)) return err;
      }
    }
  }
  
  return 0;
}
