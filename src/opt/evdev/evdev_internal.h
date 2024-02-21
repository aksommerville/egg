#ifndef EVDEV_INTERNAL_H
#define EVDEV_INTERNAL_H

#include "opt/hostio/hostio_input.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/poll.h>
#include <sys/inotify.h>
#include <linux/input.h>
#include <dirent.h>
#include <fcntl.h>

struct hostio_input_evdev {
  struct hostio_input hdr;
  int infd;
  struct evdev_device {
    int fd;
    int kid;
    int devid;
    uint16_t vid,pid,version;
    char *name;
  } *devicev;
  int devicec,devicea;
  struct pollfd *pollfdv;
  int pollfda;
  int rescan;
};

#define DRIVER ((struct hostio_input_evdev*)driver)

struct evdev_device *evdev_device_by_devid(const struct hostio_input *driver,int devid);
struct evdev_device *evdev_device_by_fd(const struct hostio_input *driver,int fd);
struct evdev_device *evdev_device_by_kid(const struct hostio_input *driver,int kid);
struct evdev_device *evdev_device_add(struct hostio_input *driver); // devid allocated; all else blank
// Remove a device by closing its file and set fd=-1. Don't trigger the callback.

int evdev_read_directory(struct hostio_input *driver);

/* These both drop the connection if anything fails.
 */
void evdev_update_inotify(struct hostio_input *driver);
void evdev_update_device(struct hostio_input *driver,struct evdev_device *device);

int evdev_list_buttons(
  int fd,
  int (*cb)(int btnid,int hidusage,int lo,int hi,int value,void *userdata),
  void *userdata
);

int evdev_guess_hid_usage(int btnid);

#endif
