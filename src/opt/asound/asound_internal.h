#ifndef ASOUND_INTERNAL_H
#define ASOUND_INTERNAL_H

#include "asound.h"
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <sys/time.h>
#include <pthread.h>
#include <alsa/asoundlib.h>

struct asound {
  struct asound_delegate delegate;
  int rate,chanc;
  int playing;
  snd_pcm_t *alsa;
  int16_t *buf;
  int bufa;
  snd_pcm_uframes_t bufa_frames;
  pthread_t iothd;
  pthread_mutex_t iomtx;
  int ioabort;
  int64_t buffer_time_us;
  double buftime_s;
};

int64_t asound_now();

#endif
