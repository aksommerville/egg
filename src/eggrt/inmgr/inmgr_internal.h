#ifndef INMGR_INTERNAL_H
#define INMGR_INTERNAL_H

#include "eggrt/eggrt_internal.h"

#define INMGR_EVENT_QUEUE_LIMIT 256

struct inmgr {

  // Circular buffer for events not yet reported to client.
  union egg_event evtq[INMGR_EVENT_QUEUE_LIMIT];
  int evtqp,evtqc;
  
};

/* Always returns an event, content initially undefined.
 * Caller must overwrite it.
 * If we breach the queue's limit, we log a warning.
 */
union egg_event *inmgr_evtq_push(struct inmgr *inmgr);

#endif
