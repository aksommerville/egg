#include "inmgr_internal.h"

/* Delete.
 */
 
void inmgr_tm_del(struct inmgr_tm *tm) {
  if (!tm) return;
  if (tm->name) free(tm->name);
  if (tm->buttonv) free(tm->buttonv);
  free(tm);
}

/* New.
 */
 
struct inmgr_tm *inmgr_tm_new(int vid,int pid,int version,const char *name,int namec) {
  struct inmgr_tm *tm=calloc(1,sizeof(struct inmgr_tm));
  if (!tm) return 0;
  tm->vid=vid;
  tm->pid=pid;
  tm->version=version;
  if (!name) namec=0; else if (namec<0) { namec=0; while (name[namec]) namec++; }
  if (namec) {
    if (!(tm->name=malloc(namec+1))) {
      inmgr_tm_del(tm);
      return 0;
    }
    memcpy(tm->name,name,namec);
    tm->name[namec]=0;
    tm->namec=namec;
  }
  return tm;
}

/* Generic hard-coded rules for the system keyboard.
 * Signals:
 *   ESC => QUIT
 *   PAUSE => PAUSE
 *   F1 => CONFIGIN
 *   F7 => SCREENCAP
 *   F8 => LOADSTATE
 *   F9 => SAVESTATE
 *   F10 => STEP
 *   F11 => FULLSCREEN
 *   F12 => PAUSE
 * Uncommon buttons, for either WASD or Arrows orientation:
 *   Tab => L1
 *   Grave => L2
 *   Backslash => R1
 *   Backspace => R2
 *   Enter => AUX1
 *   Close Bracket => AUX2
 *   Open Bracket => AUX3
 * WASD orientation:
 *   WASD => Dpad
 *   Space,Comma,Dot,Slash => Thumbs
 *   QER => WEST,EAST,NORTH, alternate thumbs.
 * Arrows orientation:
 *   Arrows => Dpad
 *   ZXCV => SOUTH,WEST,EAST,NORTH
 * Keypad:
 *   84562 => Dpad (5 and 2 are both Down)
 *   79 => L1,R1
 *   13 => L2,R2
 *   0 => SOUTH
 *   Enter => WEST
 *   Plus => EAST
 *   Dot => NORTH (I know, it's like the southernmost key, but I've chosen them by reachability)
 *   Slash,Star,Dash => AUX1,AUX2,AUX3
 */
 
static const struct inmgr_tm_button inmgr_keyboard_buttonv[]={
  {0x00070004,EGG_BTN_LEFT}, // a
  {0x00070006,EGG_BTN_EAST}, // c
  {0x00070007,EGG_BTN_RIGHT}, // d
  {0x00070008,EGG_BTN_EAST}, // e
  {0x00070014,EGG_BTN_WEST}, // q
  {0x00070015,EGG_BTN_NORTH}, // r
  {0x00070016,EGG_BTN_DOWN}, // s
  {0x00070019,EGG_BTN_NORTH}, // v
  {0x0007001a,EGG_BTN_UP}, // w
  {0x0007001b,EGG_BTN_WEST}, // x
  {0x0007001d,EGG_BTN_SOUTH}, // z
  {0x00070028,EGG_BTN_AUX1}, // enter
  {0x00070029,EGG_SIGNAL_QUIT}, // escape
  {0x0007002a,EGG_BTN_R2}, // backspace
  {0x0007002b,EGG_BTN_L1}, // tab
  {0x0007002c,EGG_BTN_SOUTH}, // space
  {0x0007002f,EGG_BTN_AUX3}, // open bracket
  {0x00070030,EGG_BTN_AUX2}, // close bracket
  {0x00070031,EGG_BTN_R1}, // backslash
  {0x00070035,EGG_BTN_L2}, // grave
  {0x00070036,EGG_BTN_WEST}, // comma
  {0x00070037,EGG_BTN_EAST}, // dot
  {0x00070038,EGG_BTN_NORTH}, // slash
  {0x0007003a,EGG_SIGNAL_CONFIGIN}, // f1
  {0x00070040,EGG_SIGNAL_SCREENCAP}, // f7
  {0x00070041,EGG_SIGNAL_LOADSTATE}, // f8
  {0x00070042,EGG_SIGNAL_SAVESTATE}, // f9
  {0x00070043,EGG_SIGNAL_STEP}, // f10
  {0x00070044,EGG_SIGNAL_FULLSCREEN}, // f11
  {0x00070045,EGG_SIGNAL_PAUSE}, // f12
  {0x00070048,EGG_SIGNAL_PAUSE}, // pause
  {0x0007004f,EGG_BTN_RIGHT}, // right
  {0x00070050,EGG_BTN_LEFT}, // left
  {0x00070051,EGG_BTN_DOWN}, // down
  {0x00070052,EGG_BTN_UP}, // up
  {0x00070054,EGG_BTN_AUX1}, // kp slash
  {0x00070055,EGG_BTN_AUX2}, // kp star
  {0x00070056,EGG_BTN_AUX3}, // kp dash
  {0x00070057,EGG_BTN_EAST}, // kp plus
  {0x00070058,EGG_BTN_WEST}, // kp enter
  {0x00070059,EGG_BTN_L2}, // kp 1
  {0x0007005a,EGG_BTN_DOWN}, // kp 2
  {0x0007005b,EGG_BTN_R2}, // kp 3
  {0x0007005c,EGG_BTN_LEFT}, // kp 4
  {0x0007005d,EGG_BTN_DOWN}, // kp 5
  {0x0007005e,EGG_BTN_RIGHT}, // kp 6
  {0x0007005f,EGG_BTN_L1}, // kp 7
  {0x00070060,EGG_BTN_UP}, // kp 8
  {0x00070061,EGG_BTN_R1}, // kp 9
  {0x00070062,EGG_BTN_SOUTH}, // kp 0
  {0x00070063,EGG_BTN_NORTH}, // kp dotw
};
 
