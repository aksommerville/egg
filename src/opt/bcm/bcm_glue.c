/* bcm_glue.c
 * Attaches the pre-written 'bcm' unit to our 'hostio' interface.
 */

#include "bcm.h"
#include "opt/hostio/hostio_video.h"

/* Instance definition.
 */
 
struct hostio_video_bcm {
  struct hostio_video hdr;
};

#define DRIVER ((struct hostio_video_bcm*)driver)

/* Delete.
 */
 
static void _bcm_del(struct hostio_video *driver) {
  bcm_quit();
}

/* Init.
 */
 
static int _bcm_init(struct hostio_video *driver,const struct hostio_video_setup *setup) {
  if (bcm_init()<0) return -1;
  driver->w=bcm_get_width();
  driver->h=bcm_get_height();
  driver->fullscreen=1;
  return 0;
}

/* Swap frame.
 */
 
static int _bcm_gx_begin(struct hostio_video *driver) {
  return 0;
}

static int _bcm_gx_end(struct hostio_video *driver) {
  return bcm_swap();
}

/* Type definition.
 */
 
const struct hostio_video_type hostio_video_type_bcm={
  .name="bcm",
  .desc="Broadcom video for early Raspberry Pis. If 'drmgx' works, prefer it.",
  .objlen=sizeof(struct hostio_video_bcm),
  .del=_bcm_del,
  .init=_bcm_init,
  .gx_begin=_bcm_gx_begin,
  .gx_end=_bcm_gx_end,
};
