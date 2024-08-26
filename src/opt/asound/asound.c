#include "asound_internal.h"

/* I/O thread.
 */
 
static void *asound_iothd(void *arg) {
  struct asound *asound=arg;
  while (1) {
    pthread_testcancel();
    
    if (pthread_mutex_lock(&asound->iomtx)) {
      usleep(1000);
      continue;
    }
    if (asound->playing&&asound->delegate.cb_pcm_out) {
      asound->delegate.cb_pcm_out(asound->buf,asound->bufa,asound->delegate.userdata);
    } else {
      memset(asound->buf,0,asound->bufa<<1);
    }
    pthread_mutex_unlock(&asound->iomtx);
    asound->buffer_time_us=asound_now();
    
    int framec=asound->bufa_frames;
    int framep=0;
    while (framep<framec) {
      pthread_testcancel();
      int pvcancel;
      pthread_setcancelstate(PTHREAD_CANCEL_DISABLE,&pvcancel);
      int err=snd_pcm_writei(asound->alsa,asound->buf+framep*asound->chanc,framec-framep);
      pthread_setcancelstate(pvcancel,0);
      if (err<=0) {
        if (snd_pcm_recover(asound->alsa,err,0)<0) return 0;
        break;
      }
      framep+=err;
    }
  }
}

/* Delete.
 */

void asound_del(struct asound *asound) {
  if (!asound) return;
  if (asound->iothd) {
    pthread_cancel(asound->iothd);
    pthread_join(asound->iothd,0);
  }
  if (asound->alsa) snd_pcm_close(asound->alsa);
  if (asound->buf) free(asound->buf);
  free(asound);
}

/* Init.
 */
 
static int asound_init(struct asound *asound,const struct asound_setup *setup) {
  
  const char *device=0;
  if (setup) {
    asound->rate=setup->rate;
    asound->chanc=setup->chanc;
    device=setup->device;
  }
       if (asound->rate<200) asound->rate=44100;
  else if (asound->rate>200000) asound->rate=44100;
       if (asound->chanc<1) asound->chanc=1;
  else if (asound->chanc>8) asound->chanc=8;
  if (!device) device="default";

  asound->bufa_frames=asound->rate/30;

  snd_pcm_hw_params_t *hwparams=0;
  if (
    (snd_pcm_open(&asound->alsa,device,SND_PCM_STREAM_PLAYBACK,0)<0)||
    (snd_pcm_hw_params_malloc(&hwparams)<0)||
    (snd_pcm_hw_params_any(asound->alsa,hwparams)<0)||
    (snd_pcm_hw_params_set_access(asound->alsa,hwparams,SND_PCM_ACCESS_RW_INTERLEAVED)<0)||
    (snd_pcm_hw_params_set_format(asound->alsa,hwparams,SND_PCM_FORMAT_S16)<0)||
    (snd_pcm_hw_params_set_rate_near(asound->alsa,hwparams,&asound->rate,0)<0)||
    (snd_pcm_hw_params_set_channels_near(asound->alsa,hwparams,&asound->chanc)<0)||
    (snd_pcm_hw_params_set_buffer_size_near(asound->alsa,hwparams,&asound->bufa_frames)<0)||
    (snd_pcm_hw_params(asound->alsa,hwparams)<0)
  ) {
    snd_pcm_hw_params_free(hwparams);
    return -1;
  }
  
  snd_pcm_hw_params_free(hwparams);
  
  if (snd_pcm_nonblock(asound->alsa,0)<0) return -1;
  if (snd_pcm_prepare(asound->alsa)<0) return -1;

  asound->bufa=asound->bufa_frames*asound->chanc;
  if (!(asound->buf=malloc(asound->bufa*2))) return -1;
  asound->buftime_s=(double)asound->bufa_frames/(double)asound->rate;

  pthread_mutexattr_t mattr;
  pthread_mutexattr_init(&mattr);
  pthread_mutexattr_settype(&mattr,PTHREAD_MUTEX_RECURSIVE);
  if (pthread_mutex_init(&asound->iomtx,&mattr)) return -1;
  pthread_mutexattr_destroy(&mattr);
  if (pthread_create(&asound->iothd,0,asound_iothd,asound)) return -1;

  return 0;
}

/* New.
 */
 
struct asound *asound_new(const struct asound_delegate *delegate,const struct asound_setup *setup) {
  struct asound *asound=calloc(1,sizeof(struct asound));
  if (!asound) return 0;
  asound->delegate=*delegate;
  if (asound_init(asound,setup)<0) {
    asound_del(asound);
    return 0;
  }
  return asound;
}

/* Accessors.
 */

int asound_get_rate(const struct asound *asound) {
  if (!asound) return 0;
  return asound->rate;
}

int asound_get_chanc(const struct asound *asound) {
  if (!asound) return 0;
  return asound->chanc;
}

int asound_get_playing(const struct asound *asound) {
  if (!asound) return 0;
  return asound->playing;
}

void asound_play(struct asound *asound,int play) {
  if (!asound) return;
  if (play) {
    if (asound->playing) return;
    asound->playing=1;
  } else {
    if (!asound->playing) return;
    if (asound_lock(asound)<0) return;
    asound->playing=0;
    asound_unlock(asound);
  }
}

int asound_lock(struct asound *asound) {
  if (!asound) return -1;
  if (pthread_mutex_lock(&asound->iomtx)) return -1;
  return 0;
}

void asound_unlock(struct asound *asound) {
  if (!asound) return;
  pthread_mutex_unlock(&asound->iomtx);
}

/* Current time.
 */
 
int64_t asound_now() {
  struct timeval tv={0};
  gettimeofday(&tv,0);
  return (int64_t)tv.tv_sec*1000000ll+tv.tv_usec;
}

/* Estimate remaining buffer.
 */
 
double asound_estimate_remaining_buffer(const struct asound *asound) {
  int64_t now=asound_now();
  double elapsed=(now-asound->buffer_time_us)/1000000.0;
  if (elapsed<0.0) return 0.0;
  if (elapsed>asound->buftime_s) return asound->buftime_s;
  return asound->buftime_s-elapsed;
}