static int inmgr_tm_synthesize_keyboard(struct inmgr_tm *tm) {
  
  // Validate the static list every time it runs. I guess this would make more sense as a unit test, but whatever.
  int buttonc=sizeof(inmgr_keyboard_buttonv)/sizeof(inmgr_keyboard_buttonv[0]);
  int i=1; for (;i<buttonc;i++) {
    if (inmgr_keyboard_buttonv[i].srcbtnid<=inmgr_keyboard_buttonv[i-1].srcbtnid) {
      fprintf(stderr,"%s:%d: INTERNAL ERROR: inmgr_keyboard_buttonv out of order around [%d] (%#.8x)\n",__FILE__,__LINE__,i,inmgr_keyboard_buttonv[i].srcbtnid);
      return -2;
    }
  }
  
  if (!(tm->buttonv=malloc(sizeof(inmgr_keyboard_buttonv)))) return -1;
  memcpy(tm->buttonv,inmgr_keyboard_buttonv,sizeof(inmgr_keyboard_buttonv));
  tm->buttonc=buttonc;
  tm->buttona=buttonc;
  
  return 0;
}

/* Return one of the provided bits, whichever has the fewest maps assigned already.
 * Ties break low.
 */
 
// We use 3 aggregates in addition to HORZ,VERT,DPAD, which are only valid during template synthesis.
#define EGG_BTN_THUMBS (EGG_BTN_SOUTH|EGG_BTN_WEST|EGG_BTN_EAST|EGG_BTN_NORTH)
#define EGG_BTN_TRIGGERS (EGG_BTN_L1|EGG_BTN_R2|EGG_BTN_L2|EGG_BTN_R2)
#define EGG_BTN_AUXES (EGG_BTN_AUX1|EGG_BTN_AUX2|EGG_BTN_AUX3)
 
static int inmgr_tm_least_popular(const struct inmgr_tm *tm,int options) {
  int count_by_index[16]={0};
  const struct inmgr_tm_button *button=tm->buttonv;
  int i=tm->buttonc;
  for (;i-->0;button++) {
    if (button->dstbtnid>0xffff) continue;
    int bit=1,p=0; for (;bit<0x10000;bit<<=1,p++) {
      if (button->dstbtnid&bit) count_by_index[p]++;
    }
  }
  int bit=1,btnid=0,bestscore=INT_MAX;
  for (i=0;i<16;bit<<=1,i++) {
    if (!(options&bit)) continue;
    if (count_by_index[i]<bestscore) {
      btnid=bit;
      bestscore=count_by_index[i];
    }
  }
  return btnid;
}

/* Get a template button.
 */
 
