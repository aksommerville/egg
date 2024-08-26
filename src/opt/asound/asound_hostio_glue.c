#include "asound_internal.h"
#include "opt/hostio/hostio_audio.h"

/* Instance definition.
 */
 
struct hostio_audio_asound {
  struct hostio_audio hdr;
  struct asound *asound;
};

#define DRIVER ((struct hostio_audio_asound*)driver)

/* Delete.
 */
 
static void _asound_del(struct hostio_audio *driver) {
  asound_del(DRIVER->asound);
}

/* Init.
 */
 
static int _asound_init(struct hostio_audio *driver,const struct hostio_audio_setup *setup) {
  struct asound_delegate delegate={
    .userdata=driver,
    .cb_pcm_out=(void*)driver->delegate.cb_pcm_out,
  };
  struct asound_setup asetup={
    .rate=setup->rate,
    .chanc=setup->chanc,
    .device=setup->device,
    .buffer_size=setup->buffer_size,
  };
  if (!(DRIVER->asound=asound_new(&delegate,&asetup))) return -1;
  driver->rate=DRIVER->asound->rate;
  driver->chanc=DRIVER->asound->chanc;
  return 0;
}

/* Trivial pass-thru.
 */
 
static void _asound_play(struct hostio_audio *driver,int play) {
  asound_play(DRIVER->asound,play);
  driver->playing=DRIVER->asound->playing;
}

static int _asound_lock(struct hostio_audio *driver) {
  return asound_lock(DRIVER->asound);
}

static void _asound_unlock(struct hostio_audio *driver) {
  asound_unlock(DRIVER->asound);
}

static double _asound_estimate_remaining_buffer(struct hostio_audio *driver) {
  return asound_estimate_remaining_buffer(DRIVER->asound);
}

/* Type definition.
 */
 
const struct hostio_audio_type hostio_audio_type_asound={
  .name="asound",
  .desc="Audio via ALSA using libasound.",
  .objlen=sizeof(struct hostio_audio_asound),
  .appointment_only=0,
  .del=_asound_del,
  .init=_asound_init,
  .play=_asound_play,
  .lock=_asound_lock,
  .unlock=_asound_unlock,
  .estimate_remaining_buffer=_asound_estimate_remaining_buffer,
};
