#include "egg_runner_internal.h"
#include "egg_inmap.h"

#define EGG_EVENTQ_LIMIT 256
#define EGG_BUTTON_LIMIT 128 /* Refuse to list more than so many buttons per device, as a safety measure. */

/* Object definition.
 * We wastefully duplicate the entire device list, including every button on every device.
 * This speeds things up for public access.
 */
 
struct egg_inmgr {
  union egg_event eventq[EGG_EVENTQ_LIMIT];
  int eventp,eventc;
  int eventmask;
  int cursor_desired;
  struct egg_device **devicev;
  int devicec,devicea;
  int mousex,mousey;
  int lock_fake_cursor;
  int show_fake_cursor;
  int texid_cursor;
  struct egg_inmap inmap;
  struct egg_inmap_rules *kbdrules; // WEAK, OPTIONAL
  uint8_t keyv[32]; // set bit for each held key; bits little-endian
};

/* Cleanup.
 */
 
static void egg_device_del(struct egg_device *device) {
  if (!device) return;
  if (device->name) free(device->name);
  if (device->buttonv) free(device->buttonv);
  free(device);
}
 
void egg_inmgr_del(struct egg_inmgr *inmgr) {
  if (!inmgr) return;
  if (inmgr->devicev) {
    while (inmgr->devicec-->0) egg_device_del(inmgr->devicev[inmgr->devicec]);
    free(inmgr->devicev);
  }
  egg_inmap_cleanup(&inmgr->inmap);
  free(inmgr);
}

/* New.
 */
 
struct egg_inmgr *egg_inmgr_new() {
  struct egg_inmgr *inmgr=calloc(1,sizeof(struct egg_inmgr));
  if (!inmgr) return 0;
  
  inmgr->eventmask=(
    (1<<EGG_EVENT_JOY)|
    (1<<EGG_EVENT_KEY)|
    //(1<<EGG_EVENT_TOUCH)| // TOUCH should enable by default, but we're not supporting at all.
  0);
  inmgr->cursor_desired=1;
  
  if (egg_inmap_load(&inmgr->inmap)<0) {
    egg_inmgr_del(inmgr);
    return 0;
  }
  if (inmgr->kbdrules=egg_inmap_rules_for_device(&inmgr->inmap,0xffff,0xffff,0,"System Keyboard",-1)) {
    fprintf(stderr,"%s: Input rules for System Keyboard will be processed by platform and not sent to game.\n",inmgr->inmap.cfgpath);
  }
  
  return inmgr;
}

struct egg_inmap *egg_inmgr_get_inmap(struct egg_inmgr *inmgr) {
  return &inmgr->inmap;
}

/* Test event capability.
 *   <1: Forbidden.
 *    1: Optional.
 *   >1: Required.
 */
 
static int egg_event_capable(int type) {
  switch (type) {
    case EGG_EVENT_JOY: return 0;
    case EGG_EVENT_KEY: return 0;
    case EGG_EVENT_TEXT: return 0;
    case EGG_EVENT_MMOTION: return 0;
    case EGG_EVENT_MBUTTON: return 0;
    case EGG_EVENT_MWHEEL: return 0;
    case EGG_EVENT_TOUCH: return -1;
    case EGG_EVENT_ACCEL: return -1;
    case EGG_EVENT_RAW: return 0;
  }
  return 0;
}

/* React to changes to the mouse event mask or visibility request.
 */
 
static int egg_inmgr_has_fake_mouse(const struct egg_inmgr *inmgr) {
  int i=inmgr->devicec;
  while (i-->0) {
    struct egg_device *device=inmgr->devicev[i];
    if (device->rptcls==EGG_EVENT_MMOTION) return 1;
  }
  return 0;
}
 
static void egg_inmgr_show_cursor(struct egg_inmgr *inmgr,int show) {
  if (egg.hostio->video->type->show_cursor) {
    egg.hostio->video->type->show_cursor(egg.hostio->video,show);
    inmgr->show_fake_cursor=0;
  } else if (show&&egg_inmgr_has_fake_mouse(inmgr)) {
    inmgr->show_fake_cursor=1;
  } else {
    inmgr->show_fake_cursor=0;
  }
}
 
static void egg_inmgr_refresh_cursor(struct egg_inmgr *inmgr) {
  const int any_mouse_event=(
    (1<<EGG_EVENT_MMOTION)|
    (1<<EGG_EVENT_MBUTTON)|
    (1<<EGG_EVENT_MWHEEL)|
  0);
  if (inmgr->eventmask&any_mouse_event) {
    if (inmgr->cursor_desired) {
      egg_inmgr_show_cursor(inmgr,1);
    } else {
      egg_inmgr_show_cursor(inmgr,0);
    }
  } else {
    egg_inmgr_show_cursor(inmgr,0);
  }
}

/* Update.
 * Probably nothing for us to do?
 * If we ever do something like faking a pointer via key or touch events, it would require maintenance here.
 */
 
int egg_inmgr_update(struct egg_inmgr *inmgr) {
  return 0;
}

/* Render.
 */
 
static uint8_t egg_fake_cursor_image[]={
#define _ 0,0,0,0,
#define K 0,0,0,255,
#define W 255,255,255,255,
  K K K K K K K K
  K W W W W W W K
  K W W W W W K _
  K W W W W K _ _
  K W W W W W K _
  K W W K W W W K
  K W K _ K W W K
  K K _ _ _ K K _
#undef _
#undef K
#undef W
};
 
