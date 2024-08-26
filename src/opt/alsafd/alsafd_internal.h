#ifndef ALSAFD_INTERNAL_H
#define ALSAFD_INTERNAL_H

#include "alsafd.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <limits.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <pthread.h>

// asound.h uses SNDRV_LITTLE_ENDIAN/SNDRV_BIG_ENDIAN but never defines them.
#include <endian.h>
#if BYTE_ORDER==LITTLE_ENDIAN
  #define SNDRV_LITTLE_ENDIAN 1
#elif BYTE_ORDER==BIG_ENDIAN
  #define SNDRV_BIG_ENDIAN 1
#endif
#include <sound/asound.h>

struct alsafd {
  int fd;
  struct alsafd_delegate delegate;
  int rate;
  int chanc;
  int running;
  int hwbufframec;
  char *device;
  int protocol_version;
  pthread_t iothd;
  pthread_mutex_t iomtx;
  int ioerror;
  int16_t *buf;
  int bufa; // samples
  int64_t buffer_time_us;
  double buftime_s;
};

// Log if enabled, and always returns -1. Null (fmt) to use errno.
int alsafd_error(struct alsafd *alsafd,const char *context,const char *fmt,...);

void alsafd_hw_params_any(struct snd_pcm_hw_params *params);
void alsafd_hw_params_none(struct snd_pcm_hw_params *params);
int alsafd_hw_params_get_mask(const struct snd_pcm_hw_params *params,int k,int bit);
int alsafd_hw_params_get_interval(int *lo,int *hi,const struct snd_pcm_hw_params *params,int k);
int alsafd_hw_params_assert_exact_interval(int *v,const struct snd_pcm_hw_params *params,int k);
void alsafd_hw_params_set_mask(struct snd_pcm_hw_params *params,int k,int bit,int v);
void alsafd_hw_params_set_interval(struct snd_pcm_hw_params *params,int k,int lo,int hi);

// Having already refined the given param, choose a single value for it as close as possible to (v).
void alsafd_hw_params_set_nearest_interval(struct snd_pcm_hw_params *params,int k,unsigned int v);

int64_t alsafd_now();

#endif
