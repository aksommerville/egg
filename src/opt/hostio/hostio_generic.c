#include "hostio_internal.h"
#include <limits.h>

/* Registry of types.
 */
 
extern const struct hostio_video_type hostio_video_type_xegl;
extern const struct hostio_video_type hostio_video_type_glx;
extern const struct hostio_video_type hostio_video_type_drmgx;
extern const struct hostio_video_type hostio_video_type_bcm;
extern const struct hostio_video_type hostio_video_type_x11fb;
extern const struct hostio_video_type hostio_video_type_drmfb;
extern const struct hostio_video_type hostio_video_type_macwm;
extern const struct hostio_video_type hostio_video_type_mswm;

extern const struct hostio_audio_type hostio_audio_type_dummy;
extern const struct hostio_audio_type hostio_audio_type_alsafd;
extern const struct hostio_audio_type hostio_audio_type_asound;
extern const struct hostio_audio_type hostio_audio_type_pulse;
extern const struct hostio_audio_type hostio_audio_type_macaudio;
extern const struct hostio_audio_type hostio_audio_type_msaudio;

extern const struct hostio_input_type hostio_input_type_evdev;
extern const struct hostio_input_type hostio_input_type_machid;
extern const struct hostio_input_type hostio_input_type_mshid;

static const struct hostio_video_type *hostio_video_typev[]={
#if USE_xegl
  &hostio_video_type_xegl,
#endif
#if USE_glx
  &hostio_video_type_glx,
#endif
#if USE_drmgx
  &hostio_video_type_drmgx,
#endif
#if USE_bcm
  &hostio_video_type_bcm,
#endif
#if USE_x11fb
  &hostio_video_type_x11fb,
#endif
#if USE_drmfb
  &hostio_video_type_drmfb,
#endif
#if USE_macwm
  &hostio_video_type_macwm,
#endif
#if USE_mswin
  &hostio_video_type_mswm,
#endif
};

static const struct hostio_audio_type *hostio_audio_typev[]={
#if USE_alsafd
  &hostio_audio_type_alsafd,
#endif
#if USE_asound
  &hostio_audio_type_asound,
#endif
#if USE_pulse
  &hostio_audio_type_pulse,
#endif
#if USE_macaudio
  &hostio_audio_type_macaudio,
#endif
#if USE_mswin
  &hostio_audio_type_msaudio,
#endif
  &hostio_audio_type_dummy,
};

static const struct hostio_input_type *hostio_input_typev[]={
#if USE_evdev
  &hostio_input_type_evdev,
#endif
#if USE_machid
  &hostio_input_type_machid,
#endif
#if USE_mswin
  &hostio_input_type_mshid,
#endif
};

/* Type accessors, ctors, dtors, identical for the three types.
 */
 
#define DRIVERTYPE(tag) \
  const struct hostio_##tag##_type *hostio_##tag##_type_by_index(int p) { \
    if (p<0) return 0; \
    int c=sizeof(hostio_##tag##_typev)/sizeof(void*); \
    if (p>=c) return 0; \
    return hostio_##tag##_typev[p]; \
  } \
  const struct hostio_##tag##_type *hostio_##tag##_type_by_name(const char *name,int namec) { \
    if (!name) return 0; \
    if (namec<0) { namec=0; while (name[namec]) namec++; } \
    const struct hostio_##tag##_type **type=hostio_##tag##_typev; \
    int i=sizeof(hostio_##tag##_typev)/sizeof(void*); \
    for (;i-->0;type++) { \
      if (memcmp((*type)->name,name,namec)) continue; \
      if ((*type)->name[namec]) continue; \
      return *type; \
    } \
    return 0; \
  } \
  void hostio_##tag##_del(struct hostio_##tag *driver) { \
    if (!driver) return; \
    if (driver->type->del) driver->type->del(driver); \
    free(driver); \
  } \
  struct hostio_##tag *hostio_##tag##_new( \
    const struct hostio_##tag##_type *type, \
    const struct hostio_##tag##_delegate *delegate, \
    const struct hostio_##tag##_setup *setup \
  ) { \
    if (!type) return 0; \
    struct hostio_##tag *driver=calloc(1,type->objlen); \
    if (!driver) return 0; \
    driver->type=type; \
    if (delegate) driver->delegate=*delegate; \
    if (type->init&&(type->init(driver,setup)<0)) { \
      hostio_##tag##_del(driver); \
      return 0; \
    } \
    return driver; \
  }
  
DRIVERTYPE(video)
DRIVERTYPE(audio)
DRIVERTYPE(input)

#undef DRIVERTYPE

/* Extra device ID registry for input.
 */
 
static int hostio_devid=1;
 
int hostio_input_devid_next() {
  if (hostio_devid>=INT_MAX) return 0;
  return hostio_devid++;
}
