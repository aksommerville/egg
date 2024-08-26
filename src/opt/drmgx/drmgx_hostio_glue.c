#include "drmgx_internal.h"
#include "opt/hostio/hostio_video.h"
#include <stdio.h>

struct hostio_video_drmgx {
  struct hostio_video hdr;
};

#define DRIVER ((struct hostio_video_drmgx*)driver)

static void _drmgx_del(struct hostio_video *driver) {
  drmgx_quit();
}

static int _drmgx_init(struct hostio_video *driver,const struct hostio_video_setup *config) {

  if (drmgx_init(config->device)<0) return -1;
  
  driver->w=drmgx.w;
  driver->h=drmgx.h;

  return 0;
}

static int _drmgx_begin(struct hostio_video *driver) {
  return 0;
}

static int _drmgx_end(struct hostio_video *driver) {
  return drmgx_swap();
}

const struct hostio_video_type hostio_video_type_drmgx={
  .name="drmgx",
  .desc="Linux Direct Rendering Manager plus OpenGL, for systems without an X server.",
  .objlen=sizeof(struct hostio_video_drmgx),
  .appointment_only=0,
  .provides_input=0,
  .del=_drmgx_del,
  .init=_drmgx_init,
  .gx_begin=_drmgx_begin,
  .gx_end=_drmgx_end,
};
