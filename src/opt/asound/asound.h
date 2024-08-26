/* asound.h
 * Required:
 * Optional: hostio
 * Link: -lasound -lpthread
 *
 * Interface to ALSA using libasound, the official way.
 * We supply another option "alsafd" that sneaks around libasound, and I find it works better.
 */
 
#ifndef ASOUND_H
#define ASOUND_H

#include <stdint.h>

struct asound;

struct asound_delegate {
  void *userdata;
  void (*cb_pcm_out)(int16_t *v,int c,void *userdata);
};

struct asound_setup {
  int rate;
  int chanc;
  int buffer_size;
  const char *device;
};

void asound_del(struct asound *asound);
struct asound *asound_new(const struct asound_delegate *delegate,const struct asound_setup *setup);

/* Rate and channel count won't change after construction.
 * But they are not necessarily what you asked for.
 * You must check before playing.
 */
int asound_get_rate(const struct asound *asound);
int asound_get_chanc(const struct asound *asound);
int asound_get_playing(const struct asound *asound);

void asound_play(struct asound *asound,int play);
int asound_lock(struct asound *asound);
void asound_unlock(struct asound *asound);

double asound_estimate_remaining_buffer(const struct asound *asound);

#endif
