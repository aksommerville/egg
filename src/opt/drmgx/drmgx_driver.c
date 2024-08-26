#include "drmgx_internal.h"

struct drmgx_driver drmgx={.fd=-1};

static int drmgx_swap_egl(uint32_t *fbid);

/* Cleanup.
 */
 
static void drmgx_fb_cleanup(struct drmgx_fb *fb) {
  if (fb->fbid) {
    drmModeRmFB(drmgx.fd,fb->fbid);
  }
}
 
void drmgx_quit() {

  // If waiting for a page flip, we must let it finish first.
  if ((drmgx.fd>=0)&&drmgx.flip_pending) {
    struct pollfd pollfd={.fd=drmgx.fd,.events=POLLIN|POLLERR|POLLHUP};
    if (poll(&pollfd,1,500)>0) {
      char dummy[64];
      if (read(drmgx.fd,dummy,sizeof(dummy))) ;
    }
  }
  
  if (drmgx.eglcontext) eglDestroyContext(drmgx.egldisplay,drmgx.eglcontext);
  if (drmgx.eglsurface) eglDestroySurface(drmgx.egldisplay,drmgx.eglsurface);
  if (drmgx.egldisplay) eglTerminate(drmgx.egldisplay);
  
  drmgx_fb_cleanup(drmgx.fbv+0);
  drmgx_fb_cleanup(drmgx.fbv+1);
  
  if (drmgx.crtc_restore&&(drmgx.fd>=0)) {
    drmModeCrtcPtr crtc=drmgx.crtc_restore;
    drmModeSetCrtc(
      drmgx.fd,crtc->crtc_id,crtc->buffer_id,
      crtc->x,crtc->y,&drmgx.connid,1,&crtc->mode
    );
    drmModeFreeCrtc(drmgx.crtc_restore);
  }
  
  if (drmgx.fd>=0) close(drmgx.fd);
  
  memset(&drmgx,0,sizeof(drmgx));
  drmgx.fd=-1;	
}

/* Init.
 */
 
int drmgx_init(const char *device) {

  drmgx.fd=-1;
  drmgx.crtcunset=1;

  if (!drmAvailable()) {
    fprintf(stderr,"DRM not available.\n");
    return -1;
  }
  
  if (
    (drmgx_open_file(device)<0)||
    (drmgx_configure()<0)||
    (drmgx_init_gx()<0)||
  0) {
    drmgx_quit();
    return -1;
  }

  return 0;
}

/* Poll file.
 */
 
static void drmgx_cb_vblank(
  int fd,unsigned int seq,unsigned int times,unsigned int timeus,void *userdata
) {}
 
static void drmgx_cb_page1(
  int fd,unsigned int seq,unsigned int times,unsigned int timeus,void *userdata
) {
  drmgx.flip_pending=0;
}
 
static void drmgx_cb_page2(
  int fd,unsigned int seq,unsigned int times,unsigned int timeus,unsigned int ctrcid,void *userdata
) {
  drmgx_cb_page1(fd,seq,times,timeus,userdata);
}
 
static void drmgx_cb_seq(
  int fd,uint64_t seq,uint64_t timeus,uint64_t userdata
) {}
 
static int drmgx_poll_file(int to_ms) {
  struct pollfd pollfd={.fd=drmgx.fd,.events=POLLIN};
  if (poll(&pollfd,1,to_ms)<=0) return 0;
  drmEventContext ctx={
    .version=DRM_EVENT_CONTEXT_VERSION,
    .vblank_handler=drmgx_cb_vblank,
    .page_flip_handler=drmgx_cb_page1,
    .page_flip_handler2=drmgx_cb_page2,
    .sequence_handler=drmgx_cb_seq,
  };
  int err=drmHandleEvent(drmgx.fd,&ctx);
  if (err<0) return -1;
  return 0;
}

/* Swap.
 */
 
static int drmgx_swap_egl(uint32_t *fbid) {
  eglSwapBuffers(drmgx.egldisplay,drmgx.eglsurface);
  
  struct gbm_bo *bo=gbm_surface_lock_front_buffer(drmgx.gbmsurface);
  if (!bo) return -1;
  
  int handle=gbm_bo_get_handle(bo).u32;
  struct drmgx_fb *fb;
  if (!drmgx.fbv[0].handle) {
    fb=drmgx.fbv;
  } else if (handle==drmgx.fbv[0].handle) {
    fb=drmgx.fbv;
  } else {
    fb=drmgx.fbv+1;
  }
  
  if (!fb->fbid) {
    int width=gbm_bo_get_width(bo);
    int height=gbm_bo_get_height(bo);
    int stride=gbm_bo_get_stride(bo);
    fb->handle=handle;
    if (drmModeAddFB(drmgx.fd,width,height,24,32,stride,fb->handle,&fb->fbid)<0) return -1;
    
    if (drmgx.crtcunset) {
      if (drmModeSetCrtc(
        drmgx.fd,drmgx.crtcid,fb->fbid,0,0,
        &drmgx.connid,1,&drmgx.mode
      )<0) {
        fprintf(stderr,"drmModeSetCrtc: %m\n");
        return -1;
      }
      drmgx.crtcunset=0;
    }
  }
  
  *fbid=fb->fbid;
  if (drmgx.bo_pending) {
    gbm_surface_release_buffer(drmgx.gbmsurface,drmgx.bo_pending);
  }
  drmgx.bo_pending=bo;
  
  return 0;
}

int drmgx_swap() {

  // There must be no more than one page flip in flight at a time.
  // If one is pending -- likely -- give it a chance to finish.
  if (drmgx.flip_pending) {
    if (drmgx_poll_file(20)<0) return -1;
    if (drmgx.flip_pending) {
      // Page flip didn't complete after a 20 ms delay? Drop the frame, no worries.
      return 0;
    }
  }
  drmgx.flip_pending=1;
  
  uint32_t fbid=0;
  if (drmgx_swap_egl(&fbid)<0) {
    drmgx.flip_pending=0;
    return -1;
  }
  
  if (drmModePageFlip(drmgx.fd,drmgx.crtcid,fbid,DRM_MODE_PAGE_FLIP_EVENT,0)<0) {
    fprintf(stderr,"drmModePageFlip: %m\n");
    drmgx.flip_pending=0;
    return -1;
  }

  return 0;
}

/* Trivial accessors.
 */
 
void drmgx_get_size(int *w,int *h) {
  *w=drmgx.w;
  *h=drmgx.h;
}
