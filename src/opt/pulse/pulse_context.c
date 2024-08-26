#include "pulse_internal.h"

/* I/O thread.
 */
 
static void *pulse_iothd(void *arg) {
  struct pulse *pulse=arg;
  while (1) {
    pthread_testcancel();
    
    if (pthread_mutex_lock(&pulse->iomtx)) {
      usleep(1000);
      continue;
    }
    if (pulse->running) {
      pulse->delegate.pcm_out(pulse->buf,pulse->bufa,pulse->delegate.userdata);
    } else {
      memset(pulse->buf,0,pulse->bufa<<1);
    }
    pthread_mutex_unlock(&pulse->iomtx);
    pulse->buffer_time_us=pulse_now();
    
    int err=0,result;
    pthread_testcancel();
    int pvcancel;
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE,&pvcancel);
    result=pa_simple_write(pulse->pa,pulse->buf,sizeof(int16_t)*pulse->bufa,&err);
    pthread_setcancelstate(pvcancel,0);
    if (result<0) {
      pulse->ioerror=-1;
      return 0;
    }
  }
}

/* Delete.
 */

void pulse_del(struct pulse *pulse) {
  if (!pulse) return;
  
  if (pulse->iothd) {
    pthread_cancel(pulse->iothd);
    pthread_join(pulse->iothd,0);
  }
  
  if (pulse->pa) pa_simple_free(pulse->pa);
  
  if (pulse->buf) free(pulse->buf);
  
  free(pulse);
}

/* Init PulseAudio client.
 */
 
static int pulse_init_pa(struct pulse *pulse,const struct pulse_setup *setup) {
  int err;
  
  const char *appname="Pulse Client";
  const char *servername=0;
  int buffersize=0;
  if (setup) {
    if (setup->rate>0) pulse->rate=setup->rate;
    if (setup->chanc>0) pulse->chanc=setup->chanc;
    if (setup->buffersize>0) buffersize=setup->buffersize;
    if (setup->appname) appname=setup->appname;
    if (setup->servername) servername=setup->servername;
  }
  if (pulse->rate<1) pulse->rate=44100;
  if (pulse->chanc<1) pulse->chanc=2;
  if (buffersize<1) buffersize=pulse->rate/20;
  if (buffersize<20) buffersize=20;

  pa_sample_spec sample_spec={
    #if BYTE_ORDER==BIG_ENDIAN
      .format=PA_SAMPLE_S16BE,
    #else
      .format=PA_SAMPLE_S16LE,
    #endif
    .rate=pulse->rate,
    .channels=pulse->chanc,
  };
  pa_buffer_attr buffer_attr={
    .maxlength=pulse->chanc*sizeof(int16_t)*buffersize,
    .tlength=pulse->chanc*sizeof(int16_t)*buffersize,
    .prebuf=0xffffffff,
    .minreq=0xffffffff,
  };
  
  if (!(pulse->pa=pa_simple_new(
    servername,
    appname,
    PA_STREAM_PLAYBACK,
    0, // sink name (?)
    appname,
    &sample_spec,
    0, // channel map
    &buffer_attr,
    &err
  ))) {
    return -1;
  }
  
  pulse->rate=sample_spec.rate;
  pulse->chanc=sample_spec.channels;
  
  return 0;
}

/* With the final rate and channel count settled, calculate a good buffer size and allocate it.
 */
 
static int pulse_init_buffer(struct pulse *pulse,const struct pulse_setup *setup) {

  const double buflen_target_s= 0.010; // about 100 Hz
  const int buflen_min=           128; // but in no case smaller than N samples
  const int buflen_max=         16384; // ...nor larger
  
  // Initial guess and clamp to the hard boundaries.
  if (setup->buffersize>0) pulse->bufa=setup->buffersize;
  else pulse->bufa=buflen_target_s*pulse->rate*pulse->chanc;
  if (pulse->bufa<buflen_min) {
    pulse->bufa=buflen_min;
  } else if (pulse->bufa>buflen_max) {
    pulse->bufa=buflen_max;
  }
  // Reduce to next multiple of channel count.
  pulse->bufa-=pulse->bufa%pulse->chanc;
  
  if (!(pulse->buf=malloc(sizeof(int16_t)*pulse->bufa))) {
    return -1;
  }
  pulse->buftime_s=(double)(pulse->bufa/pulse->chanc)/(double)pulse->rate;
  
  return 0;
}

/* Prepare mutex and thread.
 */
 
static int pulse_init_thread(struct pulse *pulse) {
  pthread_mutexattr_t mattr;
  pthread_mutexattr_init(&mattr);
  pthread_mutexattr_settype(&mattr,PTHREAD_MUTEX_RECURSIVE);
  if (pthread_mutex_init(&pulse->iomtx,&mattr)) return -1;
  pthread_mutexattr_destroy(&mattr);
  if (pthread_create(&pulse->iothd,0,pulse_iothd,pulse)) return -1;
  return 0;
}

/* New.
 */

struct pulse *pulse_new(
  const struct pulse_delegate *delegate,
  const struct pulse_setup *setup
) {
  if (!delegate||!delegate->pcm_out) return 0;
  struct pulse *pulse=calloc(1,sizeof(struct pulse));
  if (!pulse) return 0;
  
  pulse->delegate=*delegate;
  
  if (
    (pulse_init_pa(pulse,setup)<0)||
    (pulse_init_buffer(pulse,setup)<0)||
    (pulse_init_thread(pulse)<0)
  ) {
    pulse_del(pulse);
    return 0;
  }
  
  return pulse;
}

/* Trivial accessors.
 */

void *pulse_get_userdata(const struct pulse *pulse) {
  if (!pulse) return 0;
  return pulse->delegate.userdata;
}

int pulse_get_rate(const struct pulse *pulse) {
  if (!pulse) return 0;
  return pulse->rate;
}

int pulse_get_chanc(const struct pulse *pulse) {
  if (!pulse) return 0;
  return pulse->chanc;
}

int pulse_get_running(const struct pulse *pulse) {
  if (!pulse) return 0;
  return pulse->running;
}

void pulse_set_running(struct pulse *pulse,int running) {
  if (!pulse) return;
  pulse->running=running?1:0;
}

/* Update.
 */

int pulse_update(struct pulse *pulse) {
  if (!pulse) return 0;
  if (pulse->ioerror) return -1;
  return 0;
}

/* Lock.
 */
 
int pulse_lock(struct pulse *pulse) {
  if (!pulse) return -1;
  if (pthread_mutex_lock(&pulse->iomtx)) return -1;
  return 0;
}

void pulse_unlock(struct pulse *pulse) {
  if (!pulse) return;
  pthread_mutex_unlock(&pulse->iomtx);
}

/* Current time.
 */
 
int64_t pulse_now() {
  struct timeval tv={0};
  gettimeofday(&tv,0);
  return (int64_t)tv.tv_sec*1000000ll+tv.tv_usec;
}

/* Estimate remaining buffer.
 */
 
double pulse_estimate_remaining_buffer(const struct pulse *pulse) {
  int64_t now=pulse_now();
  double elapsed=(now-pulse->buffer_time_us)/1000000.0;
  if (elapsed<0.0) return 0.0;
  if (elapsed>pulse->buftime_s) return pulse->buftime_s;
  return pulse->buftime_s-elapsed;
}
