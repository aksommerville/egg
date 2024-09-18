#include "../eggrt_internal.h"

#define INCFG_PRESS_TIMEOUT 10.0
#define INCFG_DEVICE_TIMEOUT 10.0

static void incfg_cb_event(struct hostio_input *driver,int devid,int btnid,int value,void *userdata);

// 1-bit PNG 128x155
// od -tx1 -An src/eggrt/incfg/bits.png | sed 's/ /\,0x/g' | sed -E 's/^\,(.*)/  \1\,/'
static const uint8_t incfg_bits[]={
  0x89,0x50,0x4e,0x47,0x0d,0x0a,0x1a,0x0a,0x00,0x00,0x00,0x0d,0x49,0x48,0x44,0x52,
  0x00,0x00,0x00,0x80,0x00,0x00,0x00,0x9b,0x01,0x03,0x00,0x00,0x00,0x90,0xe1,0x01,
  0xd0,0x00,0x00,0x00,0x06,0x50,0x4c,0x54,0x45,0x00,0x00,0x00,0xff,0xff,0xff,0xa5,
  0xd9,0x9f,0xdd,0x00,0x00,0x02,0x19,0x49,0x44,0x41,0x54,0x48,0xc7,0xed,0xd4,0xbd,
  0x8a,0xdb,0x40,0x10,0x00,0xe0,0x91,0x56,0x89,0xb6,0x8a,0x36,0x7d,0x60,0x17,0x93,
  0x32,0x0f,0xe0,0xc2,0x58,0x8a,0x30,0xe1,0x38,0x8c,0x9f,0x21,0xf5,0x71,0x84,0x94,
  0x29,0x0e,0xfd,0x38,0x57,0x04,0x37,0x79,0x85,0x70,0x5c,0x21,0x84,0xf1,0x33,0x28,
  0x21,0x90,0x14,0xf7,0x10,0xc2,0xb8,0x08,0xc6,0x1c,0x2a,0x55,0x2c,0x56,0x66,0xb5,
  0x17,0x5b,0x56,0x72,0x7f,0x21,0xd7,0x04,0x0f,0xc6,0xbb,0xfa,0xb4,0x9e,0x99,0xd5,
  0x8f,0x01,0x6e,0x0f,0x47,0x02,0xb8,0x45,0x03,0x98,0x09,0xe0,0xd1,0x26,0x18,0x00,
  0x51,0x13,0xfa,0x0a,0x4e,0x1a,0xf0,0x59,0xc1,0xfb,0xed,0x31,0x81,0x18,0xf0,0x93,
  0x6d,0xc0,0xd2,0x20,0x36,0x40,0x35,0xb0,0x6d,0x11,0x52,0x61,0x64,0xdb,0x32,0xec,
  0x99,0xfa,0xee,0x52,0xd0,0x27,0x10,0x5e,0x2b,0x78,0x4c,0x81,0x2b,0x28,0x31,0x47,
  0x04,0x24,0x03,0x83,0x41,0x58,0x49,0xb7,0x5a,0x03,0x3c,0xc2,0x42,0x58,0xe2,0x13,
  0xe0,0x69,0x5c,0x04,0x60,0x92,0xc2,0x12,0x76,0xe4,0x19,0xd2,0x29,0x79,0x19,0x46,
  0x60,0x5a,0xd4,0x12,0xd4,0xf3,0x8c,0xd2,0x2e,0x79,0xe1,0x46,0x60,0x29,0x60,0x42,
  0x10,0x05,0x39,0xcf,0x14,0x90,0x4c,0x83,0x74,0x15,0x50,0x0b,0x7b,0x62,0x82,0xd9,
  0xa5,0x5d,0x55,0x39,0xcf,0x7f,0x07,0xa6,0x80,0x7a,0xd4,0x51,0x50,0xe0,0xb5,0x7c,
  0x5a,0xda,0x39,0xf9,0xf6,0x91,0x3e,0x01,0xe0,0xb2,0xfb,0xb6,0x80,0x0e,0xfe,0x00,
  0xcb,0x9c,0xe2,0x96,0xb0,0x91,0xfc,0x5d,0x0d,0x94,0x89,0x81,0x82,0xc2,0xc9,0xba,
  0x77,0x00,0x5f,0x83,0xdf,0xd3,0xd0,0xab,0x01,0x93,0x8e,0x7b,0x3a,0xe9,0x09,0xbc,
  0xd4,0x65,0xcd,0xbe,0x2e,0x2b,0x21,0xd2,0x8d,0x19,0xa1,0x6e,0x6c,0xad,0x81,0x09,
  0x0d,0x39,0xaf,0x21,0x08,0x34,0xc4,0xf1,0x15,0xc4,0xb1,0x06,0x80,0xeb,0x41,0x27,
  0x05,0xa8,0x93,0x92,0x22,0x08,0xec,0xc8,0x08,0xa5,0x13,0xc7,0x65,0x88,0xa0,0x6f,
  0xa0,0x11,0x56,0x75,0xac,0xc1,0xd3,0x60,0xba,0x1a,0xe4,0x1d,0x40,0x80,0x1a,0xc1,
  0xe2,0x95,0x72,0xbc,0xdb,0x82,0xa8,0x49,0x66,0x71,0xa9,0x46,0xb7,0xc4,0xbd,0xab,
  0x89,0xc0,0xab,0xae,0x46,0x5e,0x00,0xad,0x9f,0x1d,0xc6,0xec,0xfa,0x61,0x76,0x72,
  0xd8,0x4c,0x1e,0x36,0x1c,0xd9,0x86,0x75,0x0b,0x8c,0x36,0x00,0x42,0x7f,0x07,0x02,
  0xdc,0x64,0x0b,0x48,0x1b,0xec,0xea,0xd6,0xca,0xed,0xbd,0x3a,0x51,0x0b,0xec,0x76,
  0x27,0xe6,0xaf,0x37,0xb6,0x15,0x7f,0x00,0x52,0xdc,0x0c,0xf0,0x4f,0x60,0x1f,0xfb,
  0xd8,0xc7,0xfd,0xc2,0xa8,0xff,0x03,0xed,0xfc,0x61,0xe1,0xfa,0xf8,0xeb,0xf7,0x3e,
  0xb8,0x0f,0xfc,0x07,0x31,0xbc,0x98,0x8d,0x5e,0x35,0xe1,0xbc,0x93,0x8c,0xd3,0x1d,
  0x18,0xac,0xbe,0xec,0xae,0x18,0x77,0xce,0xce,0x9a,0x30,0x5a,0xbd,0xf0,0xfd,0x9b,
  0xab,0x1c,0x5f,0x1e,0xce,0x93,0xf9,0x70,0xf8,0x26,0x4d,0x67,0x0a,0xd2,0xc9,0x87,
  0x89,0x9f,0xcc,0xd3,0xf4,0x34,0x4d,0xeb,0x5c,0xab,0xc9,0xc5,0xcc,0xff,0x7e,0x39,
  0x9d,0x0e,0xd2,0xc5,0xe0,0x6a,0x45,0x7a,0x98,0x24,0xc9,0xf2,0xf9,0xf9,0xf2,0x40,
  0xc1,0xf4,0xf8,0xc7,0xd1,0x62,0x96,0x1c,0x1c,0x7d,0x5d,0x2c,0x47,0x78,0xfc,0x13,
  0xd0,0x57,0x1a,0x36,0x3d,0x76,0x97,0xe0,0x00,0x00,0x00,0x00,0x49,0x45,0x4e,0x44,
  0xae,0x42,0x60,0x82,
};

