#include "alsafd_internal.h"
#include "opt/hostio/hostio_audio.h"

/* Instance definition.
 */
 
struct hostio_audio_alsafd {
  struct hostio_audio hdr;
  struct alsafd *alsafd;
};

#define DRIVER ((struct hostio_audio_alsafd*)driver)

/* Delete.
 */
 
static void _alsafd_del(struct hostio_audio *driver) {
  alsafd_del(DRIVER->alsafd);
}

/* Init.
 */
 
static int _alsafd_init(struct hostio_audio *driver,const struct hostio_audio_setup *setup) {
  struct alsafd_delegate delegate={
    .userdata=driver,
    .pcm_out=(void*)driver->delegate.cb_pcm_out,
  };
  struct alsafd_setup asetup={
    .rate=setup->rate,
    .chanc=setup->chanc,
    .device=setup->device,
    .buffersize=setup->buffer_size,
  };
  if (!(DRIVER->alsafd=alsafd_new(&delegate,&asetup))) return -1;
  driver->rate=DRIVER->alsafd->rate;
  driver->chanc=DRIVER->alsafd->chanc;
  return 0;
}

/* Trivial pass-thru.
 */
 
static void _alsafd_play(struct hostio_audio *driver,int play) {
  alsafd_set_running(DRIVER->alsafd,play);
  driver->playing=DRIVER->alsafd->running;
}

static int _alsafd_update(struct hostio_audio *driver) {
  return alsafd_update(DRIVER->alsafd);
}

static int _alsafd_lock(struct hostio_audio *driver) {
  return alsafd_lock(DRIVER->alsafd);
}

static void _alsafd_unlock(struct hostio_audio *driver) {
  alsafd_unlock(DRIVER->alsafd);
}

static double _alsafd_estimate_remaining_buffer(struct hostio_audio *driver) {
  return alsafd_estimate_remaining_buffer(DRIVER->alsafd);
}

/* Type definition.
 */
 
const struct hostio_audio_type hostio_audio_type_alsafd={
  .name="alsafd",
  .desc="Audio via ALSA without libasound.",
  .objlen=sizeof(struct hostio_audio_alsafd),
  .appointment_only=0,
  .del=_alsafd_del,
  .init=_alsafd_init,
  .play=_alsafd_play,
  .update=_alsafd_update,
  .lock=_alsafd_lock,
  .unlock=_alsafd_unlock,
  .estimate_remaining_buffer=_alsafd_estimate_remaining_buffer,
};
