#include "alsafd_internal.h"

/* Gather details for one possible device file.
 * Fail if it doesn't look like an ALSA PCM-Out device; failure isn't fatal.
 * Caller will populate (base,path); we must populate the rest.
 * We share one struct snd_pcm_info, in order that there be storage space for the strings.
 */
 
static const char *alsafd_sanitize_string(char *src,int srca) {
  int srcc=0;
  while ((srcc<srca)&&src[srcc]) srcc++;
  while (srcc&&((unsigned char)src[srcc-1]<=0x20)) srcc--;
  int leadc=0;
  while ((leadc<srcc)&&((unsigned char)src[leadc]<=0x20)) leadc++;
  if (leadc) {
    srcc-=leadc;
    memmove(src,src+leadc,srcc);
  }
  if (srcc>=srca) srcc=srca-1;
  src[srcc]=0;
  int i=srcc; while (i-->0) {
    if ((src[i]<0x20)||(src[i]>0x7e)) src[i]='?';
  }
  return src;
}
 
static int alsafd_list_devices_1(
  struct alsafd_device *device,
  const char *path,
  struct snd_pcm_info *info
) {
  int err;
  struct snd_pcm_hw_params hwparams;
  
  // O_NONBLOCK here prevents us from stalling when some other client is using ALSA.
  // Seems like in that case, we will never actually get audio output, but from our perspective we'll appear to.
  // Update: Devices don't become available immediately after the other client closes them.
  // I'm having trouble with Romassist, reopening PCM out immediately after the menu has closed.
  // So try a fixed delay on EBUSY?
  // ...1 second did NOT work but 10 seconds did. uh. We're not going sleep for 10 seconds.
  // Without O_NONBLOCK it waits for the device, but waits forever if some other client is using it.
  
  /* plundersquad doesn't have this problem. what is it using?
   * ...libasound
   * Also, I've been launching via Romassist on the VCS for months with no trouble. This problem only happens on the Nuc.
   * Is it something to do with PulseAudio?
   * Would we be better served by the PulseAudio client?
   * 2023-05-29: Leaving this be for now, and will try Pulse for the Nuc.
   */
  
  //int fd=open(path,O_RDONLY|O_NONBLOCK);
  int fd=open(path,O_RDONLY);
  if (fd<0) {
    fprintf(stderr,"!!! %s open failed: (%d) %m\n",path,errno);
    if (errno==EBUSY) {
      fprintf(stderr,"sleep a bit a try again...\n");
      usleep(1000000);
      fd=open(path,O_RDONLY|O_NONBLOCK);
      if (fd>=0) fprintf(stderr,"...hey sleeping fixed it!\n");
    }
    if (fd<0) return -1;
  }
  
  if ((err=ioctl(fd,SNDRV_PCM_IOCTL_PVERSION,&device->protocol_version))<0) goto _done_;
  device->compiled_version=SNDRV_PCM_VERSION;
  
  if ((err=ioctl(fd,SNDRV_PCM_IOCTL_INFO,info))<0) goto _done_;
  
  if (info->stream!=SNDRV_PCM_STREAM_PLAYBACK) { err=-1; goto _done_; }
  
  device->device=info->device;
  device->subdevice=info->subdevice;
  device->card=info->card;
  device->dev_class=info->dev_class;
  device->dev_subclass=info->dev_subclass;
  
  device->id=alsafd_sanitize_string(info->id,sizeof(info->id));
  device->name=alsafd_sanitize_string(info->name,sizeof(info->name));
  device->subname=alsafd_sanitize_string(info->subname,sizeof(info->subname));
  
  alsafd_hw_params_any(&hwparams);
  if ((err=ioctl(fd,SNDRV_PCM_IOCTL_HW_REFINE,&hwparams))<0) goto _done_;
  
  // Must support interleaved write.
  if (alsafd_hw_params_get_mask(&hwparams,SNDRV_PCM_HW_PARAM_ACCESS,SNDRV_PCM_ACCESS_RW_INTERLEAVED)<=0) { err=-1; goto _done_; }
  
  // Must support s16 in native byte order.
  if (alsafd_hw_params_get_mask(&hwparams,SNDRV_PCM_HW_PARAM_FORMAT,SNDRV_PCM_FORMAT_S16)<=0) { err=-1; goto _done_; }
  
  // Report rate and channel count ranges verbatim.
  alsafd_hw_params_get_interval(&device->ratelo,&device->ratehi,&hwparams,SNDRV_PCM_HW_PARAM_RATE);
  alsafd_hw_params_get_interval(&device->chanclo,&device->chanchi,&hwparams,SNDRV_PCM_HW_PARAM_CHANNELS);
  
 _done_:;
  close(fd);
  return err;
}

/* Helper to list and sort devices within one directory.
 * It's important that we sort, so we guess the same way every time.
 */
 
