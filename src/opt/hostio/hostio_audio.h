/* hostio_audio.h
 */
 
#ifndef HOSTIO_AUDIO_H
#define HOSTIO_AUDIO_H

#include <stdint.h>

struct hostio_audio;
struct hostio_audio_type;
struct hostio_audio_delegate;
struct hostio_audio_setup;

struct hostio_audio_delegate {
  void *userdata;
  void (*cb_pcm_out)(int16_t *v,int c,struct hostio_audio *driver);
};

struct hostio_audio {
  const struct hostio_audio_type *type;
  struct hostio_audio_delegate delegate;
  int rate,chanc;
  int playing;
};

struct hostio_audio_setup {
  int rate;
  int chanc;
  const char *device;
  int buffer_size;
};

struct hostio_audio_type {
  const char *name;
  const char *desc;
  int objlen;
  int appointment_only;
  void (*del)(struct hostio_audio *driver);
  int (*init)(struct hostio_audio *driver,const struct hostio_audio_setup *setup);
  void (*play)(struct hostio_audio *driver,int play);
  int (*update)(struct hostio_audio *driver);
  int (*lock)(struct hostio_audio *driver);
  void (*unlock)(struct hostio_audio *driver);
  double (*estimate_remaining_buffer)(struct hostio_audio *driver);
};

void hostio_audio_del(struct hostio_audio *driver);

struct hostio_audio *hostio_audio_new(
  const struct hostio_audio_type *type,
  const struct hostio_audio_delegate *delegate,
  const struct hostio_audio_setup *setup
);

const struct hostio_audio_type *hostio_audio_type_by_index(int p);
const struct hostio_audio_type *hostio_audio_type_by_name(const char *name,int namec);

/* Returns a guess as to how much buffer has not reached the hardware yet, in seconds.
 * If you're tracking time based on callbacks, subtract this.
 */
static inline double hostio_audio_estimate_remaining_buffer(struct hostio_audio *driver) {
  if (!driver||!driver->type->estimate_remaining_buffer) return 0.0;
  return driver->type->estimate_remaining_buffer(driver);
}

#endif