void egg_inmgr_render(struct egg_inmgr *inmgr) {
  if (inmgr->show_fake_cursor&&!inmgr->lock_fake_cursor) {
    if ((inmgr->mousex<0)||(inmgr->mousey<0)) return; // We don't immediately know the right and bottom edges, but it shouldn't matter, the image is offscreen by then.
    if (!inmgr->texid_cursor) {
      if ((inmgr->texid_cursor=render_texture_new(egg.render))<1) return;
      render_texture_load(egg.render,inmgr->texid_cursor,8,8,32,EGG_TEX_FMT_RGBA,egg_fake_cursor_image,sizeof(egg_fake_cursor_image));
    }
    render_tint(egg.render,0);
    render_alpha(egg.render,0xff);
    render_draw_decal(egg.render,1,inmgr->texid_cursor,inmgr->mousex,inmgr->mousey,0,0,8,8,0);
  }
}

/* Push to event queue.
 * Never fails. If the queue is full, we'll log a warning and overwrite one entry.
 */
 
static union egg_event *egg_event_push(struct egg_inmgr *inmgr) {
  int p=inmgr->eventp+inmgr->eventc;
  if (p>=EGG_EVENTQ_LIMIT) p-=EGG_EVENTQ_LIMIT;
  union egg_event *event=inmgr->eventq+p;
  if (inmgr->eventc>=EGG_EVENTQ_LIMIT) {
    fprintf(stderr,"%s:ERROR: Event queue exhausted. Overwriting one event.\n",egg.exename);
    if (++(inmgr->eventp)>=EGG_EVENTQ_LIMIT) inmgr->eventp=0;
  } else {
    inmgr->eventc++;
  }
  return event;
}

/* Button list in device.
 */
 
static int egg_device_buttonv_search(const struct egg_device *device,int btnid) {
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

static struct egg_button *egg_device_buttonv_insert(struct egg_device *device,int p,int btnid) {
  if ((p<0)||(p>device->buttonc)) return 0;
  if (device->buttonc>=device->buttona) {
    if (device->buttona>EGG_BUTTON_LIMIT) return 0;
    int na=device->buttona+32;
    void *nv=realloc(device->buttonv,sizeof(struct egg_button)*na);
    if (!nv) return 0;
    device->buttonv=nv;
    device->buttona=na;
  }
  struct egg_button *button=device->buttonv+p;
  memmove(button+1,button,sizeof(struct egg_button)*(device->buttonc-p));
  device->buttonc++;
  memset(button,0,sizeof(struct egg_button));
  button->btnid=btnid;
  return button;
}

static struct egg_button *egg_device_get_button(const struct egg_device *device,int btnid) {
  int p=egg_device_buttonv_search(device,btnid);
  if (p<0) return 0;
  return device->buttonv+p;
}

/* Search device list.
 */
 
static int egg_inmgr_devicev_search(const struct egg_inmgr *inmgr,int devid) {
  int lo=0,hi=inmgr->devicec;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
    int q=inmgr->devicev[ck]->devid;
         if (devid<q) hi=ck;
    else if (devid>q) lo=ck+1;
    else return ck;
  }
  return -lo-1;
}

/* Get device.
 */
 
static inline struct egg_device *egg_inmgr_get_device(const struct egg_inmgr *inmgr,int devid) {
  int p=egg_inmgr_devicev_search(inmgr,devid);
  if (p<0) return 0;
  return inmgr->devicev[p];
}

const char *egg_inmgr_get_device_ids(int *vid,int *pid,int *version,const struct egg_inmgr *inmgr,int devid) {
  struct egg_device *device=egg_inmgr_get_device(inmgr,devid);
  if (!device) return 0;
  *vid=device->vid;
  *pid=device->pid;
  *version=device->version;
  return device->name;
}

/* Remove device.
 */
 
static void egg_inmgr_remove_device(struct egg_inmgr *inmgr,int devid) {
  int p=egg_inmgr_devicev_search(inmgr,devid);
  if (p<0) return;
  struct egg_device *device=inmgr->devicev[p];
  inmgr->devicec--;
  memmove(inmgr->devicev+p,inmgr->devicev+p+1,sizeof(void*)*(inmgr->devicec-p));
  egg_device_del(device);
}

/* Receive device's button declarations.
 * No need to make big decisions at this point, just insert it in the list.
 */
 
static int egg_inmgr_cb_declare_button(int btnid,int hidusage,int lo,int hi,int value,void *userdata) {
  struct egg_device *device=userdata;
  int range=hi-lo+1;
  if (range<2) return 0; // Not interesting.
  int p=egg_device_buttonv_search(device,btnid);
  if (p>=0) return 0; // Already got it? Something is fishy.
  p=-p-1;
  struct egg_button *button=egg_device_buttonv_insert(device,p,btnid);
  if (!button) return -1; // Stop iterating if we refused to add, maybe there's too many.
  button->hidusage=hidusage;
  button->lo=lo;
  button->hi=hi;
  button->value=value;
  return 0;
}

/* Apply existing rules to device.
 */
 
