#include "alsafd_internal.h"

/* I/O thread.
 */
 
static void *alsafd_iothd(void *arg) {
  struct alsafd *alsafd=arg;
  while (1) {
    pthread_testcancel();
    
    if (pthread_mutex_lock(&alsafd->iomtx)) {
      usleep(1000);
      continue;
    }
    if (alsafd->running) {
      alsafd->delegate.pcm_out(alsafd->buf,alsafd->bufa,alsafd->delegate.userdata);
    } else {
      memset(alsafd->buf,0,alsafd->bufa<<1);
    }
    pthread_mutex_unlock(&alsafd->iomtx);
    alsafd->buffer_time_us=alsafd_now();
    
    const uint8_t *src=(uint8_t*)alsafd->buf;
    int srcc=alsafd->bufa<<1; // bytes (from samples)
    int srcp=0;
    while (srcp<srcc) {
      pthread_testcancel();
      int pvcancel;
      pthread_setcancelstate(PTHREAD_CANCEL_DISABLE,&pvcancel);
      int err=write(alsafd->fd,src+srcp,srcc-srcp);
      pthread_setcancelstate(pvcancel,0);
      if (err<0) {
        if (errno==EPIPE) {
          if (
            (ioctl(alsafd->fd,SNDRV_PCM_IOCTL_DROP)<0)||
            (ioctl(alsafd->fd,SNDRV_PCM_IOCTL_DRAIN)<0)||
            (ioctl(alsafd->fd,SNDRV_PCM_IOCTL_PREPARE)<0)
          ) {
            alsafd_error(alsafd,"write","Failed to recover from underrun: %m");
            alsafd->ioerror=-1;
            return 0;
          }
          alsafd_error(alsafd,"io","Recovered from underrun");
        } else {
          alsafd_error(alsafd,"write",0);
          alsafd->ioerror=-1;
          return 0;
        }
      } else {
        srcp+=err;
      }
    }
  }
}

/* Delete context.
 */
 
void alsafd_del(struct alsafd *alsafd) {
  if (!alsafd) return;
  
  if (alsafd->iothd) {
    pthread_cancel(alsafd->iothd);
    pthread_join(alsafd->iothd,0);
  }
  
  if (alsafd->fd>=0) close(alsafd->fd);
  if (alsafd->device) free(alsafd->device);
  if (alsafd->buf) free(alsafd->buf);
  
  free(alsafd);
}

/* Init: Populate (alsafd->device) with path to the device file.
 * Chooses a device if necessary.
 */
 
static int alsafd_select_device_path(
  struct alsafd *alsafd,
  const struct alsafd_setup *setup
) {
  if (alsafd->device) return 0;
  
  // If caller supplied a device path or basename, that trumps all.
  if (setup&&setup->device&&setup->device[0]) {
    if (setup->device[0]=='/') {
      if (!(alsafd->device=strdup(setup->device))) return -1;
      return 0;
    }
    char tmp[1024];
    int tmpc=snprintf(tmp,sizeof(tmp),"/dev/snd/%s",setup->device);
    if ((tmpc<1)||(tmpc>=sizeof(tmp))) return -1;
    if (!(alsafd->device=strdup(tmp))) return -1;
    return 0;
  }
  
  // Use some sensible default (rate,chanc) for searching, unless the caller specifies.
  int ratelo=22050,ratehi=48000;
  int chanclo=1,chanchi=2;
  if (setup) {
    if (setup->rate>0) ratelo=ratehi=setup->rate;
    if (setup->chanc>0) chanclo=chanchi=setup->chanc;
  }
  
  // Searching explicitly in "/dev/snd" forces return of an absolute path.
  // If we passed null instead, we'd get the same result, but the basename only.
  if (!(alsafd->device=alsafd_find_device("/dev/snd",ratelo,ratehi,chanclo,chanchi))) {
    // If it failed with the exact setup params, that's ok, try again with the default ranges.
    if (setup) {
      if (alsafd->device=alsafd_find_device("/dev/snd",22050,48000,1,2)) return 0;
    }
    return -1;
  }
  return 0;
}

/* Init: Open the device file. (alsafd->device) must be set.
 */
 
