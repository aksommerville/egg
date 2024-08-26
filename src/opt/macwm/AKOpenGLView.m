#include "macwm_internal.h"

@implementation AKOpenGLView

/* Init.
 */

-(id)initWithWidth:(int)w height:(int)h {

  NSOpenGLPixelFormatAttribute attrs[]={
    NSOpenGLPFADoubleBuffer,
    NSOpenGLPFAColorSize,8,
    NSOpenGLPFAAlphaSize,0,
    NSOpenGLPFADepthSize,0,
    0
  };
  NSOpenGLPixelFormat *pixelFormat=[[NSOpenGLPixelFormat alloc] initWithAttributes:attrs];
  NSRect viewRect=NSMakeRect(0.0,0.0,w,h);
  if (!(self=[super initWithFrame:viewRect pixelFormat:pixelFormat])) {
    [pixelFormat release];
    return 0;
  }
  [pixelFormat release];

  NSOpenGLContext *context=self.openGLContext;
  if (!context) return 0;
  [context setView:self];
  
  [context makeCurrentContext];
  
  return self;
}

/* Frame control.
 */
 
-(void)beginFrame {
  [self.openGLContext makeCurrentContext];
  
}

-(void)endFrame {
  [self.openGLContext flushBuffer];
}

-(BOOL)opaque {
  return 1;
}

/* Hacky support for monitor window.
 */

#if FMN_CREATE_MONITOR_WINDOW

static int monitortexid=0;

void fmn_set_monitortexid(int texid) {
  monitortexid=texid;
}

-(void)readFrame:(void*)dst {
  if (!monitortexid) return;
  glBindTexture(GL_TEXTURE_2D,monitortexid);
  glGetTexImage(GL_TEXTURE_2D,0,GL_RGBA,GL_UNSIGNED_BYTE,dst);
  // And of course it's upside down.
  #define stride ((320+32)*4)
  uint8_t rowbuf[stride];
  int i=(192+32)>>1;
  uint8_t *rowa=dst;
  uint8_t *rowz=rowa+(192+32-1)*stride;
  for (;i-->0;rowa+=stride,rowz-=stride) {
    memcpy(rowbuf,rowa,stride);
    memcpy(rowa,rowz,stride);
    memcpy(rowz,rowbuf,stride);
  }
  #undef stride
}

#endif

@end