#define INCFG_FLAG_LEFT 1
#define INCFG_FLAG_RIGHT 2
 
const struct incfg_button {
  uint16_t btnid;
  uint8_t col,row;
  uint8_t flag;
} incfg_buttonv[]={
// Order matters. Ask in order of importance. This is not the order of EGG_BTN_* bits (which were designed to be close to Standard Mapping).
  {EGG_BTN_LEFT ,0,2,INCFG_FLAG_LEFT},
  {EGG_BTN_RIGHT,1,2,INCFG_FLAG_LEFT},
  {EGG_BTN_UP   ,3,1,INCFG_FLAG_LEFT},
  {EGG_BTN_DOWN ,2,2,INCFG_FLAG_LEFT},
  {EGG_BTN_SOUTH,2,2,INCFG_FLAG_RIGHT},
  {EGG_BTN_WEST ,1,2,INCFG_FLAG_RIGHT},
  {EGG_BTN_EAST ,0,2,INCFG_FLAG_RIGHT},
  {EGG_BTN_NORTH,3,1,INCFG_FLAG_RIGHT},
  {EGG_BTN_L1   ,1,1,INCFG_FLAG_LEFT},
  {EGG_BTN_R1   ,1,1,INCFG_FLAG_RIGHT},
  {EGG_BTN_L2   ,0,1,INCFG_FLAG_LEFT},
  {EGG_BTN_R2   ,0,1,INCFG_FLAG_RIGHT},
  {EGG_BTN_AUX1 ,3,2,INCFG_FLAG_RIGHT},
  {EGG_BTN_AUX2 ,3,2,INCFG_FLAG_LEFT},
  {EGG_BTN_AUX3 ,2,1,INCFG_FLAG_LEFT|INCFG_FLAG_RIGHT},
};

/* Device record.
 */
 
#define INCFG_BUTTON_MODE_ONEWAY 1 /* ON if v>=srclo. Implies (srclo!=1) */
#define INCFG_BUTTON_MODE_TWOWAY 2 /* -1 if v<=srclo, 1 if v>=srchi, 0 otherwise */
#define INCFG_BUTTON_MODE_HAT    3 /* v-srclo in 0..7 */

// How many two-state buttons can be simultaneously held?
// A typical human has 10 fingers, so 16 should be more than enough.
#define INCFG_HOLD_LIMIT 16
 