static int egg_device_apply_rules(struct egg_inmgr *inmgr,struct egg_device *device,struct egg_inmap_rules *rules) {
  struct egg_button *button=device->buttonv;
  int i=device->buttonc;
  for (;i-->0;button++) {
    int dstbtnid=egg_inmap_rules_get_button(rules,button->btnid);
    if (dstbtnid<1) continue;
    button->dstbtnid=dstbtnid;
    switch (dstbtnid) {
    
      // To analogue axis: Verify range of at least 3. Set (srclo,srchi) to the full range, swap if reversing.
      case EGG_JOYBTN_LX:
      case EGG_JOYBTN_LY:
      case EGG_JOYBTN_RX:
      case EGG_JOYBTN_RY: {
          if (button->lo>button->hi-2) {
            button->dstbtnid=0;
          } else {
            button->srclo=button->lo;
            button->srchi=button->hi;
          }
        } break;
      case EGG_INMAP_BTN_NLX:
      case EGG_INMAP_BTN_NLY:
      case EGG_INMAP_BTN_NRX:
      case EGG_INMAP_BTN_NRY: {
          if (button->lo>button->hi-2) {
            button->dstbtnid=0;
          } else {
            button->srclo=button->hi;
            button->srchi=button->lo;
          }
        } break;
        
      // To dpad axis: Verify range of at least 3, then set thresholds (srclo,srchi).
      case EGG_INMAP_BTN_HORZ:
      case EGG_INMAP_BTN_VERT:
      case EGG_INMAP_BTN_NHORZ:
      case EGG_INMAP_BTN_NVERT: {
          if (button->lo>button->hi-2) {
            button->dstbtnid=0;
          } else {
            int mid=(button->lo+button->hi)>>1;
            int midlo=(button->lo+mid)>>1;
            int midhi=(button->hi+mid)>>1;
            if (midlo>=mid) midlo=mid-1;
            if (midhi<=mid) midhi=mid+1;
            if ((dstbtnid==EGG_INMAP_BTN_NHORZ)||(dstbtnid==EGG_INMAP_BTN_NVERT)) {
              button->srclo=midhi;
              button->srchi=midlo;
            } else {
              button->srclo=midlo;
              button->srchi=midhi;
            }
          }
        } break;
        
      // To full dpad: Verify range of exactly 8. We'll use (lo), and (hi) will be ignored.
      case EGG_INMAP_BTN_DPAD: {
          if (button->lo!=button->hi-7) {
            button->dstbtnid=0;
          }
        } break;
        
      // Everything else has a 2-state output, and we'll map zero/nonzero. Verify they have a low value of zero.
      default: {
          if (button->lo) {
            button->dstbtnid=0;
          }
        }
    }
  }
  
  return 0;
}

/* Configure new device.
 * Decide what kind of thing it is, and set (rptcls) accordingly.
 * If buttons need mapped, anything like that, set it all up.
 * Do not push any events or remove the device.
 * Fail, or leave (rptcls) zero, and the caller should disconnect it.
 */
 
static int egg_device_configure(struct egg_inmgr *inmgr,struct egg_device *device) {
  
  /* If RAW is enabled and JOY is not, take the somewhat risky position that the device should stay in RAW mode.
   */
  if (
    (inmgr->eventmask&(1<<EGG_EVENT_RAW))&&
    !(inmgr->eventmask&(1<<EGG_EVENT_JOY))
  ) {
    device->rptcls=EGG_EVENT_RAW;
    return 0;
  }
  
  /* If matches a rule set from the config file, go with that.
   */
  struct egg_inmap_rules *rules=egg_inmap_rules_for_device(
    &inmgr->inmap,device->vid,device->pid,device->version,device->name,device->namec
  );
  if (rules) {
    device->rptcls=EGG_EVENT_JOY;
    if (egg_device_apply_rules(inmgr,device,rules)<0) return -1;
    return 0;
  }
  
  /* If it has at least 40 keys and one of them is 0x00070004 (Keyboard A), call it a keyboard.
   * These map straight off the buttons' hidusage.
   */
  if (device->buttonc>=40) {
    const struct egg_button *button=device->buttonv;
    int i=device->buttonc;
    for (;i-->0;button++) {
      if (button->hidusage==0x00070004) {
        device->rptcls=EGG_EVENT_KEY;
        return 0;
      }
    }
  }
  
  /* REL_X, REL_Y, and BTN_LEFT, assume it's a mouse.
   * We're using evdev button ids here, this is only going to work for Linux.
   * And of course if you're running a window manager, the mouse device shouldn't appear at all.
   * Rewrite hidusage, though hopefully the driver already set it up like this.
   */
  struct egg_button *relx=egg_device_get_button(device,0x00020000);
  if (relx) {
    struct egg_button *rely=egg_device_get_button(device,0x00020001);
    struct egg_button *bleft=egg_device_get_button(device,0x00010110);
    if (rely&&bleft) {
      device->rptcls=EGG_EVENT_MMOTION;
      relx->hidusage=0x00010030;
      rely->hidusage=0x00010031;
      bleft->hidusage=0x00090000;
      struct egg_button *button;
      if (button=egg_device_get_button(device,0x00010111)) button->hidusage=0x00090001; // right
      if (button=egg_device_get_button(device,0x00010112)) button->hidusage=0x00090002; // middle
      if (button=egg_device_get_button(device,0x00020006)) button->hidusage=0x00010033; // hwheel, calling it "Rx" in HID terms
      if (button=egg_device_get_button(device,0x00020008)) button->hidusage=0x00010038; // wheel
      int btnusagenext=0x00090003;
      int i=device->buttonc;
      for (button=device->buttonv;i-->0;button++) {
        if (button->hidusage) continue; // already got it, cool
        if ((button->lo==0)&&((button->hi==1)||(button->hi==2))) {
          button->hidusage=btnusagenext++;
        }
      }
      device->w=egg.hostio->video->w;
      device->h=egg.hostio->video->h;
      device->x=device->w>>1;
      device->y=device->h>>1;
      egg_inmgr_refresh_cursor(inmgr);
      return 0;
    }
  }
  
  /* Try to synthesize rules.
   */
  if (rules=egg_inmap_synthesize_rules(&inmgr->inmap,device)) {
    device->rptcls=EGG_EVENT_JOY;
    if (egg_device_apply_rules(inmgr,device,rules)<0) return -1;
    if (inmgr->inmap.cfgpath) { // Only advise to reconfigure if rules can be saved.
      fprintf(stderr,
        "%s: Generated mapping for input device %04x:%04x:%04x '%.*s' by guessing wildly.\n",
        egg.exename,device->vid,device->pid,device->version,device->namec,device->name
      );
      fprintf(stderr,
        "%s: If it's not mapped right, edit '%s' or relaunch with '--configure-input'.\n",
        egg.exename,inmgr->inmap.cfgpath
      );
    }
    return 0;
  }

  return 0;
}

