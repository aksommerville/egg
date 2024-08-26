/* hostio_audio_dummy.c
 * Skeletal audio driver, and serves as an example template.
 */
 
#include "hostio_internal.h"
#include <sys/time.h>

// This can be whatever (at least the maximum channel count).
// Ideally large enough to hold one update cycle's worth of audio for all channels.
#define DUMMY_BUF_LIMIT_BYTES 1024

// Clamp update lengths at this duration in seconds (ie assume something is broken).
// Must be substantially longer than an expected update cycle.
#define DUMMY_MAX_UPDATE_S 0.100

// If we measure an update shorter than this, noop and wait for the next cycle.
#define DUMMY_MIN_UPDATE_S 0.001

/* Object definition.
 */
 
struct hostio_audio_dummy {
  struct hostio_audio hdr;
  double prevtime;
  int16_t buf[DUMMY_BUF_LIMIT_BYTES];
  int bufa_frames;
  int bufa_samples;
};

#define DRIVER ((struct hostio_audio_dummy*)driver)

/* Current real time in seconds.
 */
 
static double dummy_now() {
  struct timeval tv={0};
  gettimeofday(&tv,0);
  return (double)tv.tv_sec+(double)tv.tv_usec/1000000.0;
}

/* Delete.
 */
 
static void _dummy_del(struct hostio_audio *driver) {
  // Shut down I/O thread if you have one.
}

/* Init.
 */
 
static int _dummy_init(struct hostio_audio *driver,const struct hostio_audio_setup *setup) {
  
  // Respect setup if applicable:
  // device, buffer_size
  
  // (rate,chanc) can be anything.
  // Try to match (setup) if it's reasonable, but try even harder not to fail.
  driver->rate=setup->rate;
  driver->chanc=setup->chanc;
  if ((driver->rate<200)||(driver->rate>200000)) driver->rate=44100;
  if ((driver->chanc<1)||(driver->chanc>8)) driver->chanc=1;
  
  DRIVER->bufa_frames=DUMMY_BUF_LIMIT_BYTES/driver->chanc;
  DRIVER->bufa_samples=DRIVER->bufa_frames*driver->chanc;
  
  // Spin up I/O thread if you do that. Ensure it does not call the delegate until we get play(1).
  
  return 0;
}

/* Start/stop.
 */
 
static void _dummy_play(struct hostio_audio *driver,int play) {
  if (play) {
    driver->playing=1;
  } else {
    // If multithreaded, block until callback is not running. Typically like so:
    // if (_dummy_lock(driver)<0) return;
    driver->playing=0;
    // _dummy_unlock(driver);
  }
}

/* Update.
 * Most drivers are not expected to implement this.
 * Better to spin up an I/O thread, and provide "lock" and "unlock" hooks instead of "update".
 * You can implement update in either case, eg for reporting errors.
 */

static int _dummy_update(struct hostio_audio *driver) {
  if (!driver->playing) return 0;
  
  /* Determine elapsed time since previous update.
   * If we want to run under headless automation,
   * it would be reasonable to set some fixed elapsed time here instead of checking the clock.
   * We don't sample the clock at init, instead it defers here to the first update.
   * (that's intentional; it prevents a potentially long update the first time, while the app is still initializing).
   */
  double now=dummy_now();
  if (DRIVER->prevtime<=0.0) {
    DRIVER->prevtime=now;
    return 0;
  }
  double elapsed=now-DRIVER->prevtime;
  if (elapsed<=0.0) {
    // Clock is broken. Reset, and do nothing this time.
    DRIVER->prevtime=now;
    return 0;
  }
  if (elapsed<DUMMY_MIN_UPDATE_S) {
    // We're running hot. Preserve (prevtime) and do nothing, wait for the next update.
    return 0;
  }
  if (elapsed>DUMMY_MAX_UPDATE_S) {
    // Too much elapsed time. We're running slow, or maybe the system clock changed.
    // Lie about the elapsed time.
    elapsed=DUMMY_MAX_UPDATE_S;
  }
  DRIVER->prevtime=now;
  
  /* Calculate sample count, and generate so many samples.
   * If we really wanted to get precise about timing, check the round-off from (framec),
   * and subtract that much from (prevtime).
   * That's under one frame per update, probably doesn't matter even if it matters.
   */
  if (driver->delegate.cb_pcm_out) {
    int framec=(int)(elapsed*driver->rate);
    if (framec<1) framec=1;
    int samplec=framec*driver->chanc;
    while (samplec>0) {
      int genc=samplec;
      if (genc>DRIVER->bufa_samples) genc=DRIVER->bufa_samples;
      driver->delegate.cb_pcm_out(DRIVER->buf,genc,driver);
      // Opportunity to examine (DRIVER->buf). Check levels, dump to a file...
      samplec-=genc;
    }
  }
  
  return 0;
}

/* Type definition.
 */
 
const struct hostio_audio_type hostio_audio_type_dummy={
  .name="dummy",
  .desc="Fake audio driver that keeps time but discards output.",
  .objlen=sizeof(struct hostio_audio_dummy),
  .appointment_only=0, // Nonzero to exclude from default list.
  .del=_dummy_del,
  .init=_dummy_init,
  .play=_dummy_play,
  .update=_dummy_update, // atypical
  
  // Typical:
  // int lock(struct hostio_audio *driver);
  // void unlock(struct hostio_audio *driver);
  // double estimate_remaining_buffer(struct hostio_audio *driver);
};
