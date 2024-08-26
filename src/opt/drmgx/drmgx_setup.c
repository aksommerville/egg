#include "drmgx_internal.h"

/* Open device file.
 */
 
int drmgx_open_file(const char *path) {

  if (!path||!path[0]) path="/dev/dri/card0";
  
  if ((drmgx.fd=open(path,O_RDWR))<0) {
    fprintf(stderr,"%s: %m\n",path);
    return -1;
  }
  
  // I get "0.0.0" here, but the important thing is the ioctl doesn't report an error.
  struct drm_version version={0};
  if (ioctl(drmgx.fd,DRM_IOCTL_VERSION,&version)<0) {
    fprintf(stderr,"%s:DRM_IOCTL_VERSION: %m\n",path);
    return -1;
  }
  
  return 0;
}

/* Compare two modes. (a) can be null.
 */
 
static int drmgx_pick_mode(
  drmModeConnectorPtr aconn,drmModeModeInfoPtr amode,
  drmModeConnectorPtr bconn,drmModeModeInfoPtr bmode
) {

  // If we don't have something yet, anything is better than nothing.
  if (!aconn) return 1;
  
  int reqw=768;//TODO do we have an opinion on ideal framebuffer size?
  int reqh=432;
  int reqr=60;
  
  // Refresh rates that don't match the request are a real pain.
  // If one matches and the other doesn't, stop there.
  if ((amode->vrefresh==reqr)&&(bmode->vrefresh!=reqr)) return -1;
  if ((amode->vrefresh!=reqr)&&(bmode->vrefresh==reqr)) return 1;
  
  // If one is smaller than the client's framebuffer, we don't want it.
  int asmall=((amode->hdisplay<reqw)||(amode->vdisplay<reqh));
  int bsmall=((bmode->hdisplay<reqw)||(bmode->vdisplay<reqh));
  if (asmall&&!bsmall) return 1;
  if (!asmall&&bsmall) return -1;
  
  // If one has the PREFERRED flag, that means it's already active.
  // That's good, we want it.
  if ((amode->type&DRM_MODE_TYPE_PREFERRED)&&!(bmode->type&DRM_MODE_TYPE_PREFERRED)) return -1;
  if (!(amode->type&DRM_MODE_TYPE_PREFERRED)&&(bmode->type&DRM_MODE_TYPE_PREFERRED)) return 1;
  
  // Compare aspect ratios to the connector's physical size, closer is better.
  if ((amode->hdisplay!=bmode->hdisplay)||(amode->vdisplay!=bmode->vdisplay)) {
    if (aconn->mmWidth&&aconn->mmHeight&&bconn->mmWidth&&bconn->mmHeight) {
      double aconnaspect=(double)aconn->mmWidth/(double)aconn->mmHeight;
      double bconnaspect=(double)bconn->mmWidth/(double)bconn->mmHeight;
      double aaspect=(double)amode->hdisplay/(double)amode->vdisplay;
      double baspect=(double)bmode->hdisplay/(double)bmode->vdisplay;
      double acmp=aaspect/aconnaspect; if (acmp<1.0) acmp=1.0/acmp;
      double bcmp=baspect/bconnaspect; if (bcmp<1.0) bcmp=1.0/bcmp;
      double diff=acmp-bcmp;
      const double threshold=0.100; // debatable
      if (diff<-threshold) return -1;
      if (diff>threshold) return 1;
    }
  }
  
  // Bigger is better?
  if (amode->hdisplay>bmode->hdisplay) return -1;
  if (amode->hdisplay<bmode->hdisplay) return 1;
  if (amode->vdisplay>bmode->vdisplay) return -1;
  if (amode->vdisplay<bmode->vdisplay) return 1;
  
  // OK, no opinion.
  return 0;
}

/* Pick the best mode.
 */
 
static int drmgx_select_best_mode(drmModeResPtr res) {
  //fprintf(stderr,"%s...\n",__func__);
  //fprintf(stderr,"Client requested %dx%d@%d\n",drmgx.delegate.fbw,drmgx.delegate.fbh,drmgx.delegate.rate);

  drmModeConnectorPtr bestconn=0;
  drmModeModeInfo bestmode={0};
  
  int conni=0;
  for (;conni<res->count_connectors;conni++) {
    drmModeConnectorPtr conn=drmModeGetConnector(drmgx.fd,res->connectors[conni]);
    if (!conn) continue;
    //fprintf(stderr,"connector %p\n",conn);
    if (conn->connection!=DRM_MODE_CONNECTED) {
      drmModeFreeConnector(conn);
      continue;
    }
    
    drmModeModeInfoPtr mode=conn->modes;
    int modei=0;
    for (;modei<conn->count_modes;modei++,mode++) {
      /**
      fprintf(stderr,
        "  mode %04dx%04d @%03dHz 0x%08x 0x%08x '%.*s'\n",
        mode->hdisplay,mode->vdisplay,mode->vrefresh,
        mode->flags,mode->type,DRM_DISPLAY_MODE_LEN,mode->name
      );
      /**/
      
      if (drmgx_pick_mode(bestconn,&bestmode,conn,mode)>0) {
        if (bestconn&&(bestconn!=conn)) {
          drmModeFreeConnector(bestconn);
        }
        bestconn=conn;
        bestmode=*mode;
      }
    }
    if (bestconn!=conn) {
      drmModeFreeConnector(conn);
    }
  }
  if (!bestconn) return -1;
  
  /**
  fprintf(stderr,
    "drm: Selected video mode '%.*s' (%dx%d@%dHz)\n",
    DRM_DISPLAY_MODE_LEN,bestmode.name,
    bestmode.hdisplay,bestmode.vdisplay,bestmode.vrefresh
  );
  /**/
  drmgx.mmw=bestconn->mmWidth;
  drmgx.mmh=bestconn->mmHeight;
  drmgx.connid=bestconn->connector_id;
  drmgx.encid=bestconn->encoder_id;
  drmgx.w=bestmode.hdisplay;
  drmgx.h=bestmode.vdisplay;
  drmgx.rate=bestmode.vrefresh;
  memcpy(&drmgx.mode,&bestmode,sizeof(drmModeModeInfo));
  drmModeFreeConnector(bestconn);
  return 0;
}