/* Add device.
 */
 
static struct egg_device *egg_inmgr_add_device(struct egg_inmgr *inmgr,struct hostio_input *driver,int devid) {

  // Allocate device and insert in list.
  if (devid<1) return 0;
  int p=egg_inmgr_devicev_search(inmgr,devid);
  if (p>=0) return 0;
  p=-p-1;
  if (inmgr->devicec>=inmgr->devicea) {
    int na=inmgr->devicea+16;
    if (na>INT_MAX/sizeof(void*)) return 0;
    void *nv=realloc(inmgr->devicev,sizeof(void*)*na);
    if (!nv) return 0;
    inmgr->devicev=nv;
    inmgr->devicea=na;
  }
  struct egg_device *device=calloc(1,sizeof(struct egg_device));
  if (!device) return 0;
  memmove(inmgr->devicev+p+1,inmgr->devicev+p,sizeof(void*)*(inmgr->devicec-p));
  inmgr->devicec++;
  inmgr->devicev[p]=device;
  device->devid=devid;
  device->driver=driver;
  
  // Collect IDs and name.
  if (driver->type->get_ids) {
    const char *name=driver->type->get_ids(&device->vid,&device->pid,&device->version,driver,devid);
    if (name&&name[0]) {
      int namec=0; while (name[namec]) namec++;
      while (namec&&((unsigned char)name[namec-1]<=0x20)) namec--;
      while (namec&&((unsigned char)name[0]<=0x20)) { namec--; name++; }
      if (device->name=malloc(namec+1)) {
        memcpy(device->name,name,namec);
        device->name[namec]=0;
        device->namec=namec;
      }
    }
  }
  
  // Collect buttons.
  if (driver->type->for_each_button) {
    driver->type->for_each_button(driver,devid,egg_inmgr_cb_declare_button,device);
  }
  
  // Decide how we're going to use the device (keyboard, mouse, joystick, touch, accelerometer?), and configure that.
  if ((egg_device_configure(egg.inmgr,device)<0)||!device->rptcls) {
    device->rptcls=EGG_EVENT_RAW;
    return device;
  }
  
  return device;
}

/* Range for one button.
 */
 
void egg_inmgr_get_button_range(int *lo,int *hi,int *rest,const struct egg_inmgr *inmgr,int devid,int btnid) {
  struct egg_device *device=egg_inmgr_get_device(inmgr,devid);
  if (!device) return;
  struct egg_button *button=egg_device_get_button(device,btnid);
  if (!button) return;
  *lo=button->lo;
  *hi=button->hi;
  *rest=button->value;
}

/* hostio callbacks.
 ******************************************************************************************************/

int egg_cb_key(struct hostio_video *driver,int keycode,int value) {

  // Update tracking bits.
  if ((keycode>=0x00070000)&&(keycode<0x00070100)) {
    int subid=keycode&0xff;
    int p=subid>>3;
    uint8_t mask=1<<(subid&7);
    if (value) egg.inmgr->keyv[p]|=mask;
    else egg.inmgr->keyv[p]&=~mask;
  }

  // If a global stateless action is mapped to this key, fire that and suppress the event.
  // We're not checking that it actually is one of the stateless actions: Anything mapped to System Keyboard is presumed to be.
  // Keys so mapped *will* report Repeat and Release events, since we won't bother searching for those.
  if (egg.inmgr->kbdrules&&(value==1)) {
    int dstbtnid=egg_inmap_rules_get_button(egg.inmgr->kbdrules,keycode);
    if (dstbtnid) {
      egg_perform_user_action(dstbtnid);
      return 0;
    }
  }

  if (egg.inmgr->eventmask&(1<<EGG_EVENT_KEY)) {
    union egg_event *event=egg_event_push(egg.inmgr);
    event->key.type=EGG_EVENT_KEY;
    event->key.keycode=keycode;
    event->key.value=value;
  }
  if (egg.inmgr->eventmask&(1<<EGG_EVENT_TEXT)) return 0;
  return 1;
}

void egg_cb_text(struct hostio_video *driver,int codepoint) {
  if (!(egg.inmgr->eventmask&(1<<EGG_EVENT_TEXT))) return;
  union egg_event *event=egg_event_push(egg.inmgr);
  event->text.type=EGG_EVENT_TEXT;
  event->text.codepoint=codepoint;
}
 
