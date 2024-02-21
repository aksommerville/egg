/* hostio_registry.c
 * This is where we name all the available drivers.
 */
 
#include "hostio_internal.h"

extern const struct hostio_video_type hostio_video_type_glx;
extern const struct hostio_video_type hostio_video_type_drmgx;
extern const struct hostio_video_type hostio_video_type_bcm;
extern const struct hostio_video_type hostio_video_type_macwm;
extern const struct hostio_video_type hostio_video_type_mswm;
extern const struct hostio_video_type hostio_video_type_sdl;
extern const struct hostio_video_type hostio_video_type_dummy;

extern const struct hostio_audio_type hostio_audio_type_alsafd;
extern const struct hostio_audio_type hostio_audio_type_pulse;
extern const struct hostio_audio_type hostio_audio_type_macaudio;
extern const struct hostio_audio_type hostio_audio_type_msaudio;
extern const struct hostio_audio_type hostio_audio_type_sdl;
extern const struct hostio_audio_type hostio_audio_type_dummy;

extern const struct hostio_input_type hostio_input_type_evdev;
extern const struct hostio_input_type hostio_input_type_machid;
extern const struct hostio_input_type hostio_input_type_mshid;
extern const struct hostio_input_type hostio_input_type_sdl;

static const struct hostio_video_type *hostio_video_typev[]={
#if USE_glx
  &hostio_video_type_glx,
#endif
#if USE_drmgx
  &hostio_video_type_drmgx,
#endif
#if USE_bcm
  &hostio_video_type_bcm,
#endif
#if USE_macos
  &hostio_video_type_macwm,
#endif
#if USE_mswin
  &hostio_video_type_mswm,
#endif
#if USE_sdl
  &hostio_video_type_sdl,
#endif
  &hostio_video_type_dummy,
};

static const struct hostio_audio_type *hostio_audio_typev[]={
#if USE_alsafd
  &hostio_audio_type_alsafd,
#endif
#if USE_pulse
  &hostio_audio_type_pulse,
#endif
#if USE_macos
  &hostio_audio_type_macaudio,
#endif
#if USE_mswin
  &hostio_audio_type_msaudio,
#endif
#if USE_sdl
  &hostio_audio_type_sdl,
#endif
  &hostio_audio_type_dummy,
};

static const struct hostio_input_type *hostio_input_typev[]={
#if USE_evdev
  &hostio_input_type_evdev,
#endif
#if USE_macos
  &hostio_input_type_machid,
#endif
#if USE_mswin
  &hostio_input_type_mshid,
#endif
#if USE_sdl
  &hostio_input_type_sdl,
#endif
};

#define TYPEACCESSORS(intf) \
  const struct hostio_##intf##_type *hostio_##intf##_type_by_name(const char *name,int namec) { \
    if (!name) return 0; \
    if (namec<0) { namec=0; while (name[namec]) namec++; } \
    const struct hostio_##intf##_type **p=hostio_##intf##_typev; \
    int i=sizeof(hostio_##intf##_typev)/sizeof(void*); \
    for (;i-->0;p++) { \
      if (memcmp((*p)->name,name,namec)) continue; \
      if ((*p)->name[namec]) continue; \
      return *p; \
    } \
    return 0; \
  } \
  const struct hostio_##intf##_type *hostio_##intf##_type_by_index(int p) { \
    if (p<0) return 0; \
    int c=sizeof(hostio_##intf##_typev)/sizeof(void*); \
    if (p>=c) return 0; \
    return hostio_##intf##_typev[p]; \
  }
  
TYPEACCESSORS(video)
TYPEACCESSORS(audio)
TYPEACCESSORS(input)

#undef TYPEACCESSORS

static int hostio_input_devid=1;

int hostio_input_devid_next() {
  if (hostio_input_devid>=INT_MAX) return 0;
  return hostio_input_devid++;
}
