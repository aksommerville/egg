/* inmgr.h
 * Generic input mapping for Egg.
 */
 
#ifndef INMGR_H
#define INMGR_H

struct inmgr;
union egg_event;

void inmgr_del(struct inmgr *inmgr);
struct inmgr *inmgr_new();
int inmgr_update(struct inmgr *inmgr);

int inmgr_get_events(union egg_event *dst,int dsta,struct inmgr *inmgr);

/* Call repeatedly when hard-paused.
 * We'll take measures to avoid overflowing the internal event queue.
 * Usually this means dropping motion events, or keystrokes if we see it was both pressed and released.
 */
void inmgr_drop_redundant_events(struct inmgr *inmgr);

/* Caller is responsible for the global event mask.
 * Don't call us redundantly.
 * Both return >0 if the change was effected, or 0 if we can't change it.
 */
int inmgr_enable_event(struct inmgr *inmgr,int evtid);
int inmgr_disable_event(struct inmgr *inmgr,int evtid);

/* Mouse coordinates should be converted to framebuffer space and dedupped before reporting to us.
 * All other events, you should pass through as hostio provides.
 */
int inmgr_event_key(struct inmgr *inmgr,int keycode,int value);
void inmgr_event_text(struct inmgr *inmgr,int codepoint);
void inmgr_event_mmotion(struct inmgr *inmgr,int x,int y);
void inmgr_event_mbutton(struct inmgr *inmgr,int btnid,int value);
void inmgr_event_mwheel(struct inmgr *inmgr,int dx,int dy);
void inmgr_event_connect(struct inmgr *inmgr,struct hostio_input *driver,int devid);
void inmgr_event_disconnect(struct inmgr *inmgr,struct hostio_input *driver,int devid);
void inmgr_event_button(struct inmgr *inmgr,struct hostio_input *driver,int devid,int btnid,int value);

#endif