static const struct inmgr_tm_button *inmgr_tm_buttonv_get(const struct inmgr_tm *tm,int srcbtnid) {
  int lo=0,hi=tm->buttonc;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
    const struct inmgr_tm_button *q=tm->buttonv+ck;
         if (srcbtnid<q->srcbtnid) hi=ck;
    else if (srcbtnid>q->srcbtnid) lo=ck+1;
    else return q;
  }
  return 0;
}

/* Insert a template button.
 */
 
static struct inmgr_tm_button *inmgr_tm_buttonv_insert(struct inmgr_tm *tm,int srcbtnid,int dstbtnid) {
  int lo=0,hi=tm->buttonc;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
    const struct inmgr_tm_button *q=tm->buttonv+ck;
         if (srcbtnid<q->srcbtnid) hi=ck;
    else if (srcbtnid>q->srcbtnid) lo=ck+1;
    else return 0;
  }
  if (tm->buttonc>=tm->buttona) {
    int na=tm->buttona+32;
    if (na>INT_MAX/sizeof(struct inmgr_tm_button)) return 0;
    void *nv=realloc(tm->buttonv,sizeof(struct inmgr_tm_button)*na);
    if (!nv) return 0;
    tm->buttonv=nv;
    tm->buttona=na;
  }
  struct inmgr_tm_button *button=tm->buttonv+lo;
  memmove(button+1,button,sizeof(struct inmgr_tm_button)*(tm->buttonc-lo));
  tm->buttonc++;
  button->srcbtnid=srcbtnid;
  button->dstbtnid=dstbtnid;
  return button;
}

/* Add buttons, having decided what class they fall into.
 */
 
int inmgr_tm_synthesize_button(struct inmgr_tm *tm,int srcbtnid,int lo,int hi,int dstbtnid) {
  if (lo>=hi) return 0;
  if (!dstbtnid) dstbtnid=inmgr_tm_least_popular(tm,EGG_BTN_THUMBS|EGG_BTN_TRIGGERS|EGG_BTN_AUXES);
  else dstbtnid=inmgr_tm_least_popular(tm,dstbtnid);
  if (!inmgr_tm_buttonv_insert(tm,srcbtnid,dstbtnid)) return -1;
  return 0;
}

int inmgr_tm_synthesize_threeway(struct inmgr_tm *tm,int srcbtnid,int lo,int hi,int dstbtnid) {
  if (lo>=hi-1) return 0; // Need at least three values.
  if (!dstbtnid) {
    dstbtnid=inmgr_tm_least_popular(tm,EGG_BTN_DPAD);
    switch (dstbtnid) {
      case EGG_BTN_LEFT: dstbtnid|=EGG_BTN_RIGHT; break;
      case EGG_BTN_RIGHT: dstbtnid|=EGG_BTN_LEFT; break;
      case EGG_BTN_UP: dstbtnid|=EGG_BTN_DOWN; break;
      case EGG_BTN_DOWN: dstbtnid|=EGG_BTN_UP; break;
    }
  }
  if (!inmgr_tm_buttonv_insert(tm,srcbtnid,dstbtnid)) return -1;
  return 0;
}

int inmgr_tm_synthesize_hat(struct inmgr_tm *tm,int srcbtnid,int lo,int hi) {
  if (lo!=hi-7) return 0; // Range must be exactly 8.
  if (!inmgr_tm_buttonv_insert(tm,srcbtnid,EGG_BTN_DPAD)) return -1;
  return 0;
}

/* Synthesize template from hostio input driver.
 */
 
