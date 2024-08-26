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
  int fd=open(path,O_RDONLY);
  if (fd<0) return -1;
  
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

/* List devices.
 */
 
int alsafd_list_devices(
  const char *path,
  int (*cb)(struct alsafd_device *device,void *userdata),
  void *userdata
) {
  if (!cb) return -1;
  if (!path||!path[0]) path="/dev/snd";
  DIR *dir=opendir(path);
  if (!dir) return -1;
  int err;
  struct snd_pcm_info info;
  struct alsafd_device device;
  char subpath[1024];
  int pathc=0;
  while (path[pathc]) pathc++;
  if (pathc>=sizeof(subpath)) {
    closedir(dir);
    return -1;
  }
  memcpy(subpath,path,pathc);
  if (subpath[pathc-1]!='/') subpath[pathc++]='/';
  struct dirent *de;
  while (de=readdir(dir)) {
    if (de->d_type!=DT_CHR) continue;
    const char *base=de->d_name;
    int basec=0;
    while (base[basec]) basec++;
    if (pathc>=sizeof(subpath)-basec) {
      closedir(dir);
      return -1;
    }
    memcpy(subpath+pathc,base,basec+1);
    if (alsafd_list_devices_1(&device,subpath,&info)<0) continue;
    device.basename=base;
    device.path=path;
    if (err=cb(&device,userdata)) {
      closedir(dir);
      return err;
    }
  }
  closedir(dir);
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