struct incfg_device {
  int devid;
  char *name; // Normalized for display.
  int namec;
  char *oname; // Verbatim from driver.
  int onamec;
  int vid,pid,version;
  // Don't record caps for 2-state buttons with zero/nonzero value.
  struct incfg_device_button {
    int btnid;
    int mode;
    int srclo,srchi;
    int value; // Digested. (0..1) for ONEWAY, (-1..1) for TWOWAY, (0..7) for HAT.
  } *buttonv;
  int buttonc,buttona;
  // 2-state buttons are listed here when ON.
  int holdv[INCFG_HOLD_LIMIT];
  int holdc;
  // Parallel to incfg_buttonv:
  struct incfg_change {
    int srcbtnid;
    int srcmode;
    int srcpart;
    int dstbtnid; // If nonzero but (srcbtnid) zero, button was explicitly skipped.
  } changev[15];
};
 
static void incfg_device_cleanup(struct incfg_device *device) {
  if (device->name) free(device->name);
  if (device->oname) free(device->oname);
  if (device->buttonv) free(device->buttonv);
}

/* Delete.
 */
 
void incfg_del(struct incfg *incfg) {
  if (!incfg) return;
  inmgr_override_all(eggrt.inmgr,0,0);
  render_texture_del(eggrt.render,incfg->texid_main);
  render_texture_del(eggrt.render,incfg->texid_bits);
  if (incfg->devicev) {
    while (incfg->devicec-->0) incfg_device_cleanup(incfg->devicev+incfg->devicec);
    free(incfg->devicev);
  }
  free(incfg);
}

/* Init.
 */
 
static int incfg_init(struct incfg *incfg) {
  if ((incfg->texid_main=render_texture_new(eggrt.render))<1) return -1;
  if ((incfg->texid_bits=render_texture_new(eggrt.render))<1) return -1;
  if (render_texture_load(eggrt.render,incfg->texid_main,INCFG_FBW,INCFG_FBH,INCFG_FBW<<2,EGG_TEX_FMT_RGBA,0,0)<0) return -1;
  if (render_texture_load(eggrt.render,incfg->texid_bits,0,0,0,0,incfg_bits,sizeof(incfg_bits))<0) return -1;
  inmgr_override_all(eggrt.inmgr,incfg_cb_event,incfg);
  incfg->buttonp=-1;
  return 0;
}

/* New.
 */

struct incfg *incfg_new() {
  struct incfg *incfg=calloc(1,sizeof(struct incfg));
  if (!incfg) return 0;
  if (incfg_init(incfg)<0) {
    incfg_del(incfg);
    return 0;
  }
  return incfg;
}

/* Set device name.
 */
 
static int incfg_device_set_name(struct incfg_device *device,const char *src,int srcc) {
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  char *nv=0,*onv=0;
  if (srcc) {
    if (!(nv=malloc(srcc+1))) return -1; // Normalization can only make it shorter.
    if (!(onv=malloc(srcc+1))) { free(nv); return -1; }
    memcpy(onv,src,srcc);
    onv[srcc]=srcc;
  }
  if (device->name) free(device->name);
  device->name=nv;
  device->namec=0;
  if (device->oname) free(device->oname);
  device->oname=onv;
  device->onamec=srcc;
  
  // Our font only contains digits and uppercase letters (and space by default).
  // Normalize name accordingly.
  if (device->name) {
    int srcp=0;
    for (;srcp<srcc;srcp++) {
      if ((src[srcp]>='a')&&(src[srcp]<='z')) device->name[device->namec++]=src[srcp]-0x20;
      else if ((src[srcp]>='A')&&(src[srcp]<='Z')) device->name[device->namec++]=src[srcp];
      else if ((src[srcp]>='0')&&(src[srcp]<='9')) device->name[device->namec++]=src[srcp];
      else { // Everything else turns into whitespace, and collapse the whitespace.
        if (device->namec&&(device->name[device->namec-1]!=0x20)) device->name[device->namec++]=0x20;
      }
    }
    while (device->namec&&((unsigned char)device->name[device->namec-1]<=0x20)) device->namec--;
    device->name[device->namec]=0;
  }
  
  return 0;
}

/* Search devices
 */
 
static int incfg_devicev_search(struct incfg *incfg,int devid) {
  int lo=0,hi=incfg->devicec;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
    int q=incfg->devicev[ck].devid;
         if (devid<q) hi=ck;
    else if (devid>q) lo=ck+1;
    else return ck;
  }
  return -lo-1;
}

/* Add or remove held two-state buttons in device.
 */
 
// >0 if added
static int incfg_device_holdv_add(struct incfg_device *device,int btnid) {
  if (btnid<1) return 0;
  int i=device->holdc;
  while (i-->0) if (device->holdv[i]==btnid) return 0;
  if (device->holdc>=INCFG_HOLD_LIMIT) return 0;
  device->holdv[device->holdc++]=btnid;
  return 1;
}

static void incfg_device_holdv_remove(struct incfg_device *device,int btnid) {
  int i=device->holdc;
  while (i-->0) {
    if (device->holdv[i]==btnid) {
      device->holdc--;
      memmove(device->holdv+i,device->holdv+i+1,sizeof(int)*(device->holdc-i));
    }
  }
}

/* Search buttons in device.
 */
 
