#include "mswm_internal.h"

/* Window is created; finish composing its graphics context.
 */

int mswm_setup_window(struct hostio_video *driver) {

  if (DRIVER->window_setup_complete) return -1;
  DRIVER->window_setup_complete=1;

  if (!(DRIVER->hdc=GetDC(DRIVER->hwnd))) return -1;

  /* Select pixel format. */
  PIXELFORMATDESCRIPTOR pixfmt={
    sizeof(PIXELFORMATDESCRIPTOR),
    1,
    PFD_DRAW_TO_WINDOW|PFD_SUPPORT_OPENGL|PFD_DOUBLEBUFFER,
    PFD_TYPE_RGBA,
    32,
    0,0,0,0,0,0,0,0,
    0,0,0,0,0,
    16,
  };
  int fmtix=ChoosePixelFormat(DRIVER->hdc,&pixfmt);
  if (fmtix<0) return -1;
  SetPixelFormat(DRIVER->hdc,fmtix,&pixfmt);

  /* Initialize OpenGL. */
  if (!(DRIVER->hglrc=wglCreateContext(DRIVER->hdc))) {
    fprintf(stderr,"Failed to create OpenGL context.\n");
    return -1;
  }   
  wglMakeCurrent(DRIVER->hdc,DRIVER->hglrc);

  glEnable(GL_POINT_SPRITE);
  glEnable(GL_PROGRAM_POINT_SIZE);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
  
  glGenTextures(1,&DRIVER->texid);
  glBindTexture(GL_TEXTURE_2D,DRIVER->texid);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST); // TODO should be configurable
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);

  return 0;
}