static int inmgr_tm_synthesize_generic_cb(int btnid,int hidusage,int lo,int hi,int value,void *userdata) {
  struct inmgr_tm *tm=userdata;
  
  /* Range of 1 or less, we definitely can't use it.
   */
  if (lo>=hi) return 0;
  
  /* A few known HID Usage codes can be super helpful.
   */
  switch (hidusage) {
    case 0x00050037: return inmgr_tm_synthesize_button(tm,btnid,lo,hi,EGG_BTN_THUMBS); // Gamepad Fire/Jump, ie thumb button
    case 0x00050039: return inmgr_tm_synthesize_button(tm,btnid,lo,hi,EGG_BTN_TRIGGERS); // Gamepad Trigger
    case 0x0001003d: return inmgr_tm_synthesize_button(tm,btnid,lo,hi,EGG_BTN_AUX1); // Desktop Start
    case 0x0001003e: return inmgr_tm_synthesize_button(tm,btnid,lo,hi,EGG_BTN_AUX2); // Desktop Select
    case 0x00010030: return inmgr_tm_synthesize_threeway(tm,btnid,lo,hi,EGG_BTN_HORZ); // Desktop X
    case 0x00010031: return inmgr_tm_synthesize_threeway(tm,btnid,lo,hi,EGG_BTN_VERT); // Desktop Y
    case 0x00010032: return inmgr_tm_synthesize_button(tm,btnid,lo,hi,EGG_BTN_L2); // Desktop Z
    case 0x00010035: return inmgr_tm_synthesize_button(tm,btnid,lo,hi,EGG_BTN_R2); // Desktop Rz
    case 0x00010039: return inmgr_tm_synthesize_hat(tm,btnid,lo,hi); // Desktop Hat
    case 0x00010085: return inmgr_tm_synthesize_button(tm,btnid,lo,hi,EGG_BTN_AUX3); // System Main Menu
  }
  
  /* If we get usage codes in page 7, search the hard-coded keyboard maps and ignore if not found.
   */
  if ((hidusage>=0x00070000)&&(hidusage<=0x0007ffff)) {
    int lo=0,hi=sizeof(inmgr_keyboard_buttonv)/sizeof(inmgr_keyboard_buttonv[0]);
    while (lo<hi) {
      int ck=(lo+hi)>>1;
      const struct inmgr_tm_button *q=inmgr_keyboard_buttonv+ck;
           if (btnid<q->srcbtnid) hi=ck;
      else if (btnid>q->srcbtnid) lo=ck+1;
      else return inmgr_tm_synthesize_button(tm,btnid,lo,hi,q->dstbtnid);
    }
    return 0;
  }
  
  /* Range of at least 3 straddling zero, assume it's a three-way axis.
   */
  if ((lo<0)&&(hi>0)) return inmgr_tm_synthesize_threeway(tm,btnid,lo,hi,0);
  
  /* Range of 8, it must be a hat.
   */
  int range=hi-lo+1;
  if (range==8) return inmgr_tm_synthesize_hat(tm,btnid,lo,hi);
  
  /* Range of 2 or 3, assume it's a two-state button.
   * 3 because there might be a '2' value for autorepeat.
   */
  if (range<=3) return inmgr_tm_synthesize_button(tm,btnid,lo,hi,0);
  
  /* We're left with ranges greater than 3, with a low limit of zero or more, and not a hat.
   * I've seen both three-way and one-way axes report this way.
   * And also what ought to be two-states: The original Xbox thumb buttons reported 0..255.
   * Since I think it's bloody stupid to report logically signed fields as unsigned, we'll assume it's one-way (like an Xbox 360 L2 or R2 button).
   */
  return inmgr_tm_synthesize_button(tm,btnid,lo,hi,0);
}
 
static int inmgr_tm_synthesize_generic(struct inmgr_tm *tm,struct hostio_input *driver,int devid) {
  if (!driver||!driver->type->for_each_button) return 0;
  return driver->type->for_each_button(driver,devid,inmgr_tm_synthesize_generic_cb,tm);
}

/* Grow tm list in context.
 */
 
static int inmgr_tmv_require(struct inmgr *inmgr) {
  if (inmgr->tmc<inmgr->tma) return 0;
  int na=inmgr->tma+16;
  if (na>INT_MAX/sizeof(void*)) return -1;
  void *nv=realloc(inmgr->tmv,sizeof(void*)*na);
  if (!nv) return -1;
  inmgr->tmv=nv;
  inmgr->tma=na;
  return 0;
}

/* Synthesize.
 * Returns WEAK reference to a new template we install.
 */
 