void egg_cb_mmotion(struct hostio_video *driver,int x,int y) {
  int locked=0;
  if (egg.hostio->video->type->lock_cursor) {
    locked=egg.hostio->video->cursor_locked;
  } else {
    locked=egg.inmgr->lock_fake_cursor;
  }
  if (locked) {
    if (!egg.directgl) {
      int fbw=1,fbh=1;
      render_texture_get_header(&fbw,&fbh,0,egg.render,1);
      x=(x*fbw)/egg.hostio->video->w;
      y=(y*fbh)/egg.hostio->video->h;
    }
    if (!x&&!y) return;
    egg.inmgr->mousex=0;
    egg.inmgr->mousey=0;
  } else {
    if (!egg.directgl) {
      render_coords_fb_from_screen(egg.render,&x,&y);
    }
    if ((x==egg.inmgr->mousex)&&(y==egg.inmgr->mousey)) return;
    egg.inmgr->mousex=x;
    egg.inmgr->mousey=y;
  }
  if (!(egg.inmgr->eventmask&(1<<EGG_EVENT_MMOTION))) return;
  if (egg.inmgr->show_fake_cursor&&!x&&!y) return;
  union egg_event *event=egg_event_push(egg.inmgr);
  event->mmotion.type=EGG_EVENT_MMOTION;
  event->mmotion.x=x;
  event->mmotion.y=y;
}

void egg_cb_mbutton(struct hostio_video *driver,int btnid,int value) {
  if (!(egg.inmgr->eventmask&(1<<EGG_EVENT_MBUTTON))) return;
  union egg_event *event=egg_event_push(egg.inmgr);
  event->mbutton.type=EGG_EVENT_MBUTTON;
  event->mbutton.x=egg.inmgr->mousex;
  event->mbutton.y=egg.inmgr->mousey;
  event->mbutton.btnid=btnid;
  event->mbutton.value=value;
}

void egg_cb_mwheel(struct hostio_video *driver,int dx,int dy) {
  if (!(egg.inmgr->eventmask&(1<<EGG_EVENT_MWHEEL))) return;
  union egg_event *event=egg_event_push(egg.inmgr);
  event->mwheel.type=EGG_EVENT_MWHEEL;
  event->mwheel.x=egg.inmgr->mousex;
  event->mwheel.y=egg.inmgr->mousey;
  event->mwheel.dx=dx;
  event->mwheel.dy=dy;
}

void egg_cb_connect(struct hostio_input *driver,int devid) {
  struct egg_device *device=egg_inmgr_add_device(egg.inmgr,driver,devid);
  if (!device) return;
  switch (device->rptcls) {
  
    case EGG_EVENT_RAW: {
        if (egg.inmgr->eventmask&(1<<EGG_EVENT_RAW)) {
          union egg_event *event=egg_event_push(egg.inmgr);
          event->raw.type=EGG_EVENT_RAW;
          event->raw.devid=devid;
          event->raw.btnid=0; // "connection state"
          event->raw.value=1;
        }
      } break;
  
    case EGG_EVENT_JOY: {
        if (egg.inmgr->eventmask&(1<<EGG_EVENT_RAW)) {
          union egg_event *event=egg_event_push(egg.inmgr);
          event->raw.type=EGG_EVENT_RAW;
          event->raw.devid=devid;
          event->raw.btnid=0; // "connection state"
          event->raw.value=1;
        }
        if (egg.inmgr->eventmask&(1<<EGG_EVENT_JOY)) {
          union egg_event *event=egg_event_push(egg.inmgr);
          event->joy.type=EGG_EVENT_JOY;
          event->joy.devid=devid;
          event->joy.btnid=0; // "connection state"
          event->joy.value=1;
        }
      } break;
    
    // Other reporting classes, I think we don't need any "connect" event.
    case EGG_EVENT_KEY: break;
    case EGG_EVENT_MMOTION: break;
    case EGG_EVENT_TOUCH: break;
    case EGG_EVENT_ACCEL: break;
  }
}

void egg_cb_disconnect(struct hostio_input *driver,int devid) {
  struct egg_device *device=egg_inmgr_get_device(egg.inmgr,devid);
  if (!device) return;
  int checkcursor=0;
  switch (device->rptcls) {
  
    case EGG_EVENT_RAW: {
        if (egg.inmgr->eventmask&(1<<EGG_EVENT_RAW)) {
          union egg_event *event=egg_event_push(egg.inmgr);
          event->raw.type=EGG_EVENT_RAW;
          event->raw.devid=devid;
          event->raw.btnid=0; // "connection state"
          event->raw.value=0;
        }
      } break;
  
    case EGG_EVENT_JOY: {
        if (egg.inmgr->eventmask&(1<<EGG_EVENT_RAW)) {
          union egg_event *event=egg_event_push(egg.inmgr);
          event->raw.type=EGG_EVENT_RAW;
          event->raw.devid=devid;
          event->raw.btnid=0; // "connection state"
          event->raw.value=0;
        }
        if (egg.inmgr->eventmask&(1<<EGG_EVENT_JOY)) {
          union egg_event *event=egg_event_push(egg.inmgr);
          event->joy.type=EGG_EVENT_JOY;
          event->joy.devid=devid;
          event->joy.btnid=0; // "connection state"
          event->joy.value=0;
        }
      } break;
      
    case EGG_EVENT_KEY: {
        if (egg.inmgr->eventmask&(1<<EGG_EVENT_KEY)) {
          struct egg_button *button=device->buttonv;
          int i=device->buttonc;
          for (;i-->0;button++) {
            if (!button->value) continue;
            if ((button->hidusage<0x00070000)||(button->hidusage>=0x00080000)) continue;
            union egg_event *event=egg_event_push(egg.inmgr);
            event->key.type=EGG_EVENT_KEY;
            event->key.keycode=button->hidusage;
            event->key.value=0;
          }
        }
      } break;
      
    case EGG_EVENT_MMOTION: {
        //TODO drop held buttons
        checkcursor=1;
      } break;

    case EGG_EVENT_TOUCH: break;//TODO?
    case EGG_EVENT_ACCEL: break;//TODO?
  }
  egg_inmgr_remove_device(egg.inmgr,devid);
  if (checkcursor) egg_inmgr_refresh_cursor(egg.inmgr);
}

