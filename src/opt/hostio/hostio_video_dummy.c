#include "hostio_internal.h"

/* Instance definition.
 */
 
struct hostio_video_dummy {
  struct hostio_video hdr;
};

#define DRIVER ((struct hostio_video_dummy*)driver)

/* Delete.
 */
 
static void _dummy_del(struct hostio_video *driver) {
}

/* Init.
 */
 
static int _dummy_init(struct hostio_video *driver,const struct hostio_video_setup *setup) {
  if ((setup->w>0)&&(setup->h>0)) {
    driver->w=setup->w;
    driver->h=setup->h;
  } else {
    driver->w=640;
    driver->h=360;
  }
  driver->fullscreen=1;
  driver->rate=60;
  if ((setup->ratemin>0)&&(driver->rate<setup->ratemin)) driver->rate=setup->ratemin;
  if ((setup->ratemax>0)&&(driver->rate>setup->ratemax)) driver->rate=setup->ratemax;
  return 0;
}

/* Type definition.
 */
 
const struct hostio_video_type hostio_video_type_dummy={
  .name="dummy",
  .desc="Fake video driver that discards output.",
  .appointment_only=1,
  .provides_keyboard=0,
  .objlen=sizeof(struct hostio_video_dummy),
  .del=_dummy_del,
  .init=_dummy_init,
};
