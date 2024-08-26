#include "macwm_internal.h"

@implementation AKMetalView

/* Init.
 */

-(id)initWithWidth:(int)w height:(int)h {

/*
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
*/

  NSRect viewRect=NSMakeRect(0.0,0.0,w,h);
  if (!(self=[super initWithFrame:viewRect])) return 0;
  
  return self;
}

/* Frame control.
 */
 
-(void)beginFrame {
}

-(void)endFrame {
}

@end