static void egg_send_joy_event(int devid,int btnid,int value) {
  if (!(egg.inmgr->eventmask&(1<<EGG_EVENT_JOY))) return;
  union egg_event *event=egg_event_push(egg.inmgr);
  event->joy.type=EGG_EVENT_JOY;
  event->joy.devid=devid;
  event->joy.btnid=btnid;
  event->joy.value=value;
}

void egg_cb_button(struct hostio_input *driver,int devid,int btnid,int value) {
  if (!btnid) return;
  struct egg_device *device=egg_inmgr_get_device(egg.inmgr,devid);
  if (!device) return;
  switch (device->rptcls) {
  
    case EGG_EVENT_RAW: {
        if (egg.inmgr->eventmask&(1<<EGG_EVENT_RAW)) {
          union egg_event *event=egg_event_push(egg.inmgr);
          event->raw.type=EGG_EVENT_RAW;
          event->raw.devid=devid;
          event->raw.btnid=btnid;
          event->raw.value=value;
        }
      } break;
  
    case EGG_EVENT_JOY: {
        if (egg.inmgr->eventmask&(1<<EGG_EVENT_RAW)) {
          union egg_event *event=egg_event_push(egg.inmgr);
          event->raw.type=EGG_EVENT_RAW;
          event->raw.devid=devid;
          event->raw.btnid=btnid;
          event->raw.value=value;
        }
        struct egg_button *button=egg_device_get_button(device,btnid);
        if (!button||!button->dstbtnid) return;
        switch (button->dstbtnid) {
        
          case EGG_JOYBTN_LX:
          case EGG_JOYBTN_LY:
          case EGG_JOYBTN_RX:
          case EGG_JOYBTN_RY: 
          case EGG_INMAP_BTN_NLX:
          case EGG_INMAP_BTN_NLY:
          case EGG_INMAP_BTN_NRX:
          case EGG_INMAP_BTN_NRY: {
              if (button->srclo<button->srchi) value=((value-button->srclo)*256)/(button->srchi-button->srclo+1)-128;
              else value=128-((value-button->srchi)*256)/(button->srclo-button->srchi+1);
              if (value<-128) value=-128;
              else if (value>127) value=127;
              if (value==button->dstvalue) return;
              button->dstvalue=value;
              int dstbtnid=button->dstbtnid;
              switch (button->dstbtnid) {
                case EGG_INMAP_BTN_NLX: dstbtnid=EGG_JOYBTN_LX; break;
                case EGG_INMAP_BTN_NLY: dstbtnid=EGG_JOYBTN_LY; break;
                case EGG_INMAP_BTN_NRX: dstbtnid=EGG_JOYBTN_RX; break;
                case EGG_INMAP_BTN_NRY: dstbtnid=EGG_JOYBTN_RY; break;
              }
              egg_send_joy_event(devid,button->dstbtnid,value);
            } break;
            
          case EGG_INMAP_BTN_HORZ:
          case EGG_INMAP_BTN_VERT: 
          case EGG_INMAP_BTN_NHORZ:
          case EGG_INMAP_BTN_NVERT: {
              if (button->srclo<button->srchi) value=(value<=button->srclo)?-1:(value>=button->srchi)?1:0;
              else value=(value<=button->srchi)?1:(value>=button->srclo)?-1:0;
              if (value==button->dstvalue) return;
              int btnidlo,btnidhi;
              if ((button->dstbtnid==EGG_INMAP_BTN_HORZ)||(button->dstbtnid==EGG_INMAP_BTN_NHORZ)) {
                btnidlo=EGG_JOYBTN_LEFT;
                btnidhi=EGG_JOYBTN_RIGHT;
              } else {
                btnidlo=EGG_JOYBTN_UP;
                btnidhi=EGG_JOYBTN_DOWN;
              }
                   if (button->dstvalue<0) egg_send_joy_event(devid,btnidlo,0);
              else if (button->dstvalue>0) egg_send_joy_event(devid,btnidhi,0);
              button->dstvalue=value;
                   if (value<0) egg_send_joy_event(devid,btnidlo,1);
              else if (value>0) egg_send_joy_event(devid,btnidhi,1);
            } break;
            
          case EGG_INMAP_BTN_DPAD: {
              value-=button->lo;
              int pdx=0,pdy=0,ndx=0,ndy=0;
              switch (button->dstvalue) {
                case 7: case 6: case 5: pdx=-1; break;
                case 1: case 2: case 3: pdx=1; break;
              }
              switch (button->dstvalue) {
                case 7: case 0: case 1: pdy=-1; break;
                case 5: case 4: case 3: pdy=1; break;
              }
              switch (value) {
                case 7: case 6: case 5: ndx=-1; break;
                case 1: case 2: case 3: ndx=1; break;
              }
              switch (value) {
                case 7: case 0: case 1: ndy=-1; break;
                case 5: case 4: case 3: ndy=1; break;
              }
              button->dstvalue=value;
              if (pdx!=ndx) {
                     if (pdx<0) egg_send_joy_event(devid,EGG_JOYBTN_LEFT,0);
                else if (pdx>0) egg_send_joy_event(devid,EGG_JOYBTN_RIGHT,0);
                     if (ndx<0) egg_send_joy_event(devid,EGG_JOYBTN_LEFT,1);
                else if (ndx>0) egg_send_joy_event(devid,EGG_JOYBTN_RIGHT,1);
              }
              if (pdy!=ndy) {
                     if (pdy<0) egg_send_joy_event(devid,EGG_JOYBTN_UP,0);
                else if (pdy>0) egg_send_joy_event(devid,EGG_JOYBTN_DOWN,0);
                     if (ndy<0) egg_send_joy_event(devid,EGG_JOYBTN_UP,1);
                else if (ndy>0) egg_send_joy_event(devid,EGG_JOYBTN_DOWN,1);
              }
            } break;
            
          default: {
              if (value&&button->dstvalue) return;
              if (!value&&!button->dstvalue) return;
              button->dstvalue=value;
              if (value&&egg_perform_user_action(button->dstbtnid)) break;
              egg_send_joy_event(devid,button->dstbtnid,value);
            }
        }
      } break;
      
    case EGG_EVENT_KEY: {
        if (!(egg.inmgr->eventmask&(1<<EGG_EVENT_KEY))) return;
        struct egg_button *button=egg_device_get_button(device,btnid);
        if (!button) return;
        if ((button->hidusage<0x00070000)||(button->hidusage>=0x00080000)) return;
        if (value==button->value) return;
        button->value=value;
        //TODO Check for configured one-shot actions.
        union egg_event *event=egg_event_push(egg.inmgr);
        event->key.type=EGG_EVENT_KEY;
        event->key.keycode=button->hidusage;
        event->key.value=value;
        // We'll fake KEY events straight off the device, but not TEXT. That would be too complicated.
      } break;
      
    case EGG_EVENT_MMOTION: {
        struct egg_button *button=egg_device_get_button(device,btnid);
        if (!button||!button->hidusage) return;
        switch (button->hidusage) {
          case 0x00010030: { // X
              if (egg.inmgr->lock_fake_cursor) {
                egg_cb_mmotion(0,value,0);
              } else {
                int nx=device->x+value;
                if (nx<0) nx=0; else if (nx>=device->w) nx=device->w-1;
                if (nx!=device->x) {
                  device->x=nx;
                  egg_cb_mmotion(0,device->x,device->y);
                }
              }
            } break;
          case 0x00010031: { // Y
              if (egg.inmgr->lock_fake_cursor) {
                egg_cb_mmotion(0,0,value);
              } else {
                int ny=device->y+value;
                if (ny<0) ny=0; else if (ny>=device->h) ny=device->h-1;
                if (ny!=device->y) {
                  device->y=ny;
                  egg_cb_mmotion(0,device->x,device->y);
                }
              }
            } break;
          case 0x00010033: { // horz wheel.
              egg_cb_mwheel(0,value,0);
            } break;
          case 0x00010038: { // vert (default) wheel
              egg_cb_mwheel(0,0,-value);
            } break;
          default: if ((button->hidusage>=0x00090000)&&(button->hidusage<0x000a0000)) {
              egg_cb_mbutton(0,button->hidusage-0x00090000+1,value);
            }
        }
      } break;

    case EGG_EVENT_TOUCH: break;//TODO?
    case EGG_EVENT_ACCEL: break;//TODO?
  }
}

