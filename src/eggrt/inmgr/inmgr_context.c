#include "inmgr_internal.h"

/* Delete.
 */
 
void inmgr_del(struct inmgr *inmgr) {
  if (!inmgr) return;
  free(inmgr);
}

/* New.
 */
 
struct inmgr *inmgr_new() {
  struct inmgr *inmgr=calloc(1,sizeof(struct inmgr));
  if (!inmgr) return 0;
  return inmgr;
}

/* Update.
 */
 
int inmgr_update(struct inmgr *inmgr) {
  if (!inmgr) return 0;
  return 0;
}

/* Push to event queue.
 */
 
union egg_event *inmgr_evtq_push(struct inmgr *inmgr) {
  int np=inmgr->evtqp+inmgr->evtqc;
  if (np>=INMGR_EVENT_QUEUE_LIMIT) np-=INMGR_EVENT_QUEUE_LIMIT;
  if (inmgr->evtqc>=INMGR_EVENT_QUEUE_LIMIT) {
    fprintf(stderr,"%s:WARNING: Event queue limit breached. Some input events will not reach the client.\n",eggrt.exename);
    inmgr->evtqp++;
    if (inmgr->evtqp>=INMGR_EVENT_QUEUE_LIMIT) {
      inmgr->evtqp=0;
    }
  } else {
    inmgr->evtqc++;
  }
  return inmgr->evtq+np;
}

/* Pop from event queue.
 */
 
int inmgr_get_events(union egg_event *dst,int dsta,struct inmgr *inmgr) {
  int dstc=inmgr->evtqc;
  if (dstc>dsta) dstc=dsta;
  int headc=INMGR_EVENT_QUEUE_LIMIT-inmgr->evtqp;
  if (headc<dstc) {
    int tailc=dstc-headc;
    memcpy(dst,inmgr->evtq+inmgr->evtqp,sizeof(union egg_event)*headc);
    memcpy(dst+headc,inmgr->evtq,sizeof(union egg_event)*tailc);
    inmgr->evtqp=tailc;
  } else {
    memcpy(dst,inmgr->evtq+inmgr->evtqp,sizeof(union egg_event)*dstc);
    inmgr->evtqp+=dstc;
    if (inmgr->evtqp>=INMGR_EVENT_QUEUE_LIMIT) {
      inmgr->evtqp=0;
    }
  }
  inmgr->evtqc-=dstc;
  return dstc;
}

/* Eliminate events to avoid queue overflow.
 */
 
