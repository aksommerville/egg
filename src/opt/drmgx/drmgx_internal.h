#ifndef DRMGX_INTERNAL_H
#define DRMGX_INTERNAL_H

#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <time.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <gbm.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GL/gl.h>

struct drmgx_fb {
  uint32_t fbid;
  int handle;
  int size;
};

extern struct drmgx_driver {
  int fd;
  
  int mmw,mmh; // monitor's physical size
  int w,h; // monitor's logical size in pixels
  int rate; // monitor's refresh rate in hertz
  drmModeModeInfo mode; // ...and more in that line
  int connid,encid,crtcid;
  drmModeCrtcPtr crtc_restore;
  
  int flip_pending;
  struct drmgx_fb fbv[2];
  int fbp;
  struct gbm_bo *bo_pending;
  int crtcunset;
  
  struct gbm_device *gbmdevice;
  struct gbm_surface *gbmsurface;
  EGLDisplay egldisplay;
  EGLContext eglcontext;
  EGLSurface eglsurface;
  
} drmgx;

int drmgx_open_file(const char *path);
int drmgx_configure();
int drmgx_init_gx();

/* Public API.
 */
void drmgx_quit();
int drmgx_init(const char *path);
int drmgx_swap();

#endif