/* Forcible calls required during saved state load.
 */
 
int egg_get_eventmask() {
  return egg.inmgr->eventmask;
}

void egg_force_eventmask(int mask) {
  egg.inmgr->eventmask=0;
  int etype=30;
  while (etype-->0) {
    if (!(mask&(1<<etype))) continue;
    egg_event_enable(etype,1);
  }
}

void egg_artificial_joy_disconnect(int devid) {
  if (!(egg.inmgr->eventmask&(1<<EGG_EVENT_JOY))) return;
  union egg_event *event=egg_event_push(egg.inmgr);
  event->joy.type=EGG_EVENT_JOY;
  event->joy.devid=devid;
  event->joy.btnid=0; // "connection state"
  event->joy.value=0;
}

void egg_artificial_raw_disconnect(int devid) {
  if (!(egg.inmgr->eventmask&(1<<EGG_EVENT_RAW))) return;
  union egg_event *event=egg_event_push(egg.inmgr);
  event->joy.type=EGG_EVENT_RAW;
  event->joy.devid=devid;
  event->joy.btnid=0; // "connection state"
  event->joy.value=0;
}

void egg_artificial_key_release(int keycode) {
  if (!(egg.inmgr->eventmask&(1<<EGG_EVENT_KEY))) return;
  union egg_event *event=egg_event_push(egg.inmgr);
  event->key.type=EGG_EVENT_KEY;
  event->key.keycode=keycode;
  event->key.value=0;
}

int egg_get_held_keys(int *dst,int dsta) {
  int dstc=0;
  int major=0;
  for (;major<sizeof(egg.inmgr->keyv);major++) {
    int src=egg.inmgr->keyv[major];
    if (!src) continue;
    int minor=0,mask=0x01;
    for (;minor<8;minor++,mask<<=1) {
      if (!(src&mask)) continue;
      if (dstc>=dsta) return dstc; // never report more than dsta
      dst[dstc++]=0x00070000|(major<<3)|minor;
    }
  }
  return dstc;
}

int egg_get_joy_devids(int *dst,int dsta) {
  int dstc=0;
  struct egg_device **p=egg.inmgr->devicev;
  int i=egg.inmgr->devicec;
  for (;i-->0;p++) {
    struct egg_device *device=*p;
    if (device->rptcls==EGG_EVENT_JOY) {
      if (dstc>=dsta) return dstc; // never report more than dsta
      dst[dstc++]=device->devid;
    }
  }
  return dstc;
}

