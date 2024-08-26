/* alsafd.h
 * Required:
 * Optional: hostio
 * Link: -lpthread
 *
 * Minimal interface to ALSA, audio for Linux, PCM output only.
 * Desktop systems should prefer PulseAudio if available.
 * We do not use libasound, we interact with device files directly.
 * I've observed pretty bad performance via libasound, no doubt due to poor configuration on my part,
 * but with the direct approach it performs much better.
 * And it eliminates a dependency, which feels good.
 */
 
#ifndef ALSAFD_H
#define ALSAFD_H

#include <stdint.h>

// Sanity limits we impose artificially. You will never get a context outside these bounds.
#define ALSAFD_RATE_MIN     200
#define ALSAFD_RATE_MAX  200000
#define ALSAFD_CHANC_MIN      1
#define ALSAFD_CHANC_MAX     16
#define ALSAFD_BUF_MIN       64
#define ALSAFD_BUF_MAX    16384

struct alsafd;

struct alsafd_delegate {
  void *userdata;
  
  /* You must write (c) samples to (v).
   * (c) is in samples as usual -- not frames, not bytes.
   */
  void (*pcm_out)(int16_t *v,int c,void *userdata);
};

/* You will not necessarily get the rate or channel count you ask for.
 * Always read it back after creating the context.
 */
struct alsafd_setup {
  int rate; // hz
  int chanc; // usually 1 or 2
  const char *device; // basename or absolute path of device file eg "pcmC0D0p", null to try all.
  int buffersize; // Hardware buffer size in frames. Usually best to leave it zero, let us decide.
};

/* Iterate possible PCM devices in a given directory.
 * We only report devices that support interleaved output of native s16.
 * Stop iteration by returning nonzero; we return the same.
 */
struct alsafd_device {
  const char *basename; // Give this to the ctor to use this device.
  const char *path;
  int protocol_version; // SNDRV_PCM_IOCTL_PVERSION, as reported by device.
  int compiled_version; // SNDRV_PCM_VERSION, what we were compiled against.
  // SNDRV_PCM_IOCTL_INFO:
  int card; // (card,device) are usually part of the basename.
  int device;
  int subdevice;
  const char *id; // I've observed that (id) and (name) are always the same. (with limited testing)
  const char *name;
  const char *subname;
  int dev_class;
  int dev_subclass;
  // SNDRV_PCM_IOCTL_HW_REFINE:
  int ratelo,ratehi;
  int chanclo,chanchi;
};
int alsafd_list_devices(
  const char *path, // null for the default "/dev/snd"
  int (*cb)(struct alsafd_device *device,void *userdata),
  void *userdata
);

/* Convenience, find the device best matching your preferred rate and channel count.
 * We prefer lower device and card numbers, so you'll usually get pcmC0D0p.
 * Whatever we return, you can use as setup->device, and then you must free it.
 */
char *alsafd_find_device(
  const char *path, // null for the default "/dev/snd"
  int ratelo,int ratehi,
  int chanclo,int chanchi
);

void alsafd_del(struct alsafd *alsafd);

/* (delegate->pcm_out) is required.
 * Everything else is optional. 1@44100 on any working device is the default.
 */
struct alsafd *alsafd_new(
  const struct alsafd_delegate *delegate,
  const struct alsafd_setup *setup
);

void *alsafd_get_userdata(const struct alsafd *alsafd);
int alsafd_get_rate(const struct alsafd *alsafd);
int alsafd_get_chanc(const struct alsafd *alsafd);
const char *alsafd_get_device(const struct alsafd *alsafd);
int alsafd_get_running(const struct alsafd *alsafd);

/* A new context is stopped until you explicitly set_running(1).
 */
void alsafd_set_running(struct alsafd *alsafd,int run);

/* Update may be used for error reporting.
 * I/O happens on our background thread, controlled by lock/unlock.
 */
int alsafd_update(struct alsafd *alsafd);
int alsafd_lock(struct alsafd *alsafd);
void alsafd_unlock(struct alsafd *alsafd);

double alsafd_estimate_remaining_buffer(const struct alsafd *alsafd);

#endif