static int incfg_device_buttonv_search(const struct incfg_device *device,int btnid) {
  int lo=0,hi=device->buttonc;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
    int q=device->buttonv[ck].btnid;
         if (btnid<q) hi=ck;
    else if (btnid>q) lo=ck+1;
    else return ck;
  }
  return -lo-1;
}

/* Add capability to device.
 */
 
static int incfg_device_define_button(struct incfg_device *device,int btnid,int mode,int srclo,int srchi) {
  struct incfg_device_button *button;
  int p=incfg_device_buttonv_search(device,btnid);
  if (p>=0) {
    button=device->buttonv+p;
  } else {
    p=-p-1;
    if (device->buttonc>256) return -1; // Sanity limit.
    if (device->buttonc>=device->buttona) {
      int na=device->buttona+32;
      if (na>INT_MAX/sizeof(struct incfg_device_button)) return -1;
      void *nv=realloc(device->buttonv,sizeof(struct incfg_device_button)*na);
      if (!nv) return -1;
      device->buttonv=nv;
      device->buttona=na;
    }
    button=device->buttonv+p;
    memmove(button+1,button,sizeof(struct incfg_device_button)*(device->buttonc-p));
    device->buttonc++;
    button->btnid=btnid;
    if (mode==INCFG_BUTTON_MODE_HAT) button->value=-1;
    else button->value=0;
  }
  button->mode=mode;
  button->srclo=srclo;
  button->srchi=srchi;
  return 0;
}

/* Receive capability report while welcoming device.
 */
 
struct incfg_cb_cap_ctx {
  struct incfg *incfg;
  struct incfg_device *device;
};
 
static int incfg_cb_cap(int btnid,int hidusage,int lo,int hi,int value,void *userdata) {
  struct incfg_cb_cap_ctx *ctx=userdata;
  int range=hi-lo+1;
  
  // range<=2, it's either invalid or two-state. Either way, no need to record it.
  if (range<=2) return 0;
  
  // range of exactly 8, call it a hat.
  if (range==8) return incfg_device_define_button(ctx->device,btnid,INCFG_BUTTON_MODE_HAT,lo,hi);
  
  // range>=3 straddling zero, call it two-way.
  if ((lo<0)&&(hi>0)) {
    int mid=(lo+hi)>>1;
    int midlo=(mid+lo)>>1;
    int midhi=(mid+hi)>>1;
    if (midlo>=mid) midlo=mid-1;
    if (midhi<=mid) midhi=mid+1;
    return incfg_device_define_button(ctx->device,btnid,INCFG_BUTTON_MODE_TWOWAY,midlo,midhi);
  }
  
  // Range of 3 with lo==0, it's probably a keyboard key (doesn't need recorded).
  if ((range==3)&&!lo) return 0;
  
  // Now it gets dicey: Is this a one-way axis (eg Xbox 360 L2), or a two-way axis unwisely reporting itself unsigned?
  // Let's assume that if the (value) provided here is at (lo), it's one-way.
  // (value) is supposed to be the resting value, in which case we're correct.
  // But drivers might report the most recent value instead. In which case we're still *probably* correct.
  if (value<=lo) return 0;
  return incfg_device_define_button(ctx->device,btnid,INCFG_BUTTON_MODE_ONEWAY,(lo+hi)>>1,INT_MAX);
}

/* Get device record, or create it if necessary.
 */
 
static struct incfg_device *incfg_device_intern(struct incfg *incfg,struct hostio_input *driver,int devid) {
  int p=incfg_devicev_search(incfg,devid);
  if (p>=0) return incfg->devicev+p;
  p=-p-1;
  if (incfg->devicec>=incfg->devicea) {
    int na=incfg->devicea+8;
    if (na>INT_MAX/sizeof(struct incfg_device)) return 0;
    void *nv=realloc(incfg->devicev,sizeof(struct incfg_device)*na);
    if (!nv) return 0;
    incfg->devicev=nv;
    incfg->devicea=na;
  }
  struct incfg_device *device=incfg->devicev+p;
  memmove(device+1,device,sizeof(struct incfg_device)*(incfg->devicec-p));
  incfg->devicec++;
  memset(device,0,sizeof(struct incfg_device));
  device->devid=devid;
  if (driver) {
    if (driver->type->get_ids) {
      const char *name=driver->type->get_ids(&device->vid,&device->pid,&device->version,driver,devid);
      incfg_device_set_name(device,name,-1);
    }
    if (!device->namec) {
      incfg_device_set_name(device,"Unknown Device",-1);
    }
    if (driver->type->for_each_button) {
      struct incfg_cb_cap_ctx ctx={.incfg=incfg,.device=device};
      driver->type->for_each_button(driver,devid,incfg_cb_cap,&ctx);
    }
  } else {
    // When there's no driver, it's the system keyboard. All keyboard buttons can take the default behavior: zero/nonzero.
    incfg_device_set_name(device,"System Keyboard",-1);
  }
  return device;
}

/* Finish collecting events.
 */
 
