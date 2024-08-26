/* !!! mswm must be initialized first !!!
 */

#ifndef MSHID_INTERNAL_H
#define MSHID_INTERNAL_H

#include "opt/hostio/hostio_input.h"
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <stdint.h>
#include <windows.h>

// On my Nuc, this should not have the "ddk/" prefix. Dell, it should. No idea who is right.
#include <hidsdi.h>

// Dell's headers don't have this. (though it should be supported since Vista, and this is 7).
#ifndef RIDEV_DEVNOTIFY
  #define RIDEV_DEVNOTIFY 0x00002000
#endif
int HidD_GetProductString(HANDLE HIDHandle,char *buf,int bufc);

#define MSHID_BUTTON_LIMIT 128

struct mshid_device {
  HANDLE handle;
  int visited; // transient use during poll
  uint16_t vid,pid,version;
  int devid;
  char *name; // ASCII (despite being given to us in UTF-16)
  int namec;
  struct mshid_button {
    int size; // field size in bits: 1,2,4,8,16. little-endian if 16
    int usage; // full usage (page and code)
    int value; // last reported value
    int logmin; // only relevant for hats i think
    int logmax;
    int btnid; // DataIndex
  } *buttonv;
  int buttonc,buttona;
  int rptlen; // Expected report length in bytes; anything smaller will be ignored.
  void *preparsed;
  int value_by_btnid[MSHID_BUTTON_LIMIT];
  int btnidc;
};

struct hostio_input_mshid {
  struct hostio_input hdr;
  int poll_soon;
  struct mshid_device **devicev;
  int devicec,devicea;
};

#define DRIVER ((struct hostio_input_mshid*)driver)

extern struct hostio_input *mshid_global;

void mshid_device_del(struct mshid_device *device);
struct mshid_device *mshid_device_new();
const char *_mshid_get_ids(int *vid,int *pid,int *version,struct hostio_input *driver,int devid);
int _mshid_list_buttons(
  struct hostio_input *driver,
  int devid,
  int (*cb)(int btnid,int usage,int lo,int hi,int value,void *userdata),
  void *userdata
);
int mshid_device_set_name(struct mshid_device *device,const char *src,int srcc);
int mshid_device_set_name_utf16lez(struct mshid_device *device,const uint8_t *src);
int mshid_device_acquire_report_descriptors(struct mshid_device *device);
void mshid_device_receive_report(struct hostio_input *driver,struct mshid_device *device,const void *src,int srcc);

struct mshid_device *mshid_device_by_handle(struct hostio_input *driver,HANDLE h);
struct mshid_device *mshid_device_by_devid(struct hostio_input *driver,int devid);
int mshid_add_device(struct hostio_input *driver,struct mshid_device *device);

/* These two are called by mswm.
 */
void mshid_event(int wparam,int lparam);
void mshid_poll_connections();
void mshid_poll_connections_later();

/* Borrowed from mswm.
 */
HWND mswm_get_window_handle();

#endif
