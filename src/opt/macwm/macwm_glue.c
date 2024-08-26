#include "macwm.h"
#include "opt/hostio/hostio_video.h"
#include <stdio.h>

float NSScreen_backingScale();

/* Instance definition.
 */

struct hostio_video_macwm {
  struct hostio_video hdr;
  struct macwm *macwm;
  struct bigpc_image *fb; // soft render only
  int pixfmt;
};

#define DRIVER ((struct hostio_video_macwm*)driver)

/* Cleanup.
 */

static void _macwm_del(struct hostio_video *driver) {
  macwm_del(DRIVER->macwm);
}

/* Callback wrappers.
 */

static void _macwm_cb_resize(void *userdata,int w,int h) {
  struct hostio_video *driver=userdata;
  driver->w=w;
  driver->h=h;
  if (driver->delegate.cb_resize) driver->delegate.cb_resize(driver,w,h);
}

/* Init.
 */

static int _macwm_init(struct hostio_video *driver,const struct hostio_video_setup *config) {

  // Delegate hooks are compatible between macwm and bigpc (only there's no "focus" in macwm).
  // We have to intercept resize, in order to retain its value.
  struct macwm_delegate delegate={
    .userdata=driver,
    .close=(void*)driver->delegate.cb_close,
    .resize=_macwm_cb_resize,
    .key=(void*)driver->delegate.cb_key,
    .text=(void*)driver->delegate.cb_text,
    .mbutton=(void*)driver->delegate.cb_mbutton,
    .mmotion=(void*)driver->delegate.cb_mmotion,
    .mwheel=(void*)driver->delegate.cb_mwheel,
  };
  struct macwm_setup setup={
    .w=config->w,
    .h=config->w,
    .fullscreen=config->fullscreen,
    .title=config->title,
    .rendermode=MACWM_RENDERMODE_OPENGL,
  };

  if (!(DRIVER->macwm=macwm_new(&delegate,&setup))) return -1;

  macwm_get_size(&driver->w,&driver->h,DRIVER->macwm);
  driver->fullscreen=macwm_get_fullscreen(DRIVER->macwm);
  
  //driver->viewscale=NSScreen_backingScale();
  
  return 0;
}

/* Update.
 */

static int _macwm_update(struct hostio_video *driver) {
  return macwm_update(DRIVER->macwm);
}

/* Render framing.
 */

static int _macwm_begin_frame(struct hostio_video *driver) {
  return macwm_render_begin(DRIVER->macwm);
}

static int _macwm_end_frame(struct hostio_video *driver) {
  macwm_render_end(DRIVER->macwm);
  return 0;
}

/* Fullscreen.
 */

static void _macwm_set_fullscreen(struct hostio_video *driver,int fullscreen) {
  macwm_set_fullscreen(DRIVER->macwm,fullscreen);
  driver->fullscreen=macwm_get_fullscreen(DRIVER->macwm);
}

/* Type definition.
 */

const struct hostio_video_type hostio_video_type_macwm={
  .name="macwm",
  .desc="Window Manager interface for MacOS 10+",
  .objlen=sizeof(struct hostio_video_macwm),
  .provides_input=1,
  .del=_macwm_del,
  .init=_macwm_init,
  .update=_macwm_update,
  .gx_begin=_macwm_begin_frame,
  .gx_end=_macwm_end_frame,
  .set_fullscreen=_macwm_set_fullscreen,
};

/* Private, extra support for renderer.
 */
 
struct macwm *hostio_video_get_macwm(struct hostio_video *driver) {
  if (!driver||(driver->type!=&hostio_video_type_macwm)) return 0;
  return DRIVER->macwm;
}