static void incfg_finish(struct incfg *incfg,int commit) {
  if (commit&&(incfg->devicec==1)) {
    struct incfg_device *device=incfg->devicev;
    struct inmgr_tm *tm=inmgr_tm_new(device->vid,device->pid,device->version,device->oname,device->onamec);
    if (tm) {
      const struct incfg_change *change=device->changev;
      int i=sizeof(device->changev)/sizeof(device->changev[0]);
      for (;i-->0;change++) {
        if (!change->srcbtnid) continue;
        switch (change->srcmode) {
          case INCFG_BUTTON_MODE_ONEWAY: inmgr_tm_synthesize_button(tm,change->srcbtnid,0,1,change->dstbtnid); break;
          // Oops "twoway" vs "threeway"; I've been confused about whether the chili contains cheese:
          case INCFG_BUTTON_MODE_TWOWAY: {
              int dstbtnid=change->dstbtnid;
              switch (dstbtnid) {
                case EGG_BTN_LEFT: dstbtnid|=EGG_BTN_RIGHT; break;
                case EGG_BTN_UP: dstbtnid|=EGG_BTN_DOWN; break;
                case EGG_BTN_RIGHT: continue;
                case EGG_BTN_DOWN: continue;
              }
              inmgr_tm_synthesize_threeway(tm,change->srcbtnid,-1,1,dstbtnid);
            } break;
          case INCFG_BUTTON_MODE_HAT: inmgr_tm_synthesize_hat(tm,change->srcbtnid,0,7); break;
        }
      }
      if (inmgr_install_tm_over(eggrt.inmgr,tm)<0) { // HANDOFF on success
        inmgr_tm_del(tm);
      } else {
        // Force them to sync now, in case main quits when we close.
        inmgr_update(eggrt.inmgr);
      }
    }
  }
  if (incfg==eggrt.incfg) {
    eggrt.incfg=0;
    incfg_del(incfg);
    return;
  }
  incfg->buttonp=-1;
  incfg->devid=0;
  while (incfg->devicec>0) {
    incfg->devicec--;
    incfg_device_cleanup(incfg->devicev+incfg->devicec);
  }
}

/* Consider whether record (buttonp) can be inferred from prior changes.
 * If so, populate (device->changev[buttonp]) and return nonzero.
 * There are two different kinds of inference:
 *  - Dpad from hat or signed axis, we just need the first value and can assume the rest.
 *  - One button of a set was skipped, we assume the rest are absent too. (eg if there's no L1, assume there's no (R1,L2,R2) either).
 * That second point is the main rationale for asking buttons in order of importance.
 */
 
static int incfg_maybe_force_change(struct incfg *incfg,struct incfg_device *device,int buttonp) {
  int buttonc=sizeof(incfg_buttonv)/sizeof(incfg_buttonv[0]);
  if ((buttonp<0)||(buttonp>=buttonc)) return 0;
  int btnid=incfg_buttonv[buttonp].btnid;
  struct incfg_change *dst=device->changev+buttonp;
  #define REVERSE(tag) { \
    struct incfg_change *change=device->changev; \
    int i=buttonp; \
    for (;i-->0;change++) { \
      if (change->dstbtnid!=EGG_BTN_##tag) continue; \
      if (!change->srcbtnid) continue; \
      if (change->srcmode==INCFG_BUTTON_MODE_TWOWAY) { \
        dst->srcbtnid=change->srcbtnid; \
        dst->srcmode=change->srcmode; \
        dst->srcpart=-change->srcpart; \
        dst->dstbtnid=btnid; \
        return 1; \
      } else if (change->srcmode==INCFG_BUTTON_MODE_HAT) { \
        dst->srcbtnid=change->srcbtnid; \
        dst->srcmode=change->srcmode; \
        switch (btnid) { \
          case EGG_BTN_LEFT: dst->srcpart=6; break; \
          case EGG_BTN_RIGHT: dst->srcpart=2; break; \
          case EGG_BTN_UP: dst->srcpart=0; break; \
          case EGG_BTN_DOWN: dst->srcpart=4; break; \
        } \
        dst->dstbtnid=btnid; \
        return 1; \
      } \
    } \
  }
  #define DROPIFNOT(tag) { \
    struct incfg_change *change=device->changev; \
    int i=buttonp; \
    for (;i-->0;change++) { \
      if (change->dstbtnid!=EGG_BTN_##tag) continue; \
      if (change->srcbtnid) continue; \
      dst->srcbtnid=0; \
      dst->srcmode=0; \
      dst->srcpart=0; \
      dst->dstbtnid=btnid; \
      return 1; \
    } \
  }
  switch (incfg_buttonv[buttonp].btnid) {
    case EGG_BTN_RIGHT: REVERSE(LEFT) break;
    case EGG_BTN_DOWN: REVERSE(UP) break;
    case EGG_BTN_WEST: DROPIFNOT(SOUTH) break;
    case EGG_BTN_EAST: DROPIFNOT(WEST) break;
    case EGG_BTN_NORTH: DROPIFNOT(EAST) break;
    case EGG_BTN_R1: DROPIFNOT(L1) break;
    case EGG_BTN_L2: DROPIFNOT(R1) break;
    case EGG_BTN_R2: DROPIFNOT(L2) break;
    case EGG_BTN_AUX2: DROPIFNOT(AUX1) break;
    case EGG_BTN_AUX3: DROPIFNOT(AUX2) break;
  }
  #undef REVERSE
  #undef DROPIFNOT
  return 0;
}