static int alsafd_open_device(struct alsafd *alsafd) {
  if (alsafd->fd>=0) return 0;
  if (!alsafd->device) return -1;
  
  if ((alsafd->fd=open(alsafd->device,O_WRONLY))<0) {
    return alsafd_error(alsafd,"open",0);
  }
  
  // Request ALSA version, really just confirming that the ioctl works, ie it's an ALSA PCM device.
  if (ioctl(alsafd->fd,SNDRV_PCM_IOCTL_PVERSION,&alsafd->protocol_version)<0) {
    alsafd_error(alsafd,"SNDRV_PCM_IOCTL_PVERSION",0);
    close(alsafd->fd);
    alsafd->fd=-1;
    return -1;
  }
              
  return 0;
}

/* Init: With device open, send the handshake ioctls to configure it.
 */
 
static int alsafd_configure_device(
  struct alsafd *alsafd,
  const struct alsafd_setup *setup
) {

  /* Refine hw params against the broadest set of criteria, anything we can technically handle.
   * (we impose a hard requirement for s16 interleaved; that's about it).
   */
  struct snd_pcm_hw_params hwparams;
  alsafd_hw_params_none(&hwparams);
  hwparams.flags=SNDRV_PCM_HW_PARAMS_NORESAMPLE;
  alsafd_hw_params_set_mask(&hwparams,SNDRV_PCM_HW_PARAM_ACCESS,SNDRV_PCM_ACCESS_RW_INTERLEAVED,1);
  alsafd_hw_params_set_mask(&hwparams,SNDRV_PCM_HW_PARAM_FORMAT,SNDRV_PCM_FORMAT_S16,1);
  alsafd_hw_params_set_mask(&hwparams,SNDRV_PCM_HW_PARAM_SUBFORMAT,SNDRV_PCM_SUBFORMAT_STD,1);
  alsafd_hw_params_set_interval(&hwparams,SNDRV_PCM_HW_PARAM_SAMPLE_BITS,16,16);
  alsafd_hw_params_set_interval(&hwparams,SNDRV_PCM_HW_PARAM_FRAME_BITS,0,UINT_MAX);
  alsafd_hw_params_set_interval(&hwparams,SNDRV_PCM_HW_PARAM_CHANNELS,ALSAFD_CHANC_MIN,ALSAFD_CHANC_MAX);
  alsafd_hw_params_set_interval(&hwparams,SNDRV_PCM_HW_PARAM_RATE,ALSAFD_RATE_MIN,ALSAFD_RATE_MAX);
  alsafd_hw_params_set_interval(&hwparams,SNDRV_PCM_HW_PARAM_PERIOD_TIME,0,UINT_MAX); // us between interrupts
  alsafd_hw_params_set_interval(&hwparams,SNDRV_PCM_HW_PARAM_PERIOD_SIZE,0,UINT_MAX); // frames between interrupts
  alsafd_hw_params_set_interval(&hwparams,SNDRV_PCM_HW_PARAM_PERIOD_BYTES,0,UINT_MAX); // bytes between interrupts
  alsafd_hw_params_set_interval(&hwparams,SNDRV_PCM_HW_PARAM_PERIODS,0,UINT_MAX); // interrupts per buffer
  alsafd_hw_params_set_interval(&hwparams,SNDRV_PCM_HW_PARAM_BUFFER_TIME,0,UINT_MAX); // us
  alsafd_hw_params_set_interval(&hwparams,SNDRV_PCM_HW_PARAM_BUFFER_SIZE,ALSAFD_BUF_MIN,ALSAFD_BUF_MAX); // frames
  alsafd_hw_params_set_interval(&hwparams,SNDRV_PCM_HW_PARAM_BUFFER_BYTES,0,UINT_MAX);
  alsafd_hw_params_set_interval(&hwparams,SNDRV_PCM_HW_PARAM_TICK_TIME,0,UINT_MAX); // us
              
  if (ioctl(alsafd->fd,SNDRV_PCM_IOCTL_HW_REFINE,&hwparams)<0) {
    return alsafd_error(alsafd,"SNDRV_PCM_IOCTL_HW_REFINE",0);
  }

  if (setup) {
    if (setup->rate>0) alsafd_hw_params_set_nearest_interval(&hwparams,SNDRV_PCM_HW_PARAM_RATE,setup->rate);
    if (setup->chanc>0) alsafd_hw_params_set_nearest_interval(&hwparams,SNDRV_PCM_HW_PARAM_CHANNELS,setup->chanc);
    if (setup->buffersize>0) alsafd_hw_params_set_nearest_interval(&hwparams,SNDRV_PCM_HW_PARAM_BUFFER_SIZE,setup->buffersize);
  }

  if (ioctl(alsafd->fd,SNDRV_PCM_IOCTL_HW_PARAMS,&hwparams)<0) {
    return alsafd_error(alsafd,"SNDRV_PCM_IOCTL_HW_PARAMS",0);
  }

  if (ioctl(alsafd->fd,SNDRV_PCM_IOCTL_PREPARE)<0) {
    return alsafd_error(alsafd,"SNDRV_PCM_IOCTL_PREPARE",0);
  }
  
  // Read the final agreed values off hwparams.
  if (!hwparams.rate_den) return -1;
  alsafd->rate=hwparams.rate_num/hwparams.rate_den;
  if (alsafd_hw_params_assert_exact_interval(&alsafd->chanc,&hwparams,SNDRV_PCM_HW_PARAM_CHANNELS)<0) return -1;
  if (alsafd_hw_params_assert_exact_interval(&alsafd->hwbufframec,&hwparams,SNDRV_PCM_HW_PARAM_BUFFER_SIZE)<0) return -1;
  
  // Validate.
  if ((alsafd->rate<ALSAFD_RATE_MIN)||(alsafd->rate>ALSAFD_RATE_MAX)) {
    return alsafd_error(alsafd,"","Rejecting rate %d, limit %d..%d.",alsafd->rate,ALSAFD_RATE_MIN,ALSAFD_RATE_MAX);
  }
  if ((alsafd->chanc<ALSAFD_CHANC_MIN)||(alsafd->chanc>ALSAFD_CHANC_MAX)) {
    return alsafd_error(alsafd,"","Rejecting chanc %d, limit %d..%d",alsafd->chanc,ALSAFD_CHANC_MIN,ALSAFD_CHANC_MAX);
  }
  if ((alsafd->hwbufframec<ALSAFD_BUF_MIN)||(alsafd->hwbufframec>ALSAFD_BUF_MAX)) {
    return alsafd_error(alsafd,"","Rejecting buffer size %d, limit %d..%d",alsafd->hwbufframec,ALSAFD_BUF_MIN,ALSAFD_BUF_MAX);
  }
  
  alsafd->bufa=(alsafd->hwbufframec*alsafd->chanc)>>1;
  if (alsafd->buf) free(alsafd->buf);
  if (!(alsafd->buf=malloc(alsafd->bufa<<1))) return -1;
  alsafd->buftime_s=(double)alsafd->hwbufframec/(double)alsafd->rate;
  
  /* Now set some driver software parameters.
   * The main thing is we want avail_min to be half of the hardware buffer size.
   * We will send half-hardware-buffers at a time, and this arrangement should click nicely, let us sleep as much as possible.
   * (in limited experimentation so far, I have found this to be so, and it makes a big impact on overall performance).
   * I've heard that swparams can be used to automatically recover from xrun, but haven't seen that work yet. Not trying here.
   */
  struct snd_pcm_sw_params swparams={
    .tstamp_mode=SNDRV_PCM_TSTAMP_NONE,
    .sleep_min=0,
    .avail_min=alsafd->hwbufframec>>1,
    .xfer_align=1,
    .start_threshold=0,
    .stop_threshold=alsafd->hwbufframec,
    .silence_threshold=alsafd->hwbufframec,
    .silence_size=0,
    .boundary=alsafd->hwbufframec,
    .proto=alsafd->protocol_version,
    .tstamp_type=SNDRV_PCM_TSTAMP_NONE,
  };
  if (ioctl(alsafd->fd,SNDRV_PCM_IOCTL_SW_PARAMS,&swparams)<0) {
    return alsafd_error(alsafd,"SNDRV_PCM_IOCTL_SW_PARAMS",0);
  }
  
  /* And finally, reset the driver and confirm that it enters PREPARED state.
   */
  if (ioctl(alsafd->fd,SNDRV_PCM_IOCTL_RESET)<0) {
    return alsafd_error(alsafd,"SNDRV_PCM_IOCTL_RESET",0);
  }
  struct snd_pcm_status status={0};
  if (ioctl(alsafd->fd,SNDRV_PCM_IOCTL_STATUS,&status)<0) {
    return alsafd_error(alsafd,"SNDRV_PCM_IOCTL_STATUS",0);
  }
  if (status.state!=SNDRV_PCM_STATE_PREPARED) {
    return alsafd_error(alsafd,"","State not PREPARED after setup. state=%d",status.state);
  }
  
  return 0;
}

