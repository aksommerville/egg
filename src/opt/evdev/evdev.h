/* evdev.h
 * Interface to Linux evdev unit for generic input.
 */
 
#ifndef EVDEV_H
#define EVDEV_H

struct evdev;
struct evdev_device;

/* Delegate is designed to match our 'hostio' unit, without imposing a dependency on it.
 */
struct evdev_delegate {
  void *userdata;
  void (*cb_connect)(struct evdev *evdev,struct evdev_device *device);
  void (*cb_disconnect)(struct evdev *evdev,struct evdev_device *device);
  void (*cb_button)(struct evdev *evdev,struct evdev_device *device,int type,int code,int value);
};

void evdev_del(struct evdev *evdev);

struct evdev *evdev_new(
  const char *path, // "/dev/input" if unset
  const struct evdev_delegate *delegate
);

void *evdev_get_userdata(const struct evdev *evdev);

/* All callbacks happen during update, nowhere else.
 * Use this simple update function to allow evdev to poll on its own.
 */
int evdev_update(struct evdev *evdev);

/* If you prefer to use your own poller, call these each cycle.
 * All files we report should poll with POLLIN; update them individually when the poll or fail.
 * Beware that files are not necessarily devices: There's normally one for inotify.
 */
int evdev_for_each_file(const struct evdev *evdev,int (*cb)(int fd,void *userdata),void *userdata);
int evdev_update_file(struct evdev *evdev,int fd);

/* Access to the full set of connected devices.
 * If you don't assign (devid), searching on it is pointless, but will return the first match.
 */
struct evdev_device *evdev_device_by_index(const struct evdev *evdev,int p);
struct evdev_device *evdev_device_by_devid(const struct evdev *evdev,int devid);
struct evdev_device *evdev_device_by_fd(const struct evdev *evdev,int fd);
struct evdev_device *evdev_device_by_kid(const struct evdev *evdev,int kid);

/* Yours to assign.
 */
int evdev_device_get_devid(const struct evdev_device *device);
void evdev_device_set_devid(struct evdev_device *device,int devid);

/* Close the underlying file and remove this device from our list.
 * Wise to do this when we find a device you know you don't need.
 * Some systems report a bunch of like front panel switches or HDMI control channels or other weirdness.
 */
void evdev_device_disconnect(struct evdev *evdev,struct evdev_device *device);

/* IDs are fetched before your connect callback; this retrieve is dirt cheap.
 * We always return a non-null name for valid devices. It may be empty or misencoded.
 */
const char *evdev_device_get_ids(int *vid,int *pid,int *version,const struct evdev_device *device);

/* Button declarations are *not* cached; this performs some I/O.
 * Calls (cb) for each KEY, ABS, REL, and SW event that the device claims to support.
 */
int evdev_device_for_each_button(
  struct evdev_device *device,
  int (*cb)(int type,int code,int hidusage,int lo,int hi,int value,void *userdata),
  void *userdata
);

int evdev_guess_hidusage(int type,int code);

#endif