/* Receive significant event from device being mapped.
 * (part) is (1) for two-state or one-way; (-1,1) for two-way; (0,2,4,6) for hat.
 */
 
static void incfg_button_pressed(struct incfg *incfg,int btnid,int mode,int part) {
  if (incfg->buttonp<0) return;
  if (incfg->waitstage!=0) return;
  if (incfg->devicec==1) {
    struct incfg_device *device=incfg->devicev;
    if (incfg->buttonp<sizeof(device->changev)/sizeof(device->changev[0])) {
      struct incfg_change *change=device->changev+incfg->buttonp;
      change->srcbtnid=btnid;
      change->srcmode=mode;
      change->srcpart=part;
      change->dstbtnid=incfg_buttonv[incfg->buttonp].btnid;
    }
  }
  incfg->waitstage=1;
  incfg->waitbtnid=btnid;
  incfg->waitelapsed=0.0;
}
 
static void incfg_button_released(struct incfg *incfg,int btnid) {
  if (incfg->buttonp<0) return;
  if (incfg->waitstage!=1) return;
  if (btnid!=incfg->waitbtnid) return;
  if (incfg->devicec!=1) return;
  struct incfg_device *device=incfg->devicev;
  for (;;) {
    incfg->buttonp++;
    if (incfg->buttonp>=sizeof(incfg_buttonv)/sizeof(incfg_buttonv[0])) {
      incfg_finish(incfg,1);
    } else {
      if (incfg_maybe_force_change(incfg,device,incfg->buttonp)) continue;
      incfg->waitstage=0;
      incfg->waitbtnid=0;
      incfg->waitelapsed=0.0;
    }
    return;
  }
}

static void incfg_skip_button(struct incfg *incfg) {
  if (incfg->buttonp<0) return;
  if (incfg->waitstage!=0) return;
  if (incfg->devicec==1) {
    struct incfg_device *device=incfg->devicev;
    if (incfg->buttonp<sizeof(device->changev)/sizeof(device->changev[0])) {
      struct incfg_change *change=device->changev+incfg->buttonp;
      change->srcbtnid=0;
      change->srcmode=0;
      change->srcpart=0;
      change->dstbtnid=incfg_buttonv[incfg->buttonp].btnid;
    }
  }
  // Cheat a little bit and borrow incfg_button_released() to advance state:
  incfg->waitstage=1;
  incfg->waitbtnid=123;
  incfg_button_released(incfg,123);
}

/* Receive event at the first stage, when multiple devices may be in play.
 * Return >0 if the event is significant, ie we should become active.
 */
 
static int incfg_event_passive(struct incfg_device *device,int btnid,int value) {
  int p=incfg_device_buttonv_search(device,btnid);
  if (p>=0) {
    struct incfg_device_button *button=device->buttonv+p;
    switch (button->mode) {
      case INCFG_BUTTON_MODE_ONEWAY: {
          int adjusted=((value>=button->srclo)&&(value<=button->srchi))?1:0;
          if (adjusted==button->value) return 0;
          button->value=adjusted;
          if (adjusted) return 1;
          return 0;
        }
      case INCFG_BUTTON_MODE_TWOWAY: {
          int adjusted=(value<=button->srclo)?-1:(value>=button->srchi)?1:0;
          if (adjusted==button->value) return 0;
          button->value=adjusted;
          if (adjusted) return 1;
          return 0;
        }
      case INCFG_BUTTON_MODE_HAT: {
          int adjusted=value-button->srclo;
          if ((adjusted<0)||(adjusted>7)) adjusted=-1;
          if (adjusted==button->value) return 0;
          int px=0,py=0,nx=0,ny=0;
          switch (button->value) {
            case 7: case 6: case 5: px=-1; break;
            case 1: case 2: case 3: px=1; break;
          }
          switch (button->value) {
            case 7: case 0: case 1: py=-1; break;
            case 5: case 4: case 3: py=1; break;
          }
          button->value=adjusted;
          switch (button->value) {
            case 7: case 6: case 5: nx=-1; break;
            case 1: case 2: case 3: nx=1; break;
          }
          switch (button->value) {
            case 7: case 0: case 1: ny=-1; break;
            case 5: case 4: case 3: ny=1; break;
          }
          if (nx&&!px) return 1;
          if (ny&&!py) return 1;
          return 0;
        }
    }
  } else {
    if (value) {
      if (incfg_device_holdv_add(device,btnid)>0) return 1;
    } else {
      incfg_device_holdv_remove(device,btnid);
    }
  }
  return 0;
}

/* Device is active, receive an event on it.
 */
 
