#ifndef PULSE_INTERNAL_H
#define PULSE_INTERNAL_H

#include "pulse.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <endian.h>
#include <pthread.h>
#include <pulse/pulseaudio.h>
#include <pulse/simple.h>

/* Type definition.
 */
 
struct pulse {
  void (*cb)(int16_t *v,int c,void *userdata);
  int rate,chanc;
  void *userdata;
  
  pa_simple *pa;
  
  pthread_t iothd;
  pthread_mutex_t iomtx;
  int ioerror;
  int iocancel; // pa_simple doesn't like regular thread cancellation
  
  int16_t *buf;
  int bufa;
  int playing;
};

#endif
