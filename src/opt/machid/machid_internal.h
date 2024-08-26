#ifndef MACHID_INTERNAL_H
#define MACHID_INTERNAL_H

#include "machid.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <IOKit/hid/IOHIDLib.h>

#define MACHID_RUNLOOP_MODE CFSTR("com.aksommerville.machid")

struct machid_btn {
  int btnid;
  int usage;
  int value;
  int lo,hi;
};

struct machid_dev {
  IOHIDDeviceRef obj;
  int devid;
  int vid,pid;
  char *name;
  struct machid_btn *btnv;
  int btnc,btna;
};

struct machid {
  IOHIDManagerRef hidmgr;
  struct machid_dev *devv; // Sorted by (obj)
  int devc,deva;
  struct machid_delegate delegate; // All callbacks are set, possibly with defaults.
  int error; // Sticky error from callbacks, reported at end of update.
  int devid_next; // if the default devid generator in play
};

int machid_devv_search(const struct machid *machid,int devid);
struct machid_dev *machid_devv_insert(struct machid *machid,int p,int devid);
struct machid_dev *machid_get_device_by_id(const struct machid *machid,int devid);
struct machid_dev *machid_get_device_by_IOHIDDeviceRef(const struct machid *machid,IOHIDDeviceRef obj);

/* Call once for new devices, after populating (dev->obj).
 * Populates (name) and (btnv).
 */
void machid_dev_setup(struct machid *machid,struct machid_dev *dev);

int machid_dev_btnv_search(const struct machid_dev *dev,int btnid);
struct machid_btn *machid_dev_btnv_add(struct machid_dev *dev,int btnid);
struct machid_btn *machid_dev_btnv_get(const struct machid_dev *dev,int btnid);

#endif
