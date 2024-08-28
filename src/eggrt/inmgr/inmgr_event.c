#include "inmgr_internal.h"

/* System keyboard.
 */
 
int inmgr_event_key(struct inmgr *inmgr,int keycode,int value) {
  if (eggrt.evtmask&(1<<EGG_EVENT_KEY)) {
    union egg_event *event=inmgr_evtq_push(inmgr);
    event->key.type=EGG_EVENT_KEY;
    event->key.keycode=keycode;
    event->key.value=value;
  }
  return (eggrt.evtmask&(1<<EGG_EVENT_TEXT))?0:1;
}

/* Text from system keyboard.
 */
 
void inmgr_event_text(struct inmgr *inmgr,int codepoint) {
  if (eggrt.evtmask&(1<<EGG_EVENT_TEXT)) {
    union egg_event *event=inmgr_evtq_push(inmgr);
    event->text.type=EGG_EVENT_TEXT;
    event->text.codepoint=codepoint;
  }
}

/* Mouse.
 */
 
void inmgr_event_mmotion(struct inmgr *inmgr,int x,int y) {
  if (eggrt.evtmask&(1<<EGG_EVENT_MMOTION)) {
    union egg_event *event=inmgr_evtq_push(inmgr);
    event->mmotion.type=EGG_EVENT_MMOTION;
    event->mmotion.x=x;
    event->mmotion.y=y;
  }
}

void inmgr_event_mbutton(struct inmgr *inmgr,int btnid,int value) {
  if (eggrt.evtmask&(1<<EGG_EVENT_MBUTTON)) {
    union egg_event *event=inmgr_evtq_push(inmgr);
    event->mbutton.type=EGG_EVENT_MBUTTON;
    event->mbutton.btnid=btnid;
    event->mbutton.value=value;
    event->mbutton.x=eggrt.mousex;
    event->mbutton.y=eggrt.mousey;
  }
}

void inmgr_event_mwheel(struct inmgr *inmgr,int dx,int dy) {
  if (!dx&&!dy) return;
  if (eggrt.evtmask&(1<<EGG_EVENT_MWHEEL)) {
    union egg_event *event=inmgr_evtq_push(inmgr);
    event->mwheel.type=EGG_EVENT_MWHEEL;
    event->mwheel.dx=dx;
    event->mwheel.dy=dy;
    event->mwheel.x=eggrt.mousex;
    event->mwheel.y=eggrt.mousey;
  }
}

/* Connect joystick.
 */
 
void inmgr_event_connect(struct inmgr *inmgr,struct hostio_input *driver,int devid) {
  //TODO Look for gamepad, keyboard, or mouse mapping.
  if (eggrt.evtmask&(1<<EGG_EVENT_RAW)) {
    union egg_event *event=inmgr_evtq_push(inmgr);
    event->raw.type=EGG_EVENT_RAW;
    event->raw.devid=devid;
    event->raw.btnid=0;
    event->raw.value=1;
  }
}

/* Disconnect joystick.
 */
 
void inmgr_event_disconnect(struct inmgr *inmgr,struct hostio_input *driver,int devid) {
  //TODO Disconnect logically mapped devices.
  if (eggrt.evtmask&(1<<EGG_EVENT_RAW)) {
    union egg_event *event=inmgr_evtq_push(inmgr);
    event->raw.type=EGG_EVENT_RAW;
    event->raw.devid=devid;
    event->raw.btnid=0;
    event->raw.value=0;
  }
}

/* Joystick button.
 */
 
void inmgr_event_button(struct inmgr *inmgr,struct hostio_input *driver,int devid,int btnid,int value) {
  if (!btnid) return; // Must not report button zero, even if the device does. That's our special "carrier detect" button.
  if (eggrt.evtmask&(1<<EGG_EVENT_RAW)) {
    union egg_event *event=inmgr_evtq_push(inmgr);
    event->raw.type=EGG_EVENT_RAW;
    event->raw.devid=devid;
    event->raw.btnid=btnid;
    event->raw.value=value;
  }
  //TODO Mapping to logical devices.
}
