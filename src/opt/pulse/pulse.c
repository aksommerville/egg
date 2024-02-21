#include "pulse_internal.h"

/* Delete.
 */

void pulse_del(struct pulse *pulse) {
  if (!pulse) return;
  if (pulse->iothd) {
    pulse->iocancel=1;
    pthread_join(pulse->iothd,0);
    pulse->iothd=0;
  }
  pthread_mutex_destroy(&pulse->iomtx);
  if (pulse->pa) {
    pa_simple_free(pulse->pa);
  }
  free(pulse);
}

/* I/O thread.
 */
 
static void *pulse_iothd(void *arg) {
  struct pulse *pulse=arg;
  while (1) {
    if (pulse->iocancel) return 0;
    
    // Fill buffer while holding the lock.
    if (pulse->playing) {
      if (pthread_mutex_lock(&pulse->iomtx)) {
        pulse->ioerror=-1;
        return 0;
      }
      pulse->cb(pulse->buf,pulse->bufa,pulse->userdata);
      if (pthread_mutex_unlock(&pulse->iomtx)) {
        pulse->ioerror=-1;
        return 0;
      }
    } else {
      memset(pulse->buf,0,pulse->bufa*2);
    }
    if (pulse->iocancel) return 0;
    
    // Deliver to PulseAudio.
    int err=0,result;
    result=pa_simple_write(pulse->pa,pulse->buf,sizeof(int16_t)*pulse->bufa,&err);
    if (pulse->iocancel) return 0;
    if (result<0) {
      pulse->ioerror=-1;
      return 0;
    }
  }
}

/* Init PulseAudio client.
 */
 
static int pulse_init_pa(struct pulse *pulse,const char *appname) {
  int err;

  pa_sample_spec sample_spec={
    #if BYTE_ORDER==BIG_ENDIAN
      .format=PA_SAMPLE_S16BE,
    #else
      .format=PA_SAMPLE_S16LE,
    #endif
    .rate=pulse->rate,
    .channels=pulse->chanc,
  };
  int bufframec=pulse->rate/20; //TODO more sophisticated buffer length decision
  if (bufframec<20) bufframec=20;
  pa_buffer_attr buffer_attr={
    .maxlength=pulse->chanc*sizeof(int16_t)*bufframec,
    .tlength=pulse->chanc*sizeof(int16_t)*bufframec,
    .prebuf=0xffffffff,
    .minreq=0xffffffff,
  };
  
  if (!(pulse->pa=pa_simple_new(
    0, // server name
    appname, // our name
    PA_STREAM_PLAYBACK,
    0, // sink name (?)
    appname, // stream (as opposed to client) name
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
 
static int pulse_init_buffer(struct pulse *pulse) {

  const double buflen_target_s= 0.010; // about 100 Hz
  const int buflen_min=           128; // but in no case smaller than N samples
  const int buflen_max=         16384; // ...nor larger
  
  // Initial guess and clamp to the hard boundaries.
  pulse->bufa=buflen_target_s*pulse->rate*pulse->chanc;
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
  
  return 0;
}

/* Init thread.
 */
 
static int pulse_init_thread(struct pulse *pulse) {
  int err;
  if (err=pthread_mutex_init(&pulse->iomtx,0)) return -1;
  if (err=pthread_create(&pulse->iothd,0,pulse_iothd,pulse)) return -1;
  return 0;
}

/* New.
 */
 
struct pulse *pulse_new(
  int rate,int chanc,
  void (*cb)(int16_t *v,int c,void *userdata),
  void *userdata,
  const char *appname
) {
  struct pulse *pulse=calloc(1,sizeof(struct pulse));
  if (!pulse) return 0;
  
  pulse->rate=rate;
  pulse->chanc=chanc;
  pulse->cb=cb;
  pulse->userdata=userdata;
  
  if (
    (pulse_init_pa(pulse,appname)<0)||
    (pulse_init_buffer(pulse)<0)||
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
  return pulse?pulse->userdata:0;
}

int pulse_get_rate(const struct pulse *pulse) {
  return pulse?pulse->rate:0;
}

int pulse_get_chanc(const struct pulse *pulse) {
  return pulse?pulse->chanc:0;
}
 
void pulse_play(struct pulse *pulse,int play) {
  if (!pulse) return;
  pulse->playing=play?1:0;
}

int pulse_lock(struct pulse *pulse) {
  if (!pulse) return -1;
  if (pthread_mutex_lock(&pulse->iomtx)) return -1;
  return 0;
}

void pulse_unlock(struct pulse *pulse) {
  if (!pulse) return;
  pthread_mutex_unlock(&pulse->iomtx);
}
