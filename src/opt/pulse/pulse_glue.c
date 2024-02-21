#include "pulse_internal.h"
#include "opt/hostio/hostio_audio.h"

/* Instance definition.
 */
 
struct hostio_audio_pulse {
  struct hostio_audio hdr;
  struct pulse *pulse;
};

#define DRIVER ((struct hostio_audio_pulse*)driver)

/* Hooks.
 */
 
static void _pulse_del(struct hostio_audio *driver) {
  pulse_del(DRIVER->pulse);
}

static int _pulse_init(
  struct hostio_audio *driver,
  const struct hostio_audio_setup *setup
) {
  //TODO Can we get appname from the caller?
  if (setup) {
    driver->rate=setup->rate;
    driver->chanc=setup->chanc;
  }
  if (!driver->rate) driver->rate=44100;
  if (!driver->chanc) driver->chanc=2;
  if (!(DRIVER->pulse=pulse_new(driver->rate,driver->chanc,driver->delegate.cb_pcm_out,driver->delegate.userdata,""))) return -1;
  driver->rate=pulse_get_rate(DRIVER->pulse);
  driver->chanc=pulse_get_chanc(DRIVER->pulse);
  return 0;
}

static void _pulse_play(struct hostio_audio *driver,int play) {
  pulse_play(DRIVER->pulse,play);
  driver->playing=play?1:0;
}

static int _pulse_lock(struct hostio_audio *driver) {
  return pulse_lock(DRIVER->pulse);
}

static void _pulse_unlock(struct hostio_audio *driver) {
  pulse_unlock(DRIVER->pulse);
}

/* Type definition.
 */
 
const struct hostio_audio_type hostio_audio_type_pulse={
  .name="pulse",
  .desc="Audio for Linux via PulseAudio, preferred for desktop systems.",
  .objlen=sizeof(struct hostio_audio_pulse),
  .del=_pulse_del,
  .init=_pulse_init,
  .play=_pulse_play,
  .lock=_pulse_lock,
  .unlock=_pulse_unlock,
};
