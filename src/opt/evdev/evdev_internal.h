#ifndef EVDEV_INTERNAL_H
#define EVDEV_INTERNAL_H

#include "evdev.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <dirent.h>
#include <sys/poll.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/input.h>
#include <sys/inotify.h>

struct evdev {
  struct evdev_delegate delegate;
  struct evdev_device **devicev;
  int devicec,devicea;
  char *path;
  int pathc;
  int inofd;
  struct pollfd *pollfdv;
  int pollfda;
  int rescan;
};

struct evdev_device {
  int fd;
  int devid;
  int kid;
  char *name;
  int vid,pid,version;
};

int evdev_add_device(struct evdev *evdev,struct evdev_device *device);
void evdev_device_del(struct evdev_device *device);
struct evdev_device *evdev_device_new();
int evdev_device_acquire_ids(struct evdev_device *device);

// Manages disconnect and removal on errors. <0 returned here is a serious fatal error.
int evdev_device_update(struct evdev *evdev,struct evdev_device *device);

#endif
