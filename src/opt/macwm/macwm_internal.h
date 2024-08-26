#ifndef MACWM_INTERNAL_H
#define MACWM_INTERNAL_H

#include "macwm.h"
#include "AKWindow.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
//#include <MetalKit/MetalKit.h>
#include <OpenGL/gl.h>

#define MACWM_KEY_LIMIT 16

#ifndef FMN_CREATE_MONITOR_WINDOW
  #define FMN_CREATE_MONITOR_WINDOW 0
#endif

struct macwm {
  struct macwm_delegate delegate;
  int w,h;
  int fullscreen;
  AKWindow *window;
  int modifiers;
  int keyv[MACWM_KEY_LIMIT];
  int keyc;
  int rendermode;
  int fbw,fbh; // MACWM_RENDERMODE_FRAMEBUFFER only
  int mouse_in_bounds;
  struct macwm *monitor; // experimental second view, for kiosk
  void *monitorfb;
  int monitorclock;
};

void macwm_register_key(struct macwm *macwm,int keycode);
void macwm_unregister_key(struct macwm *macwm,int keycode);

@interface AKFramebufferView : NSView {
@public
  const void *fb;
  int read_in_progress;
}
-(id)initWithWidth:(int)width height:(int)height;
@end

@interface AKOpenGLView : NSOpenGLView
-(id)initWithWidth:(int)width height:(int)height;
-(void)beginFrame;
-(void)endFrame;
#if FMN_CREATE_MONITOR_WINDOW
-(void)readFrame:(void*)dst;
#endif
@end

#if 0
@interface AKMetalView : MTKView
#else
@interface AKMetalView : NSView
#endif
-(id)initWithWidth:(int)width height:(int)height;
-(void)beginFrame;
-(void)endFrame;
@end

#endif

float NSScreen_backingScale();