struct inmgr_tm *inmgr_tmv_synthesize(
  struct inmgr *inmgr,
  struct hostio_input *driver,int devid,
  int vid,int pid,int version,const char *name,int namec
) {

  /* Create and install at front of list.
   * Order matters, and it's entirely possible there's a catch-all at the end.
   * Synthesizing should only happen when we have a specific device to match exactly, so the front is appropriate.
   */
  if (inmgr_tmv_require(inmgr)<0) return 0;
  struct inmgr_tm *tm=inmgr_tm_new(vid,pid,version,name,namec);
  if (!tm) return 0;
  if (!driver&&!devid) { // With null inputs, they're just adding a blank one at the end of the list.
    inmgr->tmv[inmgr->tmc++]=tm;
    return tm;
  }
  memmove(inmgr->tmv+1,inmgr->tmv,sizeof(void*)*inmgr->tmc);
  inmgr->tmc++;
  inmgr->tmv[0]=tm;
  
  /* If they supplied a driver, synthesize generically the hard way.
   * Otherwise, assume it's the system keyboard and apply the default hard-coded keyboard rules.
   */
  int err=-1;
  if (driver) err=inmgr_tm_synthesize_generic(tm,driver,devid);
  else err=inmgr_tm_synthesize_keyboard(tm);
  if (err<0) {
    if (err!=-2) fprintf(stderr,"%s: Error synthesizing input map for device %04x:%04x:%04x '%.*s'\n",eggrt.exename,vid,pid,version,namec,name);
    inmgr->tmc--;
    memmove(inmgr->tmv,inmgr->tmv+1,sizeof(void*)*inmgr->tmc);
    inmgr_tm_del(tm);
    return 0;
  }
  
  return tm;
}

/* Replace regular button mappings in (dst) with those in (src).
 */
 
static int inmgr_tm_bodysnatch(struct inmgr_tm *dst,const struct inmgr_tm *src) {
  /* Both lists are sorted by (srcbtnid).
   * Walk them in tandem and modify (dst) as needed.
   * Don't retain a pointer into (dst->buttonv) since we might reallocate it along the way.
   */
  const struct inmgr_tm_button *srcbtn=src->buttonv;
  int dstp=0,srcp=0;
  for (;;) {
    
    #define ADD { \
      if (dst->buttonc>=dst->buttona) { \
        int na=dst->buttona+16; \
        if (na>INT_MAX/sizeof(struct inmgr_tm_button)) return -1; \
        void *nv=realloc(dst->buttonv,sizeof(struct inmgr_tm_button)*na); \
        if (!nv) return -1; \
        dst->buttonv=nv; \
        dst->buttona=na; \
      } \
      struct inmgr_tm_button *dstbtn=dst->buttonv+dstp; \
      memmove(dstbtn+1,dstbtn,sizeof(struct inmgr_tm_button)*(dst->buttonc-dstp)); \
      memcpy(dstbtn,srcbtn,sizeof(struct inmgr_tm_button)); \
      dst->buttonc++; \
      dstp++; \
      srcp++; \
      srcbtn++; \
      continue; \
    }
    
    #define REMOVE { \
      struct inmgr_tm_button *dstbtn=dst->buttonv+dstp; \
      dst->buttonc--; \
      memmove(dstbtn,dstbtn+1,sizeof(struct inmgr_tm_button)*(dst->buttonc-dstp)); \
      continue; \
    }
    
    #define SKIPDST { \
      dstp++; \
      continue; \
    }
    
    #define SKIPSRC { \
      srcp++; \
      srcbtn++; \
      continue; \
    }
    
    if ((dstp>=dst->buttonc)&&(srcp>=src->buttonc)) break;
    if (dstp>=dst->buttonc) {
      if (srcbtn->dstbtnid&EGG_SIGNAL_BIT) SKIPSRC // Shouldn't happen but let's be consistent: We're not copying SIGNAL buttons.
      ADD
    }
    if (dst->buttonv[dstp].dstbtnid&EGG_SIGNAL_BIT) SKIPDST
    if (srcp>=src->buttonc) REMOVE
    if (srcbtn->dstbtnid&EGG_SIGNAL_BIT) SKIPSRC // Shouldn't happen but let's be consistent: We're not copying SIGNAL buttons.
    if (dst->buttonv[dstp].srcbtnid<srcbtn->srcbtnid) REMOVE
    if (dst->buttonv[dstp].srcbtnid>srcbtn->srcbtnid) ADD
    // Finally, (srcbtnid) matches and is not a signal. Copy it and advance both.
    memcpy(dst->buttonv+dstp,srcbtn,sizeof(struct inmgr_tm_button));
    dstp++;
    srcp++;
    srcbtn++;
    #undef ADD
    #undef REMOVE
    #undef SKIPDST
    #undef SKIPSRC
  }
  return 0;
}

/* Install external tm, handoff.
 */
 