struct alsafd_devlist {
  char **v; // full path
  int c,a;
};

static void alsafd_devlist_cleanup(struct alsafd_devlist *devlist) {
  if (devlist->v) {
    while (devlist->c-->0) free(devlist->v[devlist->c]);
    free(devlist->v);
  }
  memset(devlist,0,sizeof(struct alsafd_devlist));
}

static int alsafd_devlist_append(struct alsafd_devlist *devlist,const char *src) {
  if (devlist->c>=devlist->a) {
    int na=devlist->a+16;
    if (na>INT_MAX/sizeof(void*)) return -1;
    void *nv=realloc(devlist->v,sizeof(void*)*na);
    if (!nv) return -1;
    devlist->v=nv;
    devlist->a=na;
  }
  if (!(devlist->v[devlist->c]=strdup(src))) return -1;
  devlist->c++;
  return 0;
}

static int alsafd_devlist_gather(struct alsafd_devlist *devlist,const char *dirpath) {
  if (!dirpath) return 0;
  int dirpathc=0; while (dirpath[dirpathc]) dirpathc++;
  char path[1024];
  if (dirpathc>=sizeof(path)) return -1;
  memcpy(path,dirpath,dirpathc);
  path[dirpathc++]='/';
  DIR *dir=opendir(dirpath);
  if (!dir) return -1;
  struct dirent *de;
  while (de=readdir(dir)) {
    if (de->d_type!=DT_CHR) continue;
    if (memcmp(de->d_name,"pcm",3)) continue;
    int basec=3; while (de->d_name[basec]) basec++;
    if (de->d_name[basec-1]!='p') continue;
    if (dirpathc>=sizeof(path)-basec) continue;
    memcpy(path+dirpathc,de->d_name,basec+1);
    if (alsafd_devlist_append(devlist,path)<0) {
      closedir(dir);
      return 0;
    }
  }
  closedir(dir);
  return 0;
}

static void alsafd_devpath_parse(int *card,int *device,const char *path) {
  *card=*device=0;
  char stage='p';
  for (;*path;path++) {
    if (*path=='/') {
      *card=*device=0;
      stage='p';
    } else if (stage=='p') {
      if (*path=='C') stage='c';
    } else if (stage=='c') {
      if ((*path>='0')&&(*path<='9')) {
        (*card)*=10;
        (*card)+=(*path)-'0';
      } else if (*path=='D') stage='d';
      else stage='p';
    } else if (stage=='d') {
      if ((*path>='0')&&(*path<='9')) {
        (*device)*=10;
        (*device)+=(*path)-'0';
      } else if (*path=='c') stage='c';
      else stage='p';
    }
  }
}

static int alsafd_devlist_cmp(const void *a,const void *b) {
  const char *A=*(void**)a,*B=*(void**)b;
  int ac,ad,bc,bd;
  alsafd_devpath_parse(&ac,&ad,A);
  alsafd_devpath_parse(&bc,&bd,B);
  if (ac<bc) return -1;
  if (ac>bc) return 1;
  if (ad<bd) return -1;
  if (ad>bd) return 1;
  return strcmp(a,b);
}

static void alsafd_devlist_sort(struct alsafd_devlist *devlist) {
  qsort(devlist->v,devlist->c,sizeof(void*),alsafd_devlist_cmp);
}

static int alsafd_devlist_init(struct alsafd_devlist *devlist,const char *dirpath) {
  if (alsafd_devlist_gather(devlist,dirpath)<0) return -1;
  alsafd_devlist_sort(devlist);
  return 0;
}

static const char *alsafd_basename(const char *path) {
  const char *base=path;
  int i=0; for (;path[i];i++) if (path[i]=='/') base=path+i+1;
  return base;
}

/* List devices.
 */
 
int alsafd_list_devices(
  const char *path,
  int (*cb)(struct alsafd_device *device,void *userdata),
  void *userdata
) {
  if (!cb) return -1;
  if (!path||!path[0]) path="/dev/snd";
  //fprintf(stderr,"%s %s...\n",__func__,path);
  struct alsafd_devlist devlist={0};
  if (alsafd_devlist_init(&devlist,path)<0) {
    alsafd_devlist_cleanup(&devlist);
    return -1;
  }
  int i=0,err;
  for (;i<devlist.c;i++) {
    //fprintf(stderr,"  %s?\n",devlist.v[i]);
    struct alsafd_device device={0};
    struct snd_pcm_info info={0};
    if (alsafd_list_devices_1(&device,devlist.v[i],&info)<0) continue;
    device.basename=alsafd_basename(devlist.v[i]);
    device.path=devlist.v[i];
    if (err=cb(&device,userdata)) {
      alsafd_devlist_cleanup(&devlist);
      return err;
    }
  }
  alsafd_devlist_cleanup(&devlist);
  return 0;
}

/* Convenience: List devices and choose one.
 */
 