static void incfg_event_active(struct incfg *incfg,int btnid,int value) {
  //fprintf(stderr,"%s (%d).%#.08x=%d\n",__func__,incfg->devid,btnid,value);
  if (incfg->devicec!=1) return;
  struct incfg_device *device=incfg->devicev;
  int p=incfg_device_buttonv_search(device,btnid);
  if (p>=0) {
    struct incfg_device_button *button=device->buttonv+p;
    switch (button->mode) {
      case INCFG_BUTTON_MODE_ONEWAY: {
          int adjusted=((value>=button->srclo)&&(value<=button->srchi))?1:0;
          if (adjusted==button->value) return;
          button->value=adjusted;
          if (adjusted) incfg_button_pressed(incfg,btnid,INCFG_BUTTON_MODE_ONEWAY,1);
          else incfg_button_released(incfg,btnid);
        } break;
      case INCFG_BUTTON_MODE_TWOWAY: {
          int adjusted=(value<=button->srclo)?-1:(value>=button->srchi)?1:0;
          if (adjusted==button->value) return;
          button->value=adjusted;
          if (adjusted) incfg_button_pressed(incfg,btnid,INCFG_BUTTON_MODE_TWOWAY,adjusted);
          else incfg_button_released(incfg,btnid);
        } break;
      case INCFG_BUTTON_MODE_HAT: {
          int adjusted=value-button->srclo;
          if ((adjusted<0)||(adjusted>7)) adjusted=-1;
          if (adjusted==button->value) return;
          int px=0,py=0,nx=0,ny=0;
          switch (button->value) {
            case 7: case 6: case 5: px=-1; break;
            case 1: case 2: case 3: px=1; break;
          }
          switch (button->value) {
            case 7: case 0: case 1: py=-1; break;
            case 5: case 4: case 3: py=1; break;
          }
          button->value=adjusted;
          switch (button->value) {
            case 7: case 6: case 5: nx=-1; break;
            case 1: case 2: case 3: nx=1; break;
          }
          switch (button->value) {
            case 7: case 0: case 1: ny=-1; break;
            case 5: case 4: case 3: ny=1; break;
          }
          // Diagonals are ambiguous. We map only if there's a newly nonzero axis, and the other axis is zero.
          if (
            (nx&&!px&&!ny)||
            (ny&&!py&&!nx)
          ) {
            incfg_button_pressed(incfg,btnid,INCFG_BUTTON_MODE_HAT,adjusted);
          } else if (!nx&&!ny) {
            incfg_button_released(incfg,btnid);
          }
        } break;
    }
  } else {
    if (value) {
      if (incfg_device_holdv_add(device,btnid)>0) {
        incfg_button_pressed(incfg,btnid,INCFG_BUTTON_MODE_ONEWAY,1);
      }
    } else {
      incfg_device_holdv_remove(device,btnid);
      incfg_button_released(incfg,btnid);
    }
  }
}

/* Make device active and remove other devices.
 */
 
static int incfg_focus_device(struct incfg *incfg,struct incfg_device *device) {
  int devid=device->devid;
  struct incfg_device *other=incfg->devicev;
  int i=0,p=-1;
  for (;i<incfg->devicec;i++,other++) {
    if (other==device) { p=i; continue; }
    incfg_device_cleanup(other);
  }
  if (p<0) {
    incfg->devicec=0;
    return -1;
  }
  if (p) memcpy(incfg->devicev,device,sizeof(struct incfg_device));
  incfg->devicec=1;
  incfg->devid=devid;
  incfg->buttonp=0;
  incfg->waitstage=0;
  incfg->waitbtnid=0;
  incfg->waitelapsed=0.0;
  memset(device->changev,0,sizeof(device->changev));
  return 0;
}

/* Event from drivers.
 */
 
static void incfg_cb_event(struct hostio_input *driver,int devid,int btnid,int value,void *userdata) {
  struct incfg *incfg=userdata;
  if (incfg->devid) {
    if (devid!=incfg->devid) return;
    incfg_event_active(incfg,btnid,value);
  } else {
    struct incfg_device *device=incfg_device_intern(incfg,driver,devid);
    if (!device) return;
    if (incfg_event_passive(device,btnid,value)) {
      incfg_focus_device(incfg,device);
    }
  }
}

/* Update.
 */

int incfg_update(struct incfg *incfg,double elapsed) {
  incfg->waitelapsed+=elapsed;
  incfg->time_warning=0;
  #define MAYBE_WARN(endtime,expcode) { \
    double remaining=endtime-incfg->waitelapsed; \
    if (remaining<=0.0) { \
      expcode; \
    } else { \
      incfg->time_warning=(int)remaining+1; \
      if (incfg->time_warning<1) incfg->time_warning=1; \
      else if (incfg->time_warning>5) incfg->time_warning=0; \
    } \
  }
  if (incfg->buttonp>=0) {
    if (incfg->waitstage==0) {
      MAYBE_WARN(INCFG_PRESS_TIMEOUT,incfg_skip_button(incfg))
    }
  } else {
    MAYBE_WARN(INCFG_DEVICE_TIMEOUT,incfg_finish(incfg,0))
  }
  #undef MAYBE_WARN
  return 0;
}

/* Render.
 */