/* Init: Open and prepare the device.
 */
 
static int alsafd_init_alsa(
  struct alsafd *alsafd,
  const struct alsafd_setup *setup
) {
  if (alsafd_select_device_path(alsafd,setup)<0) return -1;
  if (alsafd_open_device(alsafd)<0) return -1;
  if (alsafd_configure_device(alsafd,setup)<0) return -1;
  return 0;
}

/* Prepare mutex and thread.
 */
 
static int alsafd_init_thread(struct alsafd *alsafd) {
  pthread_mutexattr_t mattr;
  pthread_mutexattr_init(&mattr);
  pthread_mutexattr_settype(&mattr,PTHREAD_MUTEX_RECURSIVE);
  if (pthread_mutex_init(&alsafd->iomtx,&mattr)) return -1;
  pthread_mutexattr_destroy(&mattr);
  if (pthread_create(&alsafd->iothd,0,alsafd_iothd,alsafd)) return -1;
  return 0;
}

/* New.
 */

struct alsafd *alsafd_new(
  const struct alsafd_delegate *delegate,
  const struct alsafd_setup *setup
) {
  if (!delegate||!delegate->pcm_out) return 0;
  struct alsafd *alsafd=calloc(1,sizeof(struct alsafd));
  if (!alsafd) return 0;
  
  alsafd->fd=-1;
  alsafd->delegate=*delegate;
  
  if (alsafd_init_alsa(alsafd,setup)<0) {
    alsafd_del(alsafd);
    return 0;
  }
  
  if (alsafd_init_thread(alsafd)<0) {
    alsafd_del(alsafd);
    return 0;
  }
  
  return alsafd;
}

