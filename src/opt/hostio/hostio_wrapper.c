#include "hostio_internal.h"

void hostio_video_del(struct hostio_video *driver) {
  if (!driver) return;
  if (driver->type->del) driver->type->del(driver);
  free(driver);
}

struct hostio_video *hostio_video_new(
  const struct hostio_video_type *type,
  const struct hostio_video_delegate *delegate,
  const struct hostio_video_setup *setup
) {
  if (!type||!delegate||!setup) return 0;
  struct hostio_video *driver=calloc(1,type->objlen);
  if (!driver) return 0;
  driver->type=type;
  driver->delegate=*delegate;
  if (type->init&&(type->init(driver,setup)<0)) {
    hostio_video_del(driver);
    return 0;
  }
  return driver;
}

void hostio_audio_del(struct hostio_audio *driver) {
  if (!driver) return;
  if (driver->type->del) driver->type->del(driver);
  free(driver);
}

struct hostio_audio *hostio_audio_new(
  const struct hostio_audio_type *type,
  const struct hostio_audio_delegate *delegate,
  const struct hostio_audio_setup *setup
) {
  if (!type||!delegate||!setup) return 0;
  struct hostio_audio *driver=calloc(1,type->objlen);
  if (!driver) return 0;
  driver->type=type;
  driver->delegate=*delegate;
  if (type->init&&(type->init(driver,setup)<0)) {
    hostio_audio_del(driver);
    return 0;
  }
  return driver;
}

void hostio_input_del(struct hostio_input *driver) {
  if (!driver) return;
  if (driver->type->del) driver->type->del(driver);
  free(driver);
}

struct hostio_input *hostio_input_new(
  const struct hostio_input_type *type,
  const struct hostio_input_delegate *delegate,
  const struct hostio_input_setup *setup
) {
  if (!type||!delegate||!setup) return 0;
  struct hostio_input *driver=calloc(1,type->objlen);
  if (!driver) return 0;
  driver->type=type;
  driver->delegate=*delegate;
  if (type->init&&(type->init(driver,setup)<0)) {
    hostio_input_del(driver);
    return 0;
  }
  return driver;
}
