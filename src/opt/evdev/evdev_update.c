#include "evdev_internal.h"

/* Check a file newly discovered in the devices directory.
 * Errors are for fatal context-wide problems only.
 * Not an evdev device, fails to open, etc, not an error.
 */
 
static int evdev_try_file(struct evdev *evdev,const char *base) {
  
  /* Basename must be "eventN" where N is the decimal kid.
   * Already got it open? Done.
   */
  if (memcmp(base,"event",5)||!base[5]) return 0;
  int kid=0,basep=5;
  for (;base[basep];basep++) {
    int digit=base[basep]-'0';
    if ((digit<0)||(digit>9)) return 0;
    kid*=10;
    kid+=digit;
    if (kid>=999999) return 0;
  }
  if (evdev_device_by_kid(evdev,kid)) return 0;
  
  /* On a typical desktop system, open will fail for most device. That's fine.
   * On consoles, it's more likely they will all open and we'll end up ignoring most of them. Also fine.
   */
  char path[1024];
  int pathc=snprintf(path,sizeof(path),"%s/%s",evdev->path,base);
  if ((pathc<=1)||(pathc>=sizeof(path))) return 0;
  int fd=open(path,O_RDONLY);
  if (fd<0) return 0;
  
  /* Provision the device object, hand off file to it, and attach it to the context eagerly.
   */
  struct evdev_device *device=evdev_device_new();
  if (!device) {
    close(fd);
    return -1;
  }
  device->fd=fd; // HANDOFF
  device->kid=kid;
  if (evdev_add_device(evdev,device)<0) {
    evdev_device_del(device);
    return -1;
  }
  
  /* Acquire IDs from kernel.
   * We could fail here if it's not a real evdev device, or for other reasons.
   */
  if (evdev_device_acquire_ids(device)<0) {
    evdev_device_disconnect(evdev,device);
    return 0;
  }

  /* Grab. No worries if this fails, but it does matter sometimes.
   * eg on my Pi, if we don't grab the keyboard, it keeps talking to the console while we're running.
   */
  ioctl(device->fd,EVIOCGRAB,1);
  
  /* Alert our owner.
   * Beware that owner may disconnect the device during this callback, which will delete it for real.
   * So this must be the last step here. (which it should be anyway)
   */
  if (evdev->delegate.cb_connect) {
    evdev->delegate.cb_connect(evdev,device);
  }
  return 0;
}

/* Scan directory.
 * This should only happen once, on the first update.
 * We could add a "poke" feature at any time, just set (evdev->rescan), and this runs again.
 */
 
static int evdev_scan(struct evdev *evdev) {
  DIR *dir=opendir(evdev->path);
  if (!dir) return -1;
  struct dirent *de;
  while (de=readdir(dir)) {
    if (de->d_type!=DT_CHR) continue;
    int err=evdev_try_file(evdev,de->d_name);
    if (err<0) {
      closedir(dir);
      return err;
    }
  }
  closedir(dir);
  return 0;
}

/* Update inotify.
 * Handles closure on errors. Errors reported out of here are fatal.
 */
 
static int evdev_update_inotify(struct evdev *evdev) {
  char tmp[1024];
  int tmpc=read(evdev->inofd,tmp,sizeof(tmp));
  if (tmpc<=0) {
    close(evdev->inofd);
    evdev->inofd=-1;
    return 0;
  }
  int tmpp=0;
  while (tmpp<=tmpc-sizeof(struct inotify_event)) {
    struct inotify_event *event=(struct inotify_event*)(tmp+tmpp);
    tmpp+=sizeof(struct inotify_event);
    if (tmpp>tmpc-event->len) break;
    tmpp+=event->len;
    const char *base=event->name;
    int err=evdev_try_file(evdev,base);
    if (err<0) return err;
  }
  return 0;
}

/* Iterate live files.
 */
 
int evdev_for_each_file(const struct evdev *evdev,int (*cb)(int fd,void *userdata),void *userdata) {
  int err;
  if (evdev->inofd>=0) {
    if (err=cb(evdev->inofd,userdata)) return err;
  }
  int p=0;
  for (;p<evdev->devicec;p++) {
    struct evdev_device *device=evdev->devicev[p];
    if (device->fd<0) continue;
    if (err=cb(device->fd,userdata)) return err;
  }
  return 0;
}

/* Update one file.
 */
 
int evdev_update_file(struct evdev *evdev,int fd) {
  if (fd<0) return 0;
  if (fd==evdev->inofd) return evdev_update_inotify(evdev);
  struct evdev_device *device=evdev_device_by_fd(evdev,fd);
  if (device) return evdev_device_update(evdev,device);
  return 0;
}

/* Drop any device which has run out of funk.
 */
 
static void evdev_drop_defunct_devices(struct evdev *evdev) {
  int p=evdev->devicec;
  while (p-->0) {
    struct evdev_device *device=evdev->devicev[p];
    if (device->fd>=0) continue;
    evdev->devicec--;
    memmove(evdev->devicev+p,evdev->devicev+p+1,sizeof(void*)*(evdev->devicec-p));
    if (evdev->delegate.cb_disconnect) evdev->delegate.cb_disconnect(evdev,device);
    evdev_device_del(device);
  }
}

/* Update, main.
 */
 
int evdev_update(struct evdev *evdev) {
  
  if (evdev->rescan) {
    evdev->rescan=0;
    if (evdev_scan(evdev)<0) return -1;
  }
  evdev_drop_defunct_devices(evdev);
  
  int pollfdc=evdev->devicec;
  if (evdev->inofd>=0) pollfdc++;
  if (pollfdc>evdev->pollfda) {
    int na=(pollfdc+16)&~15;
    if (na>INT_MAX/sizeof(struct pollfd)) return -1;
    void *nv=realloc(evdev->pollfdv,sizeof(struct pollfd)*na);
    if (!nv) return -1;
    evdev->pollfdv=nv;
    evdev->pollfda=na;
  }
  if (pollfdc<1) return 0;
  
  struct pollfd *p=evdev->pollfdv;
  if (evdev->inofd>=0) {
    p->fd=evdev->inofd;
    p->events=POLLIN|POLLERR|POLLHUP;
    p->revents=0;
    p++;
  }
  int i=evdev->devicec;
  struct evdev_device **device=evdev->devicev;
  for (;i-->0;p++,device++) {
    p->fd=(*device)->fd;
    p->events=POLLIN|POLLERR|POLLHUP;
    p->revents=0;
  }
  
  int err=poll(evdev->pollfdv,pollfdc,0);
  if (err<=0) return 0;
  
  for (p=evdev->pollfdv,i=pollfdc;i-->0;p++) {
    if (!p->revents) continue;
    if (evdev_update_file(evdev,p->fd)<0) {
      fprintf(stderr,"evdev_update_file: error\n");
      return -1;
    }
  }
  
  return 0;
}