/* Trivial accessors.
 */
 
void *alsafd_get_userdata(const struct alsafd *alsafd) {
  if (!alsafd) return 0;
  return alsafd->delegate.userdata;
}

int alsafd_get_rate(const struct alsafd *alsafd) {
  if (!alsafd) return 0;
  return alsafd->rate;
}

int alsafd_get_chanc(const struct alsafd *alsafd) {
  if (!alsafd) return 0;
  return alsafd->chanc;
}

int alsafd_get_running(const struct alsafd *alsafd) {
  if (!alsafd) return 0;
  return alsafd->running;
}

void alsafd_set_running(struct alsafd *alsafd,int run) {
  if (!alsafd) return;
  alsafd->running=run?1:0;
}

/* Update.
 */
 
int alsafd_update(struct alsafd *alsafd) {
  if (!alsafd) return 0;
  if (alsafd->ioerror) return -1;
  return 0;
}

/* Lock.
 */
 
int alsafd_lock(struct alsafd *alsafd) {
  if (!alsafd) return -1;
  if (pthread_mutex_lock(&alsafd->iomtx)) return -1;
  return 0;
}

void alsafd_unlock(struct alsafd *alsafd) {
  if (!alsafd) return;
  pthread_mutex_unlock(&alsafd->iomtx);
}

/* Log errors.
 */
 
int alsafd_error(struct alsafd *alsafd,const char *context,const char *fmt,...) {
  const int log_enabled=1;
  if (log_enabled) {
    if (!context) context="";
    fprintf(stderr,"%s:%s: ",alsafd->device?alsafd->device:"alsafd",context);
    if (fmt&&fmt[0]) {
      va_list vargs;
      va_start(vargs,fmt);
      vfprintf(stderr,fmt,vargs);
      fprintf(stderr,"\n");
    } else {
      fprintf(stderr,"%m\n");
    }
  }
  return -1;
}

/* Current time.
 */
 
int64_t alsafd_now() {
  struct timeval tv={0};
  gettimeofday(&tv,0);
  return (int64_t)tv.tv_sec*1000000ll+tv.tv_usec;
}

/* Estimate remaining buffer.
 */
 
double alsafd_estimate_remaining_buffer(const struct alsafd *alsafd) {
  int64_t now=alsafd_now();
  double elapsed=(now-alsafd->buffer_time_us)/1000000.0;
  if (elapsed<0.0) return 0.0;
  if (elapsed>alsafd->buftime_s) return alsafd->buftime_s;
  return alsafd->buftime_s-elapsed;
}
