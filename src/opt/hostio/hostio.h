/* hostio.h
 * Hardware abstraction for games.
 * Drivers for a given video, audio, or input system should provide a little glue to squeeze to the hostio interface.
 */
 
#ifndef HOSTIO_H
#define HOSTIO_H

#include "hostio_video.h"
#include "hostio_audio.h"
#include "hostio_input.h"

struct hostio {
  struct hostio_video *video;
  struct hostio_audio *audio;
  struct hostio_input **inputv;
  int inputc,inputa;
  struct hostio_video_delegate video_delegate;
  struct hostio_audio_delegate audio_delegate;
  struct hostio_input_delegate input_delegate;
};

void hostio_del(struct hostio *hostio);

/* Provide your delegates at construction. Should be fair, they shouldn't change.
 * But if needed, you can touch them directly in (hostio) before initializing that system.
 * We could do the same thing with setups, but I think those will require configuration that might not be ready yet.
 */
struct hostio *hostio_new(
  const struct hostio_video_delegate *video_delegate,
  const struct hostio_audio_delegate *audio_delegate,
  const struct hostio_input_delegate *input_delegate
);

/* Drops any existing video driver, then attempts to re-open.
 * (names) is an optional comma-delimited list of driver names to try in order.
 * First successful driver wins.
 */
int hostio_init_video(
  struct hostio *hostio,
  const char *names,
  const struct hostio_video_setup *setup
);

/* Drops any existing audio driver, then attempts to re-open.
 * (names) is an optional comma-delimited list of driver names to try in order.
 * First successful driver wins.
 */
int hostio_init_audio(
  struct hostio *hostio,
  const char *names,
  const struct hostio_audio_setup *setup
);

/* Drops all open input drivers, then attempts to re-open multiple.
 * (names) is an optional comma-delimited list of driver names.
 * We will try to open all of them, but proceed if any fails.
 */
int hostio_init_input(
  struct hostio *hostio,
  const char *names,
  const struct hostio_input_setup *setup
);

/* A pretty common thing to want, and annoying to repeat, but nothing you couldn't do on your own.
 * Print a line to stderr for each active driver.
 */
void hostio_log_driver_names(const struct hostio *hostio);

/* Updates all drivers. May block.
 * This pumps the video driver if necessary, but does not touch its actual video ops. Just its event queue.
 */
int hostio_update(struct hostio *hostio);

/* Conveniences.
 */
int hostio_toggle_fullscreen(struct hostio *hostio); // => (0,1)=(window,fullscreen) new state.
int hostio_audio_play(struct hostio *hostio,int play); // => (0,1) new state
int hostio_audio_lock(struct hostio *hostio);
void hostio_audio_unlock(struct hostio *hostio);

#endif
