/* machid.h
 * Interface to input devices for MacOS.
 * Link: -framework IOKit
 */

#ifndef MACHID_H
#define MACHID_H

struct machid;

/* You may supply a devid_next() hook to generate device IDs.
 * If null, we take care of it.
 * (you'll want this if you have multiple input providers that get lumped together).
 * Valid devid are >0.
 * If you set (filter), return nonzero to proceed with connecting a device, based on vid/pid/usage.
 * If unset, we give you 0x00010004 Joystick and 0x00010005 Game Pad, and drop everything else.
 */
struct machid_delegate {
  void *userdata;
  int (*devid_next)(struct machid *machid);
  int (*filter)(struct machid *machid,int vid,int pid,int page,int usage);
  void (*connect)(struct machid *machid,int devid);
  void (*disconnect)(struct machid *machid,int devid);
  void (*button)(struct machid *machid,int devid,int btnid,int value);
};

void machid_del(struct machid *machid);
struct machid *machid_new(const struct machid_delegate *delegate);

void *machid_get_userdata(const struct machid *machid);

int machid_update(struct machid *machid,double timeout);

/* Fetch vendor ID, product ID, and name for one device.
 */
const char *machid_get_ids(int *vid,int *pid,struct machid *machid,int devid);

/* Call (cb) for each button reported by the device.
 * (btnid) is the ID your delegate will receive.
 * (usage) is a 32-bit HID Usage code if we can determine it.
 */
int machid_enumerate(
  struct machid *machid,
  int devid,
  int (*cb)(int btnid,int usage,int lo,int hi,int value,void *userdata),
  void *userdata
);

#endif