void inmgr_drop_redundant_events(struct inmgr *inmgr) {
  
  // If we're under half of the queue's available size, do nothing.
  if (inmgr->evtqc<(INMGR_EVENT_QUEUE_LIMIT>>1)) {
    return;
  }
  
  // Zero our origin, for sanity's sake.
  if (inmgr->evtqp) {
    int tailp=inmgr->evtqp+inmgr->evtqc;
    if (tailp>INMGR_EVENT_QUEUE_LIMIT) {
      union egg_event *tmp=malloc(sizeof(union egg_event)*inmgr->evtqc);
      if (!tmp) return;
      int headc=INMGR_EVENT_QUEUE_LIMIT-inmgr->evtqp;
      memcpy(tmp,inmgr->evtq+inmgr->evtqp,sizeof(union egg_event)*headc);
      memcpy(tmp+headc,inmgr->evtq,sizeof(union egg_event)*(inmgr->evtqc-headc));
      memcpy(inmgr->evtq,tmp,sizeof(union egg_event)*inmgr->evtqc);
      free(tmp);
    } else {
      memmove(inmgr->evtq,inmgr->evtq+inmgr->evtqp,sizeof(union egg_event)*inmgr->evtqc);
    }
    inmgr->evtqp=0;
  }
  
  // Walk the queue and painstakingly decide whether to keep each event.
  union egg_event *event=inmgr->evtq;
  int p=0;
  while (p<inmgr->evtqc) {
    #define KEEP { event++; p++; continue; }
    #define DROP { inmgr->evtqc--; memmove(event,event+1,sizeof(union egg_event)*(inmgr->evtqc-p)); continue; }
  
    // RAW: (btnid==0) must remain. Those are connect/disconnect events. Everything else can go.
    if (event->type==EGG_EVENT_RAW) {
      if (event->raw.btnid) DROP
      KEEP
    }
    
    // GAMEPAD: Drop all. TODO We could be more sophisticated about this.
    if (event->type==EGG_EVENT_GAMEPAD) DROP
    
    // KEY: Drop pairs if we have both ON and OFF. Drop all repeats.
    if (event->type==EGG_EVENT_KEY) {
      if (event->key.value>1) DROP
      if (!event->key.value) KEEP
      int futurep=-1,i=p+1;
      for (;i<inmgr->evtqc;i++) {
        const union egg_event *other=inmgr->evtq+i;
        if (other->type!=EGG_EVENT_KEY) continue;
        if (other->key.keycode!=event->key.keycode) continue;
        if (!other->key.value) {
          futurep=i;
          break;
        }
      }
      if (futurep<0) KEEP
      inmgr->evtqc--;
      memmove(inmgr->evtq+futurep,inmgr->evtq+futurep+1,sizeof(union egg_event)*(inmgr->evtqc-futurep));
      DROP
    }
    
    // TEXT: Drop all.
    if (event->type==EGG_EVENT_TEXT) DROP
    
    // MMOTION,MBUTTON,MWHEEL: Drop all. Arguably we should keep the last MMOTION, but whatever.
    if (event->type==EGG_EVENT_MMOTION) DROP
    if (event->type==EGG_EVENT_MBUTTON) DROP
    if (event->type==EGG_EVENT_MWHEEL) DROP
    
    // TOUCH: Tricky. Let's say, keep the Off events but drop everything else.
    // We don't want the game to get a touch stuck on.
    if (event->type==EGG_EVENT_TOUCH) {
      if (event->touch.state==0) KEEP
      DROP
    }
    
    // ACCEL: Drop all. Yikes, we really shouldn't even have these enabled if we're hard-paused.
    if (event->type==EGG_EVENT_ACCEL) DROP
    
    // IMAGE: Keep all.
    if (event->type==EGG_EVENT_IMAGE) KEEP
    
    // Unknown event: Keep.
    KEEP
    #undef DROP
    #undef KEEP
  }
}

/* Enable event.
 */
 
int inmgr_enable_event(struct inmgr *inmgr,int evtid) {
  switch (evtid) {
  
    // In general, events can be enabled at will.
    case EGG_EVENT_RAW:
    case EGG_EVENT_GAMEPAD:
    case EGG_EVENT_IMAGE:
      return 1;
      
    // Keyboard and mouse events should only enable if we determine they are present.
    // For now, we allow always.
    case EGG_EVENT_KEY:
    case EGG_EVENT_TEXT:
    case EGG_EVENT_MMOTION:
    case EGG_EVENT_MBUTTON:
    case EGG_EVENT_MWHEEL:
      return 1;
    
    // TOUCH and ACCEL are not supported by the native runtime. TODO?
    case EGG_EVENT_TOUCH:
    case EGG_EVENT_ACCEL:
      return 0;
  }
  return 0;
}

/* Disable event.
 */
 
int inmgr_disable_event(struct inmgr *inmgr,int evtid) {
  switch (evtid) {
  
    // Most events can be disabled at will, it's just a matter of ignoring them on the way in.
    // TODO: Turn off providers. May be relevant for high-frequency events like ACCEL.
    case EGG_EVENT_RAW:
    case EGG_EVENT_GAMEPAD:
    case EGG_EVENT_KEY:
    case EGG_EVENT_TEXT:
    case EGG_EVENT_MMOTION:
    case EGG_EVENT_MBUTTON:
    case EGG_EVENT_MWHEEL:
    case EGG_EVENT_TOUCH:
    case EGG_EVENT_ACCEL:
      return 1;
    
    // You're not allowed to disable system notifications.
    case EGG_EVENT_IMAGE:
      return 0;
  }
  return 0;
}