int incfg_render(struct incfg *incfg) {
  {
    struct egg_draw_rect vtx={0,0,INCFG_FBW,INCFG_FBH,0x80,0x80,0x90,0xff};
    render_draw_rect(eggrt.render,incfg->texid_main,&vtx,1);
  }
  const int gamepadw=64;
  const int gamepadh=48;
  const int gamepadwhalf=gamepadw>>1;
  const int chw=4;
  const int chh=5;
  const int chsrcy=144; // First row: 0..9. Second row: A..Z. One pixel padding between rows.
  int dstx=(INCFG_FBW>>1)-(gamepadw>>1);
  int dsty=(INCFG_FBH>>1)-(gamepadh>>1);
  
  // Device name if focussed.
  if (incfg->devid&&(incfg->devicec==1)) {
    struct egg_draw_decal vtxv[64];
    const int vtxa=sizeof(vtxv)/sizeof(vtxv[0]);
    int vtxc=0;
    int x=(INCFG_FBW>>1)-((incfg->devicev->namec*chw)>>1);
    int y=dsty-8-2;
    const char *src=incfg->devicev->name;
    int i=incfg->devicev->namec;
    for (;i-->0;src++,x+=chw) {
      if (vtxc>=vtxa) break;
      vtxv[vtxc].dstx=x;
      vtxv[vtxc].dsty=y;
      if ((*src>='0')&&(*src<='9')) {
        vtxv[vtxc].srcx=((*src)-'0')*chw;
        vtxv[vtxc].srcy=chsrcy;
      } else if ((*src>='A')&&(*src<='Z')) {
        vtxv[vtxc].srcx=((*src)-'A')*chw;
        vtxv[vtxc].srcy=chsrcy+chh+1;
      } else continue;
      vtxv[vtxc].w=chw;
      vtxv[vtxc].h=chh;
      vtxv[vtxc].xform=0;
      vtxc++;
    }
    render_tint(eggrt.render,0xffffffff);
    render_draw_decal(eggrt.render,incfg->texid_main,incfg->texid_bits,vtxv,vtxc);
  }
  
  #define DRAW_FULL_GAMEPAD(column,rgb) { \
    render_tint(eggrt.render,(rgb<<8)|0xff); \
    struct egg_draw_decal vtx={dstx,dsty,column*gamepadw,0,gamepadw,gamepadh,0,0}; \
    render_draw_decal(eggrt.render,incfg->texid_main,incfg->texid_bits,&vtx,1); \
  }
  #define DRAW_BUTTON(b,rgb) { \
    render_tint(eggrt.render,(rgb<<8)|0xff); \
    if ((b)->flag&INCFG_FLAG_LEFT) { \
      struct egg_draw_decal vtx={dstx,dsty,(b)->col*gamepadwhalf,(b)->row*gamepadh,gamepadwhalf,gamepadh,0,0}; \
      render_draw_decal(eggrt.render,incfg->texid_main,incfg->texid_bits,&vtx,1); \
    } \
    if ((b)->flag&INCFG_FLAG_RIGHT) { \
      struct egg_draw_decal vtx={dstx+gamepadwhalf,dsty,(b)->col*gamepadwhalf,(b)->row*gamepadh,gamepadwhalf,gamepadh,EGG_XFORM_XREV,0}; \
      render_draw_decal(eggrt.render,incfg->texid_main,incfg->texid_bits,&vtx,1); \
    } \
  }
  if (incfg->devid) {
    DRAW_FULL_GAMEPAD(1,0x404050)
  }
  
  // If we have a focus device, indicate the buttons already visited or skipped.
  if (incfg->devid&&(incfg->devicec==1)) {
    struct incfg_device *device=incfg->devicev;
    const struct incfg_change *change=device->changev;
    const struct incfg_button *button=incfg_buttonv;
    int i=incfg->buttonp;
    for (;i-->0;change++,button++) {
      int rgb=0;
      if (change->srcbtnid) rgb=0x306030;
      else rgb=0x603030;
      DRAW_BUTTON(button,rgb)
    }
  }
  
  // Highlight current button if there is one.
  if (incfg->buttonp>=0) {
    const struct incfg_button *button=incfg_buttonv+incfg->buttonp;
    int rgb;
    if (incfg->waitstage) rgb=0x008000;
    else if (((int)(incfg->waitelapsed*2.0))&1) rgb=0xffff00;
    else rgb=0xff8000;
    DRAW_BUTTON(button,rgb)
  }
  
  DRAW_FULL_GAMEPAD(0,incfg->devid?0x101010:0x606070)
  #undef DRAW_FULL_GAMEPAD
  #undef DRAW_BUTTON
  
  // Time warning if applicable.
  if (incfg->time_warning) {
    render_tint(eggrt.render,0x400000ff);
    struct egg_draw_decal vtx={
      .dstx=(INCFG_FBW>>1)-(chw>>1),
      .dsty=dsty+gamepadh+2,
      .srcx=incfg->time_warning*chw,
      .srcy=chsrcy,
      .w=chw,
      .h=chh,
      .xform=0,
    };
    render_draw_decal(eggrt.render,incfg->texid_main,incfg->texid_bits,&vtx,1);
  }
  
  render_tint(eggrt.render,0);
  render_draw_to_main(eggrt.render,eggrt.hostio->video->w,eggrt.hostio->video->h,incfg->texid_main);
  return 0;
}