int inmgr_install_tm_over(struct inmgr *inmgr,struct inmgr_tm *tm) {
  if (!inmgr||!tm) return -1;
  
  /* If an existing template has exactly the same match criteria, keep it.
   */
  int i=0;
  for (;i<inmgr->tmc;i++) {
    struct inmgr_tm *other=inmgr->tmv[i];
    if (other->vid!=tm->vid) continue;
    if (other->pid!=tm->pid) continue;
    if (other->version!=tm->version) continue;
    if (other->namec!=tm->namec) continue;
    if (memcmp(other->name,tm->name,tm->namec)) continue;
    if (inmgr_tm_bodysnatch(other,tm)<0) return -1;
    inmgr_tm_del(tm);
    inmgr->tmv_dirty=1;
    return 0;
  }
  
  /* Add to front.
   */
  if (inmgr_tmv_require(inmgr)<0) return -1;
  memmove(inmgr->tmv+1,inmgr->tmv,sizeof(void*)*inmgr->tmc);
  inmgr->tmc++;
  inmgr->tmv[0]=tm;
  inmgr->tmv_dirty=1;

  return 0;
}

/* Match.
 */
 
int inmgr_tm_match(const struct inmgr_tm *tm,int vid,int pid,int version,const char *name,int namec) {
  if (!tm) return 0;
  if (tm->vid&&(tm->vid!=vid)) return 0;
  if (tm->pid&&(tm->pid!=pid)) return 0;
  if (tm->version&&(tm->version!=version)) return 0;
  if (tm->namec) {
    if (!name) namec=0; else if (namec<0) { namec=0; while (name[namec]) namec++; }
    if (tm->namec!=namec) return 0;
    if (memcmp(tm->name,name,namec)) return 0;
  }
  return 1;
}

/* Add button to device, with all fields final.
 * NB (srclo,srchi) are the inclusive endpoints of the ON range, they are not the device's logical range.
 */
 
static int inmgr_tm_apply_verbatim(
  struct inmgr_device *device,
  int srcbtnid,int dstbtnid,
  int srclo,int srchi,int srcvalue
) {
  int p=inmgr_device_buttonv_search(device,srcbtnid);
  if (p<0) p=-p-1;
  if (device->buttonc>=device->buttona) {
    int na=device->buttona+32;
    if (na>INT_MAX/sizeof(struct inmgr_device_button)) return -1;
    void *nv=realloc(device->buttonv,sizeof(struct inmgr_device_button)*na);
    if (!nv) return -1;
    device->buttonv=nv;
    device->buttona=na;
  }
  struct inmgr_device_button *button=device->buttonv+p;
  memmove(button+1,button,sizeof(struct inmgr_device_button)*(device->buttonc-p));
  device->buttonc++;
  memset(button,0,sizeof(struct inmgr_device_button));
  button->srcbtnid=srcbtnid;
  button->dstbtnid=dstbtnid;
  button->srclo=srclo;
  button->srchi=srchi;
  button->srcvalue=srcvalue;
  return 0;
}

/* Apply twostate and hat.
 */
 
static int inmgr_tm_apply_twostate(struct inmgr_device *device,int srcbtnid,int dstbtnid,int lo,int hi,int value) {
  if (lo>hi-1) return 0;
  return inmgr_tm_apply_verbatim(device,srcbtnid,dstbtnid,lo+1,INT_MAX,value);
}

static int inmgr_tm_apply_hat(struct inmgr_device *device,int srcbtnid,int srclo,int srcvalue) {
  return inmgr_tm_apply_verbatim(device,srcbtnid,EGG_BTN_DPAD,srclo,srclo+7,srcvalue);
}

/* Apply with dynamic caps from the device.
 * It's possible that we've just synthesized the template, in which case we iterate the buttons twice.
 * That's annoying but I think still the right way to go.
 */
 
struct inmgr_tm_apply_context {
  struct inmgr *inmgr;
  struct inmgr_device *device;
  struct inmgr_tm *tm;
  struct hostio_input *driver;
};

