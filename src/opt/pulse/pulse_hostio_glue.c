#include "pulse_internal.h"
#include "opt/hostio/hostio_audio.h"

/* Instance definition.
 */
 
struct hostio_audio_pulse {
  struct hostio_audio hdr;
  struct pulse *pulse;
};

#define DRIVER ((struct hostio_audio_pulse*)driver)

/* Delete.
 */
 
static void _pulse_del(struct hostio_audio *driver) {
  pulse_del(DRIVER->pulse);
}

/* Init.
 */
 
static int _pulse_init(struct hostio_audio *driver,const struct hostio_audio_setup *setup) {
  struct pulse_delegate delegate={
    .userdata=driver,
    .pcm_out=(void*)driver->delegate.cb_pcm_out,
  };
  struct pulse_setup psetup={
    .rate=setup->rate,
    .chanc=setup->chanc,
    .buffersize=setup->buffer_size,
    //.appname="",
    //.servername="",
  };
  if (!(DRIVER->pulse=pulse_new(&delegate,&psetup))) return -1;
  driver->rate=DRIVER->pulse->rate;
  driver->chanc=DRIVER->pulse->chanc;
  driver->playing=DRIVER->pulse->running;
  return 0;
}

/* Trivial forwards.
 */
 
static void _pulse_play(struct hostio_audio *driver,int play) {
  pulse_set_running(DRIVER->pulse,play);
  driver->playing=DRIVER->pulse->running;
}
 
static int _pulse_lock(struct hostio_audio *driver) {
  return pulse_lock(DRIVER->pulse);
}

static void _pulse_unlock(struct hostio_audio *driver) {
  pulse_unlock(DRIVER->pulse);
}

static double _pulse_estimate_remaining_buffer(struct hostio_audio *driver) {
  return pulse_estimate_remaining_buffer(DRIVER->pulse);
}

/* Type definition.
 */
 
const struct hostio_audio_type hostio_audio_type_pulse={
  .name="pulse",
  .desc="PulseAudio for Linux. Preferred for desktop systems.",
  .objlen=sizeof(struct hostio_audio_pulse),
  .appointment_only=0,
  .del=_pulse_del,
  .init=_pulse_init,
  .play=_pulse_play,
  .lock=_pulse_lock,
  .unlock=_pulse_unlock,
  .estimate_remaining_buffer=_pulse_estimate_remaining_buffer,
};