/* Connection freshly established: Determine mode, etc.
 */
 
int drmgx_configure() {

  drmModeResPtr res=drmModeGetResources(drmgx.fd);
  if (!res) {
    fprintf(stderr,"drmModeGetResources() failed\n");
    return -1;
  }
  
  if (drmgx_select_best_mode(res)<0) {
    fprintf(stderr,"Failed to select DRM mode.\n");
    drmModeFreeResources(res);
    return -1;
  }
  
  drmModeEncoderPtr encoder=drmModeGetEncoder(drmgx.fd,drmgx.encid);
  if (!encoder) {
    fprintf(stderr,"drmModeGetEncoder() failed\n");
    drmModeFreeResources(res);
    return -1;
  }
  drmgx.crtcid=encoder->crtc_id;
  drmModeFreeEncoder(encoder);
  
  // Store the current CRTC so we can restore it at quit.
  if (!(drmgx.crtc_restore=drmModeGetCrtc(drmgx.fd,drmgx.crtcid))) {
    fprintf(stderr,"drmModeGetCrtc() failed\n");
    drmModeFreeResources(res);
    return -1;
  }
  
  drmModeFreeResources(res);
  return 0;
}

/* Initialize OpenGL ES.
 */
 
int drmgx_init_gx() {
  
  const EGLint context_attribs[]={
    EGL_CONTEXT_CLIENT_VERSION,2,
    EGL_NONE,
  };
  
  const EGLint config_attribs[]={
    EGL_SURFACE_TYPE,EGL_WINDOW_BIT,
    EGL_RED_SIZE,8,
    EGL_GREEN_SIZE,8,
    EGL_BLUE_SIZE,8,
    EGL_ALPHA_SIZE,8,
    EGL_RENDERABLE_TYPE,EGL_OPENGL_ES_BIT,
    EGL_NONE,
  };
  
  const EGLint surface_attribs[]={
    EGL_WIDTH,drmgx.w,
    EGL_HEIGHT,drmgx.h,
    EGL_NONE,
  };
  
  if (!(drmgx.gbmdevice=gbm_create_device(drmgx.fd))) return -1;
  
  if (!(drmgx.gbmsurface=gbm_surface_create(
    drmgx.gbmdevice,
    drmgx.w,drmgx.h,
    GBM_FORMAT_XRGB8888,
    GBM_BO_USE_SCANOUT|GBM_BO_USE_RENDERING
  ))) return -1;
  
  PFNEGLGETPLATFORMDISPLAYEXTPROC eglGetPlatformDisplayEXT=(void*)eglGetProcAddress("eglGetPlatformDisplayEXT");
  if (!eglGetPlatformDisplayEXT) return -1;
  
  if (!(drmgx.egldisplay=eglGetPlatformDisplayEXT(
    EGL_PLATFORM_GBM_KHR,drmgx.gbmdevice,0
  ))) return -1;
  
  EGLint major=0,minor=0;
  if (!eglInitialize(drmgx.egldisplay,&major,&minor)) return -1;
  
  if (!eglBindAPI(EGL_OPENGL_ES_API)) return -1;
  
  EGLConfig eglconfig;
  EGLint n;
  if (!eglChooseConfig(
    drmgx.egldisplay,config_attribs,&eglconfig,1,&n
  )||(n!=1)) return -1;
  
  if (!(drmgx.eglcontext=eglCreateContext(
    drmgx.egldisplay,eglconfig,EGL_NO_CONTEXT,context_attribs
  ))) return -1;
  
  if (!(drmgx.eglsurface=eglCreateWindowSurface(
    drmgx.egldisplay,eglconfig,drmgx.gbmsurface,0
  ))) return -1;
  
  eglMakeCurrent(drmgx.egldisplay,drmgx.eglsurface,drmgx.eglsurface,drmgx.eglcontext);
  
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
  
  return 0;
}