struct alsafd_find_device_context {
// output:
  char *dst;
// match criteria:
  const char *path;
  int ratelo,ratehi;
  int chanclo,chanchi;
// metadata for current output:
  int cardid,deviceid;
  int rateedge,chancedge; // device's range has an endpoint in the acceptable range.
};

static int alsafd_find_device_keep(
  struct alsafd_find_device_context *ctx,
  struct alsafd_device *device
) {
  if (ctx->path) {
    char tmp[1024];
    int tmpc=snprintf(tmp,sizeof(tmp),"%s/%s",ctx->path,device->basename);
    if ((tmpc<1)||(tmpc>=sizeof(tmp))) return -1;
    if (ctx->dst) free(ctx->dst);
    if (!(ctx->dst=strdup(tmp))) return -1;
  } else {
    if (ctx->dst) free(ctx->dst);
    if (!(ctx->dst=strdup(device->basename))) return -1;
  }
  ctx->cardid=device->card;
  ctx->deviceid=device->device;
  ctx->rateedge=(
    ((ctx->ratelo<=device->ratelo)&&(ctx->ratehi>=device->ratelo))||
    ((ctx->ratelo<=device->ratehi)&&(ctx->ratehi>=device->ratehi))
  )?1:0;
  ctx->chancedge=(
    ((ctx->chanclo<=device->chanclo)&&(ctx->chanchi>=device->chanclo))||
    ((ctx->chanclo<=device->chanchi)&&(ctx->chanchi>=device->chanchi))
  )?1:0;
  return 0;
}

static int alsafd_find_device_cb(struct alsafd_device *device,void *userdata) {
  struct alsafd_find_device_context *ctx=userdata;
  
  /*
  fprintf(stderr,
    "  %s (%s) pro=%d hdr=%d card=%d dev=%d sub=%d id=%s name=%s subname=%s class=%d subclass=%d rate=%d..%d,chanc=%d..%d\n",
    device->basename,device->path,
    device->protocol_version,device->compiled_version,
    device->card,device->device,device->subdevice,
    device->id,device->name,device->subname,
    device->dev_class,device->dev_subclass,
    device->ratelo,device->ratehi,device->chanclo,device->chanchi
  );
  /**/
  
  // Disqualify if the rate or channel ranges don't overlap.
  if (device->ratehi<ctx->ratelo) return 0;
  if (device->ratelo>ctx->ratehi) return 0;
  if (device->chanchi<ctx->chanclo) return 0;
  if (device->chanclo>ctx->chanchi) return 0;
  
  // If we don't have anything yet, take this one.
  if (!ctx->dst) return alsafd_find_device_keep(ctx,device);
  
  /* If one matches an edge and the other does not, keep the edge-matcher.
   * eg if you ask for (44100..44100) and a device claims to support (32000..192000),
   * we don't actually know how it will behave with 44100.
   * But if they asked for (22050..44100), or the device claims (44100..192000),
   * we can assume there really will be an acceptable match.
   * Rate takes precedence over chanc here.
   */
  if (ctx->rateedge) {
    if (!(
      ((ctx->ratelo<=device->ratelo)&&(ctx->ratehi>=device->ratelo))||
      ((ctx->ratelo<=device->ratehi)&&(ctx->ratehi>=device->ratehi))
    )) return 0;
  } else if (
    ((ctx->ratelo<=device->ratelo)&&(ctx->ratehi>=device->ratelo))||
    ((ctx->ratelo<=device->ratehi)&&(ctx->ratehi>=device->ratehi))
  ) return alsafd_find_device_keep(ctx,device);
  if (ctx->chancedge) {
    if (!(
      ((ctx->chanclo<=device->chanclo)&&(ctx->chanchi>=device->chanclo))||
      ((ctx->chanclo<=device->chanchi)&&(ctx->chanchi>=device->chanchi))
    )) return 0;
  } else if (
    ((ctx->chanclo<=device->chanclo)&&(ctx->chanchi>=device->chanclo))||
    ((ctx->chanclo<=device->chanchi)&&(ctx->chanchi>=device->chanchi))
  ) return alsafd_find_device_keep(ctx,device);
  
  // They both look good. Take the incoming device if its IDs are lower.
  if (device->card<ctx->cardid) return alsafd_find_device_keep(ctx,device);
  if (device->card==ctx->cardid) {
    if (device->device<ctx->deviceid) return alsafd_find_device_keep(ctx,device);
  }
  
  return 0;
}
 
char *alsafd_find_device(
  const char *path,
  int ratelo,int ratehi,
  int chanclo,int chanchi
) {
  struct alsafd_find_device_context ctx={
    .path=path,
    .ratelo=ratelo,
    .ratehi=ratehi,
    .chanclo=chanclo,
    .chanchi=chanchi,
  };
  alsafd_list_devices(path,alsafd_find_device_cb,&ctx);
  return ctx.dst;
}
