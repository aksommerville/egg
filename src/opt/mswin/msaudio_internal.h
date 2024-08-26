#ifndef MSAUDIO_INTERNAL_H
#define MSAUDIO_INTERNAL_H

#include "opt/hostio/hostio_audio.h"
#include <windows.h>
#include <mmsystem.h>
#include <stdio.h>

struct hostio_audio_msaudio {
  struct hostio_audio hdr;
  HWAVEOUT waveout;
  WAVEHDR bufv[2];
  int bufp;
  HANDLE thread;
  HANDLE thread_terminate;
  HANDLE thread_complete;
  HANDLE buffer_ready;
  HANDLE mutex;
};

#define DRIVER ((struct hostio_audio_msaudio*)driver)

#endif
