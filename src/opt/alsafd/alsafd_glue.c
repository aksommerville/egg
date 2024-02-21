/* alsafd_glue.c
 * Connects the pre-written alsafd unit to hostio.
 */
 
#include "alsafd.h"
#include "opt/hostio/hostio_audio.h"

struct hostio_audio_alsafd {
  struct hostio_audio hdr;
  struct alsafd *alsafd;
};

#define DRIVER ((struct hostio_audio_alsafd*)driver)

static void _alsafd_del(struct hostio_audio *driver) {
  alsafd_del(DRIVER->alsafd);
}

static int _alsafd_init(struct hostio_audio *driver,const struct hostio_audio_setup *setup) {
  
  // The alsafd and hostio delegates were made to match each other:
  struct alsafd_delegate delegate={
    .userdata=driver->delegate.userdata,
    .pcm_out=driver->delegate.cb_pcm_out,
  };
  struct alsafd_setup afsetup={
    .rate=setup->rate,
    .chanc=setup->chanc,
    .device=setup->device,
    .buffersize=setup->buffersize,
  };
  
  if (!(DRIVER->alsafd=alsafd_new(&delegate,&afsetup))) return -1;
  
  driver->rate=alsafd_get_rate(DRIVER->alsafd);
  driver->chanc=alsafd_get_chanc(DRIVER->alsafd);
  
  return 0;
}

static void _alsafd_play(struct hostio_audio *driver,int play) {
  alsafd_set_running(DRIVER->alsafd,play);
}

static int _alsafd_update(struct hostio_audio *driver) {
  // 'update' is for error reporting only
  return alsafd_update(DRIVER->alsafd);
}

static int _alsafd_lock(struct hostio_audio *driver) {
  return alsafd_lock(DRIVER->alsafd);
}

static void _alsafd_unlock(struct hostio_audio *driver) {
  return alsafd_unlock(DRIVER->alsafd);
}

const struct hostio_audio_type hostio_audio_type_alsafd={
  .name="alsafd",
  .desc="Audio for Linux via ALSA, direct access.",
  .objlen=sizeof(struct hostio_audio_alsafd),
  .appointment_only=0,
  .del=_alsafd_del,
  .init=_alsafd_init,
  .play=_alsafd_play,
  .update=_alsafd_update,
  .lock=_alsafd_lock,
  .unlock=_alsafd_unlock,
};
