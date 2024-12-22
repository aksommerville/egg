#include "macaudio.h"
#include "opt/hostio/hostio_audio.h"
#include <stdio.h>

/* Instance definition.
 */

struct hostio_audio_macaudio {
  struct hostio_audio hdr;
  struct macaudio *macaudio;
};

#define DRIVER ((struct hostio_audio_macaudio*)driver)

/* Cleanup.
 */

static void _macaudio_del(struct hostio_audio *driver) {
  macaudio_del(DRIVER->macaudio);
}

/* Init.
 */

static int _macaudio_init(struct hostio_audio *driver,const struct hostio_audio_setup *config) {
  struct macaudio_delegate delegate={
    .userdata=driver,
    .pcm_out=(void*)driver->delegate.cb_pcm_out,
  };
  struct macaudio_setup setup={
    .rate=config->rate,
    .chanc=config->chanc,
  };
  if (!(DRIVER->macaudio=macaudio_new(&delegate,&setup))) return -1;
  driver->rate=macaudio_get_rate(DRIVER->macaudio);
  driver->chanc=macaudio_get_chanc(DRIVER->macaudio);
  return 0;
}

/* Play, lock, unlock.
 */

static void _macaudio_play(struct hostio_audio *driver,int play) {
  macaudio_play(DRIVER->macaudio,play);
  driver->playing=play;
}

static int _macaudio_lock(struct hostio_audio *driver) {
  return macaudio_lock(DRIVER->macaudio);
}

static void _macaudio_unlock(struct hostio_audio *driver) {
  macaudio_unlock(DRIVER->macaudio);
}

/* Type definition.
 */

const struct hostio_audio_type hostio_audio_type_macaudio={
  .name="macaudio",
  .desc="Audio out via AudioUnit, for MacOS",
  .objlen=sizeof(struct hostio_audio_macaudio),
  .del=_macaudio_del,
  .init=_macaudio_init,
  .play=_macaudio_play,
  .lock=_macaudio_lock,
  .unlock=_macaudio_unlock,
  //TODO estimate_remaining_buffer
};