int egg_get_raw_devids(int *dst,int dsta) {
  int dstc=0;
  struct egg_device **p=egg.inmgr->devicev;
  int i=egg.inmgr->devicec;
  for (;i-->0;p++) {
    struct egg_device *device=*p;
    if ((device->rptcls==EGG_EVENT_JOY)||(device->rptcls==EGG_EVENT_RAW)) {
      if (dstc>=dsta) return dstc; // never report more than dsta
      dst[dstc++]=device->devid;
    }
  }
  return dstc;
}

/* Public API.
 ***********************************************************************************************************/
 
int egg_event_get(union egg_event *v,int a) {
  if (a>egg.inmgr->eventc) a=egg.inmgr->eventc;
  int headc=EGG_EVENTQ_LIMIT-egg.inmgr->eventp;
  if (headc>a) headc=a;
  memcpy(v,egg.inmgr->eventq+egg.inmgr->eventp,sizeof(union egg_event)*headc);
  if ((egg.inmgr->eventp+=headc)>=EGG_EVENTQ_LIMIT) egg.inmgr->eventp=0;
  egg.inmgr->eventc-=headc;
  int tailc=a-headc;
  if (tailc>0) {
    memcpy(v+headc,egg.inmgr->eventq,sizeof(union egg_event)*tailc);
    egg.inmgr->eventp+=tailc;
    egg.inmgr->eventc-=tailc;
  }
  return a;
}

int egg_event_enable(int type,int enable) {
  if ((type<1)||(type>EGG_EVENT_ID_MAX)) return 0;
  int bit=1<<type;
  if (enable) {
    if (egg.inmgr->eventmask&bit) return 1;
    if (egg_event_capable(type)<0) return 0;
    egg.inmgr->eventmask|=bit;
  } else {
    if (!(egg.inmgr->eventmask&bit)) return 0;
    if (egg_event_capable(type)>0) return 1;
    egg.inmgr->eventmask&=~bit;
  }
  switch (type) {
    case EGG_EVENT_JOY: break;
    case EGG_EVENT_KEY: break;
    case EGG_EVENT_TEXT: break;
    case EGG_EVENT_MMOTION: egg_inmgr_refresh_cursor(egg.inmgr); break;
    case EGG_EVENT_MBUTTON: egg_inmgr_refresh_cursor(egg.inmgr); break;
    case EGG_EVENT_MWHEEL: egg_inmgr_refresh_cursor(egg.inmgr); break;
    case EGG_EVENT_TOUCH: break;
    case EGG_EVENT_ACCEL: break;
    case EGG_EVENT_RAW: break;
  }
  return (egg.inmgr->eventmask&bit)?1:0;
}

void egg_show_cursor(int show) {
  if (show) {
    if (egg.inmgr->cursor_desired) return;
    egg.inmgr->cursor_desired=1;
  } else {
    if (!egg.inmgr->cursor_desired) return;
    egg.inmgr->cursor_desired=0;
  }
  egg_inmgr_refresh_cursor(egg.inmgr);
}

int egg_lock_cursor(int lock) {
  if (egg.hostio->video->type->lock_cursor) {
    egg.hostio->video->type->lock_cursor(egg.hostio->video,lock);
    return egg.hostio->video->cursor_locked;
  } else {
    return egg.inmgr->lock_fake_cursor=lock;
  }
}

int egg_joystick_devid_by_index(int p) {
  if (p<0) return 0;
  int i=0; for (;i<egg.inmgr->devicec;i++) {
    struct egg_device *device=egg.inmgr->devicev[i];
    if ((device->rptcls==EGG_EVENT_RAW)||(device->rptcls==EGG_EVENT_JOY)) {
      if (!p--) return device->devid;
    }
  }
  return 0;
}

void egg_joystick_get_ids(int *vid,int *pid,int *version,int devid) {
  struct egg_device *device=egg_inmgr_get_device(egg.inmgr,devid);
  if (!device) return;
  if (vid) *vid=device->vid;
  if (pid) *pid=device->pid;
  if (version) *version=device->version;
}

int egg_joystick_get_name(char *dst,int dsta,int devid) {
  struct egg_device *device=egg_inmgr_get_device(egg.inmgr,devid);
  if (!device) return 0;
  if (device->namec<=dsta) memcpy(dst,device->name,device->namec);
  return device->namec;
}

int egg_joystick_for_each_button(int devid,int (*cb)(int btnid,int usage,int lo,int hi,int value,void *userdata),void *userdata) {
  struct egg_device *device=egg_inmgr_get_device(egg.inmgr,devid);
  if (!device) return 0;
  const struct egg_button *button=device->buttonv;
  int i=device->buttonc;
  switch (device->rptcls) {
    case EGG_EVENT_JOY: {
        for (;i-->0;button++) {
          if (!button->dstbtnid) continue;
          int err=cb(button->dstbtnid,button->hidusage,button->lo,button->hi,button->value,userdata);
          if (err) return err;
        }
      } break;
    case EGG_EVENT_KEY: {
        for (;i-->0;button++) {
          int err=cb(button->hidusage,button->hidusage,button->lo,button->hi,button->value,userdata);
          if (err) return err;
        }
      } break;
    default: {
        for (;i-->0;button++) {
          int err=cb(button->btnid,button->hidusage,button->lo,button->hi,button->value,userdata);
          if (err) return err;
        }
      }
  }
  return 0;
}
