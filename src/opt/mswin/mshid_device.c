#include "mshid_internal.h"

/* Delete.
 */

void mshid_device_del(struct mshid_device *device) {
  if (!device) return;

  if (device->name) free(device->name);
  if (device->buttonv) free(device->buttonv);
  if (device->preparsed) free(device->preparsed);

  free(device);
}

/* New.
 */

struct mshid_device *mshid_device_new() {
  struct mshid_device *device=calloc(1,sizeof(struct mshid_device));
  if (!device) return 0;

  return device;
}

/* Set name.
 */

int mshid_device_set_name(struct mshid_device *device,const char *src,int srcc) {
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  while (srcc&&((unsigned char)src[srcc-1]<=0x20)) srcc--;
  while (srcc&&((unsigned char)src[0]<=0x20)) { srcc--; src++; }
  char *nv=0;
  if (srcc) {
    if (!(nv=malloc(srcc+1))) return -1;
    memcpy(nv,src,srcc);
    nv[srcc]=0;
  }
  if (device->name) free(device->name);
  device->name=nv;
  device->namec=srcc;
  return 0;
}
 
int mshid_device_set_name_utf16lez(struct mshid_device *device,const uint8_t *src) {
  if (!device) return -1;
  char tmp[256];
  int srcp=0,tmpc=0;
  while (1) {
    int ch=src[srcp]|(src[srcp+1]<<8);
    if (!ch) break;
    srcp+=2;
    if ((ch>=0xd000)&&(ch<0xd800)) {
      int lo=src[srcp]|(src[srcp+1]<<8);
      if ((lo>=0xd800)&&(lo<0xe000)) {
        srcp+=2;
        ch=0x10000+((ch&0x3ff)<<10)+(lo&0x3ff);
      }
    }
    if ((ch<0x20)||(ch>0x7e)) tmp[tmpc++]='?';
    else tmp[tmpc++]=ch;
    if (tmpc>=sizeof(tmp)) break;
  }
  return mshid_device_set_name(device,tmp,tmpc);
}

/* Get IDs.
 */

const char *_mshid_get_ids(int *vid,int *pid,int *version,struct hostio_input *driver,int devid) {
  struct mshid_device *device=mshid_device_by_devid(driver,devid);
  if (!device) return 0;
  *vid=device->vid;
  *pid=device->pid;
  *version=device->version;
  return device->name;
}

/* Iterate buttons on one device.
 */

int _mshid_list_buttons(
  struct hostio_input *driver,
  int devid,
  int (*cb)(int btnid,int usage,int lo,int hi,int value,void *userdata),
  void *userdata
) {
  struct mshid_device *device=mshid_device_by_devid(driver,devid);
  if (!device) return 0;
  const struct mshid_button *button=device->buttonv;
  int i=device->buttonc,err;
  for (;i-->0;button++) {
    int lo=button->logmin;
    int hi;
    int rest=0;
    switch (button->size) {
      case 1: hi=lo+1; break;
      case 2: hi=lo+3; break;
      case 4: hi=lo+7; break;
      case 8: hi=lo+255; break;
      case 16: hi=lo+65535; if (!lo) rest=0x8000; break;
      default: continue;
    }
    if (err=cb(button->btnid,button->usage,lo,hi,rest,userdata)) return err;
  }
  return 0;
}

/* Add button from descriptor.
 */

static struct mshid_button *mshid_device_add_button(struct mshid_device *device) {
  if (device->buttonc>=device->buttona) {
    int na=device->buttona+16;
    if (na>INT_MAX/sizeof(struct mshid_button)) return 0;
    void *nv=realloc(device->buttonv,sizeof(struct mshid_button)*na);
    if (!nv) return 0;
    device->buttonv=nv;
    device->buttona=na;
  }
  struct mshid_button *button=device->buttonv+device->buttonc++;
  memset(button,0,sizeof(struct mshid_button));
  button->btnid=device->buttonc;
  return button;
}

/* Apply report descriptor (during setup).
 */

