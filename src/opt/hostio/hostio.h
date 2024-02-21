/* hostio.h
 * Declares generic interfaces for audio, video, and input.
 * All useful implementations of those are in separate units.
 */
 
#ifndef HOSTIO_H
#define HOSTIO_H

#include "hostio_video.h"
#include "hostio_audio.h"
#include "hostio_input.h"

struct hostio {
  struct hostio_video *video;
  struct hostio_audio *audio;
  struct hostio_input *input;
};

void hostio_del(struct hostio *hostio);

/* Making a new hostio is cheap and more or less foolproof.
 * Drivers are not initialized at this time.
 */
struct hostio *hostio_new();

/* Initialize a driver.
 * (names) is a comma-delimited list of drivers to try. The first to succeed wins.
 * If empty, we try all available drivers in a hard-coded order of preference.
 * For each driver type, there can be just one live driver.
 */
int hostio_video_init(
  struct hostio *hostio,
  const char *names,int namesc,
  const struct hostio_video_delegate *delegate,
  const struct hostio_video_setup *setup
);
int hostio_audio_init(
  struct hostio *hostio,
  const char *names,int namesc,
  const struct hostio_audio_delegate *delegate,
  const struct hostio_audio_setup *setup
);
int hostio_input_init(
  struct hostio *hostio,
  const char *names,int namesc,
  const struct hostio_input_delegate *delegate,
  const struct hostio_input_setup *setup
);

/* Call often, ideally once per video frame.
 * Shouldn't block.
 */
int hostio_update(struct hostio *hostio);

#endif