static int inmgr_tm_apply_cb(int srcbtnid,int hidusage,int lo,int hi,int value,void *userdata) {
  struct inmgr_tm_apply_context *ctx=userdata;
  const struct inmgr_tm_button *tmbtn=inmgr_tm_buttonv_get(ctx->tm,srcbtnid);
  if (!tmbtn) return 0;
  
  if (!tmbtn->dstbtnid) return 0;
  
  // Signals are two-state for mapping purposes.
  if (tmbtn->dstbtnid>0xffff) return inmgr_tm_apply_twostate(ctx->device,srcbtnid,tmbtn->dstbtnid,lo,hi,value);
  
  // Hats go out as a single entry, but we must ensure range is exactly 8.
  if (tmbtn->dstbtnid==EGG_BTN_DPAD) {
    int range=hi-lo+1;
    if (range!=8) return 0;
    return inmgr_tm_apply_hat(ctx->device,srcbtnid,lo,value);
  }
  
  // HORZ and VERT aggregates produce two entries, and range must be at least 3.
  if ((tmbtn->dstbtnid==EGG_BTN_HORZ)||(tmbtn->dstbtnid==EGG_BTN_VERT)) {
    if (lo>hi-1) return 0;
    int mid=(lo+hi)>>1;
    int midlo=(lo+mid)>>1;
    int midhi=(mid+hi)>>1;
    if (midlo>=mid) midlo=mid-1;
    if (midhi<=mid) midhi=mid+1;
    if (inmgr_tm_apply_verbatim(ctx->device,srcbtnid,tmbtn->dstbtnid&(EGG_BTN_LEFT|EGG_BTN_UP),INT_MIN,midlo,value)<0) return -1;
    if (inmgr_tm_apply_verbatim(ctx->device,srcbtnid,tmbtn->dstbtnid&(EGG_BTN_RIGHT|EGG_BTN_DOWN),midhi,INT_MAX,value)<0) return -1;
    return 0;
  }
  
  // Anything else must be a single bit under 0x10000, and emits as two-state.
  int bit=1; for (;bit<0x10000;bit<<=1) {
    if (bit==tmbtn->dstbtnid) {
      return inmgr_tm_apply_twostate(ctx->device,srcbtnid,bit,lo,hi,value);
    }
  }
  return 0;
}
 
static int inmgr_tm_apply_with_caps(struct inmgr *inmgr,struct inmgr_device *device,struct inmgr_tm *tm,struct hostio_input *driver) {
  struct inmgr_tm_apply_context ctx={.inmgr=inmgr,.device=device,.tm=tm,.driver=driver};
  return driver->type->for_each_button(driver,device->devid,inmgr_tm_apply_cb,&ctx);
}

/* Apply template to device without dynamic caps.
 * Assume every button exists and that none is an aggregate.
 */
 
static int inmgr_tm_apply_blind(struct inmgr *inmgr,struct inmgr_device *device,struct inmgr_tm *tm) {

  // We know we're creating exactly one device button per template button, so allocate them all in advance.
  device->buttona=device->buttonc=0;
  if (device->buttonv) free(device->buttonv);
  if (!(device->buttonv=malloc(sizeof(struct inmgr_device_button)*tm->buttonc))) return -1;
  device->buttona=tm->buttonc;

  struct inmgr_device_button *dbtn=device->buttonv;
  const struct inmgr_tm_button *tmbtn=tm->buttonv;
  int i=tm->buttonc;
  for (;i-->0;dbtn++,tmbtn++) {
    dbtn->srcbtnid=tmbtn->srcbtnid;
    dbtn->dstbtnid=tmbtn->dstbtnid;
    dbtn->srcvalue=0;
    dbtn->dstvalue=0;
    dbtn->srclo=1;
    dbtn->srchi=INT_MAX;
  }
  device->buttonc=tm->buttonc;
  
  return 0;
}

/* Apply.
 */
 
int inmgr_tm_apply(struct inmgr *inmgr,struct inmgr_device *device,struct inmgr_tm *tm,struct hostio_input *driver) {
  if (!inmgr||!device||!tm) return -1;
  if (tm->buttonc<1) return 0;
  if (driver&&driver->type->for_each_button) return inmgr_tm_apply_with_caps(inmgr,device,tm,driver);
  else return inmgr_tm_apply_blind(inmgr,device,tm);
}

/* Decode and store from text.
 */

