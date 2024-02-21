#include "hostio_internal.h"
#include <time.h>
#include <sys/time.h>

/* Current real time in seconds.
 */
 
static double dummy_current_time() {
  #if USE_mswin
    struct timeval tv={0};
    gettimeofday(&tv,0);
    return (double)tv.tv_sec+(double)tv.tv_usec/1000000.0;
  #else
    struct timespec tv={0};
    clock_gettime(CLOCK_REALTIME,&tv);
    return (double)tv.tv_sec+(double)tv.tv_nsec/1000000000.0;
  #endif
}

/* Instance definition.
 */
 
#define DUMMY_BUFFER_SIZE_SAMPLES 1024
 
struct hostio_audio_dummy {
  struct hostio_audio hdr;
  double pvtime;
  int16_t buffer[DUMMY_BUFFER_SIZE_SAMPLES];
  int buffer_size_frames;
  int buffer_size_samples; // <=DUMMY_BUFFER_SIZE_SAMPLES, and always a multiple of chanc
};

#define DRIVER ((struct hostio_audio_dummy*)driver)

/* Delete.
 */
 
static void _dummy_del(struct hostio_audio *driver) {
}

/* Init.
 */
 
static int _dummy_init(struct hostio_audio *driver,const struct hostio_audio_setup *setup) {
  if ((setup->rate>=200)&&(setup->rate<=200000)) driver->rate=setup->rate;
  else driver->rate=44100;
  if ((setup->chanc>=1)&&(setup->chanc<=8)) driver->chanc=setup->chanc;
  else driver->chanc=2;
  driver->playing=0;
  DRIVER->buffer_size_frames=DUMMY_BUFFER_SIZE_SAMPLES/driver->chanc;
  DRIVER->buffer_size_samples=DRIVER->buffer_size_frames*driver->chanc;
  return 0;
}

/* Play.
 */
 
static void _dummy_play(struct hostio_audio *driver,int play) {
  if (play) {
    if (driver->playing) return;
    driver->playing=1;
    DRIVER->pvtime=dummy_current_time(); // Toggling play/pause interrupts the flow of time. I don't think it matters.
  } else {
    if (!driver->playing) return;
    driver->playing=0;
    // !!! Important! If an I/O thread were in play, we must block here until we're sure the callback is not running.
  }
}

/* Generate and discard some PCM.
 */
 
static void dummy_generate_samples(int16_t *v,struct hostio_audio *driver,int c) {
  driver->delegate.cb_pcm_out(v,c,driver->delegate.userdata);
  //TODO Could check levels, dump to a WAV file, whatever. This would be the place.
}
 
static void dummy_generate_frames(struct hostio_audio *driver,int framec) {
  while (framec>=DRIVER->buffer_size_frames) {
    dummy_generate_samples(DRIVER->buffer,driver,DRIVER->buffer_size_samples);
    framec-=DRIVER->buffer_size_frames;
  }
  if (framec>0) {
    dummy_generate_samples(DRIVER->buffer,driver,framec*driver->chanc);
  }
}

/* Update.
 */
 
static int _dummy_update(struct hostio_audio *driver) {
  double now=dummy_current_time();
  if (DRIVER->pvtime>0.0) { // First update is noop, pvtime will be zero. That's by design, clock doesn't start until we're really running.
    double elapsed=now-DRIVER->pvtime;
    if (elapsed<0.100) { // If we detect a really long interval, say 1/10 sec, skip it.
      int framec=(int)(elapsed*driver->rate);
      dummy_generate_frames(driver,framec);
    }
  }
  DRIVER->pvtime=now;
  return 0;
}

/* Type definition.
 */
 
const struct hostio_audio_type hostio_audio_type_dummy={
  .name="dummy",
  .desc="Fake audio driver that keeps accurate time but discards output.",
  .appointment_only=0, // Even though we're a dummy, audio is not mission-critical. Make sure we are last in the list.
  .objlen=sizeof(struct hostio_audio_dummy),
  .del=_dummy_del,
  .init=_dummy_init,
  .play=_dummy_play,
  .update=_dummy_update,
};