static int mshid_device_apply_preparsed(struct mshid_device *device,const uint8_t *src,int srcc) {
  int err;
  fprintf(stderr,"%s...\n",__func__);

  HIDP_BUTTON_CAPS bcaps[256];
  unsigned long bcapc=256;
  HidP_GetButtonCaps(HidP_Input,bcaps,&bcapc,(void*)src);
  if ((bcapc>0)&&(bcapc<256)) {
    HIDP_BUTTON_CAPS *cap=bcaps;
    int i=bcapc;
    for (;i-->0;cap++) {
      if (cap->IsRange) {
        int di=cap->Range.DataIndexMin;
        int u=cap->Range.UsageMin;
        for (;di<=cap->Range.DataIndexMax;di++,u++) {
          if (di>=MSHID_BUTTON_LIMIT) break;
          struct mshid_button *button=mshid_device_add_button(device);
          if (!button) return -1;
          button->size=1;
          button->usage=(cap->UsagePage<<16)|u;
          button->btnid=di;
          button->logmax=1;
        }
      } else if (cap->NotRange.DataIndex<MSHID_BUTTON_LIMIT) {
        struct mshid_button *button=mshid_device_add_button(device);
        if (!button) return -1;
        button->size=1;
        button->usage=(cap->UsagePage<<16)|cap->NotRange.Usage;
        button->btnid=cap->NotRange.DataIndex;
        button->logmax=1;
      }
    }
  }
  
  HIDP_VALUE_CAPS vcaps[256];
  unsigned long vcapc=256;
  HidP_GetValueCaps(HidP_Input,vcaps,&vcapc,(void*)src);
  if ((vcapc>0)&&(vcapc<256)) {
    HIDP_VALUE_CAPS *cap=vcaps;
    int i=vcapc;
    for (;i-->0;cap++) {
      if (cap->IsRange) {
        int di=cap->Range.DataIndexMin;
        int u=cap->Range.UsageMin;
        for (;di<=cap->Range.DataIndexMax;di++,u++) {
          if (di>=MSHID_BUTTON_LIMIT) break;
          struct mshid_button *button=mshid_device_add_button(device);
          if (!button) return -1;
          button->size=cap->BitSize;
          button->usage=(cap->UsagePage<<16)|u;
          button->btnid=di;
          button->logmin=cap->LogicalMin;
          button->logmax=cap->LogicalMax;
        }
      } else if (cap->NotRange.DataIndex<MSHID_BUTTON_LIMIT) {
        struct mshid_button *button=mshid_device_add_button(device);
        if (!button) return -1;
        button->size=cap->BitSize;
        button->usage=(cap->UsagePage<<16)|cap->NotRange.Usage;
        button->btnid=cap->NotRange.DataIndex;
        button->logmin=cap->LogicalMin;
        button->logmax=cap->LogicalMax;
      }
    }
  }

  return 0;
}

/* Acquire descriptors (during setup).
 */

int mshid_device_acquire_report_descriptors(struct mshid_device *device) {
  int a=1024;
  uint8_t *v=malloc(a);
  if (!v) return -1;
  while (1) {
    UINT len=a;
    int c=GetRawInputDeviceInfo(device->handle,RIDI_PREPARSEDDATA,v,&len);
    if (c<0) c=len;
    if (c<0) break;
    if (c>a) {
      if (a>INT_MAX>>1) break;
      a<<=1;
      void *nv=realloc(v,a);
      if (!nv) break;
      v=nv;
      continue;
    }
    mshid_device_apply_preparsed(device,v,c);
    if (device->preparsed) free(device->preparsed);
    device->preparsed=v; // HANDOFF
    device->btnidc=0;
    int i=device->buttonc; while (i-->0) {
      if (device->buttonv[i].btnid>=device->btnidc) {
        device->btnidc=device->buttonv[i].btnid+1;
      }
    }
    return 0;
  }
  free(v);
  return 0;
}

/* Receive report (ongoing).
 */
 
void mshid_device_receive_report(struct hostio_input *driver,struct mshid_device *device,const void *src,int srcc) {
  if (!device->preparsed) return;

  /* Utterly ridiculous! They don't send events for buttons whose value has dropped to zero.
   * So we're expected to review all inputs and compare to previous state, to detect the zeroes?
   * whyyyyyy
   */
  int nv[MSHID_BUTTON_LIMIT]={0};

  HIDP_DATA dv[256];
  long unsigned int dc=256;
  HidP_GetData(HidP_Input,dv,&dc,device->preparsed,(void*)src,srcc);
  if ((dc>0)&&(dc<256)) {
    HIDP_DATA *data=dv;
    int i=dc;
    for (;i-->0;data++) {
      if (data->DataIndex>=device->btnidc) continue;
      nv[data->DataIndex]=data->RawValue;
    }
    int btnid=device->btnidc;
    while (btnid-->0) {
      if (nv[btnid]==device->value_by_btnid[btnid]) continue;
      device->value_by_btnid[btnid]=nv[btnid];
      driver->delegate.cb_button(driver->delegate.userdata,device->devid,btnid,nv[btnid]);
    }
  }
}