int inmgr_decode_tmv(struct inmgr *inmgr,const char *src,int srcc,const char *path) {
  struct sr_decoder decoder={.v=src,.c=srcc};
  const char *line;
  int linec,lineno=1,err;
  struct inmgr_tm *tm=0;
  for (;(linec=sr_decode_line(&line,&decoder))>0;lineno++) {
    while (linec&&((unsigned char)line[linec-1]<=0x20)) linec--;
    while (linec&&((unsigned char)line[0]<=0x20)) { linec--; line++; }
    if (!linec) continue;
    
    if ((linec>=6)&&!memcmp(line,"device",6)) {
      int linep=6,vid=0,pid=0,version=0;
      while ((linep<linec)&&((unsigned char)line[linep]<=0x20)) linep++;
      const char *token=line+linep;
      int tokenc=0;
      while ((linep<linec)&&((unsigned char)line[linep++]>0x20)) tokenc++;
      while ((linep<linec)&&((unsigned char)line[linep]<=0x20)) linep++;
      if (sr_int_eval(&vid,token,tokenc)<2) {
        fprintf(stderr,"%s:%d: Expected Vendor ID, found '%.*s'\n",path,lineno,tokenc,token);
        return -2;
      }
      token=line+linep;
      tokenc=0;
      while ((linep<linec)&&((unsigned char)line[linep++]>0x20)) tokenc++;
      while ((linep<linec)&&((unsigned char)line[linep]<=0x20)) linep++;
      if (sr_int_eval(&pid,token,tokenc)<2) {
        fprintf(stderr,"%s:%d: Expected Product ID, found '%.*s'\n",path,lineno,tokenc,token);
        return -2;
      }
      token=line+linep;
      tokenc=0;
      while ((linep<linec)&&((unsigned char)line[linep++]>0x20)) tokenc++;
      while ((linep<linec)&&((unsigned char)line[linep]<=0x20)) linep++;
      if (sr_int_eval(&version,token,tokenc)<2) {
        fprintf(stderr,"%s:%d: Expected Version, found '%.*s'\n",path,lineno,tokenc,token);
        return -2;
      }
      const char *name=line+linep;
      int namec=linec-linep;
      if (!(tm=inmgr_tmv_synthesize(inmgr,0,0,vid,pid,version,name,namec))) return -1;
      continue;
    }
    
    if (!tm) {
      fprintf(stderr,"%s:%d: Expected 'device VID PID VERSION NAME'\n",path,lineno);
      return -2;
    }
    
    const char *token=line;
    int tokenc=0;
    int linep=0,srcbtnid,dstbtnid;
    while ((linep<linec)&&((unsigned char)line[linep++]>0x20)) tokenc++;
    while ((linep<linec)&&((unsigned char)line[linep]<=0x20)) linep++;
    if (sr_int_eval(&srcbtnid,token,tokenc)<1) {
      fprintf(stderr,"%s:%d: Expected integer (srcbtnid), found '%.*s'\n",path,lineno,tokenc,token);
      return -2;
    }
    token=line+linep;
    tokenc=linec-linep;
    if (inmgr_button_eval(&dstbtnid,token,tokenc)<0) {
      fprintf(stderr,"%s:%d: Expected button name, found '%.*s'\n",path,lineno,tokenc,token);
      return -2;
    }
    
    if (!inmgr_tm_buttonv_insert(tm,srcbtnid,dstbtnid)) return -1;
  }
  return 0;
}

/* Encode text.
 */
 
int inmgr_encode_tmv(struct sr_encoder *dst,struct inmgr *inmgr) {
  int i=0;
  for (;i<inmgr->tmc;i++) {
    struct inmgr_tm *tm=inmgr->tmv[i];
    if (sr_encode_fmt(dst,"device 0x%04x 0x%04x 0x%04x %.*s\n",tm->vid,tm->pid,tm->version,tm->namec,tm->name)<0) return -1;
    const struct inmgr_tm_button *button=tm->buttonv;
    int ii=tm->buttonc;
    for (;ii-->0;button++) {
      char bname[32];
      int bnamec=inmgr_button_repr(bname,sizeof(bname),button->dstbtnid);
      if ((bnamec<1)||(bnamec>sizeof(bname))) {
        bnamec=sr_decsint_repr(bname,sizeof(bname),button->dstbtnid);
        if ((bnamec<1)||(bnamec>sizeof(bname))) return -1;
      }
      if (sr_encode_fmt(dst,"  0x%08x %.*s\n",button->srcbtnid,bnamec,bname)<0) return -1;
    }
    if (sr_encode_u8(dst,0x0a)<0) return -1; // Blank line between blocks. Not necessary but polite.
  }
  return 0;
}
