#ifndef MACAUDIO_INTERNAL_H
#define MACAUDIO_INTERNAL_H

#include "macaudio.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <CoreAudio/CoreAudio.h>
#include <AudioUnit/AudioUnit.h>

struct macaudio {
  int rate,chanc;
  int play;
  struct macaudio_delegate delegate;
  AudioComponent component;
  AudioComponentInstance instance;
  pthread_mutex_t cbmtx;
};

#endif
