#include "macwm_internal.h"

@implementation AKWindow {
  struct macwm *macwm; // WEAK
}

/* Init.
 */
 
-(id)initWithOwner:(struct macwm*)_macwm title:(const char*)title {

  NSScreen *screen=NSScreen.mainScreen;
  int screenw=1920,screenh=1080;
  if (screen) {
    screenw=screen.frame.size.width;
    screenh=screen.frame.size.height;
  }
  NSRect bounds=NSMakeRect(
    (screenw>>1)-(_macwm->w>>1),
    (screenh>>1)-(_macwm->h>>1),
    _macwm->w,_macwm->h
  );
  
  NSWindowStyleMask styleMask=
    NSWindowStyleMaskTitled|
    NSWindowStyleMaskClosable|
    NSWindowStyleMaskMiniaturizable|
    NSWindowStyleMaskResizable|
  0;

  if (!(self=[super initWithContentRect:bounds styleMask:styleMask backing:NSBackingStoreBuffered defer:0])) {
    return 0;
  }

  self->macwm=_macwm;
  self.delegate=self;
  self.releasedWhenClosed=0;
  self.acceptsMouseMovedEvents=TRUE;

  if (title) {
    self.title=[NSString stringWithUTF8String:title];
  }

  switch (macwm->rendermode) {
    case MACWM_RENDERMODE_FRAMEBUFFER: self.contentView=[[AKFramebufferView alloc] initWithWidth:macwm->fbw height:macwm->fbh]; break;
    case MACWM_RENDERMODE_OPENGL: self.contentView=[[AKOpenGLView alloc] initWithWidth:macwm->w height:macwm->h]; break;
    case MACWM_RENDERMODE_METAL: self.contentView=[[AKMetalView alloc] initWithWidth:macwm->w height:macwm->h]; break;
    default: fprintf(stderr,"Unsupported macwm.rendermode %d\n",macwm->rendermode);
  }
  if (!self.contentView) return 0;

  if (macwm->fullscreen) {
    [self toggleFullScreen:self];
  }

  [self makeKeyAndOrderFront:0];

  CGRect cr=[self contentRectForFrameRect:NSMakeRect(0.0f,0.0f,100.0f,100.0f)];
  framew=100.0f-cr.size.width;
  frameh=100.0f-cr.size.height;

  return self;
}

/* Window events.
 */

-(NSSize)windowWillResize:(NSWindow*)window toSize:(NSSize)size {
  CGFloat w=size.width,h=size.height;
  if (!macwm->fullscreen) {
    w-=framew;
    h-=frameh;
  }
  self.contentView.frame=NSMakeRect(0,0,w,h);
  macwm->w=w;
  macwm->h=h;
  if (macwm->delegate.resize) macwm->delegate.resize(macwm->delegate.userdata,(int)w,(int)h);
  return size;
}

-(void)windowDidResize:(id)dummy {
  // What we set at windowWillResize might get fudged by the WM after. It is important that macwm->(w,h) be at least as large as the contentView.
  int changed=0;
  self.contentView.frame=NSMakeRect(0.0f,0.0f,macwm->w,macwm->h);
}

-(void)windowWillClose:(NSWindow*)window {
  if (macwm->delegate.close) macwm->delegate.close(macwm->delegate.userdata);
}

-(void)windowDidResignKey:(NSWindow*)window {
  macwm_release_keys(macwm);
}

-(void)windowWillEnterFullScreen:(id)any {
  macwm->fullscreen=1;
}

-(void)windowWillExitFullScreen:(id)any {
  macwm->fullscreen=0;
}

-(void)windowDidEnterFullScreen:(NSNotification*)note {
  macwm_release_keys(macwm);
}

-(void)windowDidExitFullScreen:(NSNotification*)note {
  macwm_release_keys(macwm);
}

/* Mouse events.
 */

-(void)mouseButton:(int)btnid value:(int)value {
  if (macwm->delegate.mbutton) macwm->delegate.mbutton(macwm->delegate.userdata,btnid,value);
}
-(void)mouseDown:(NSEvent*)event { [self mouseButton:event.buttonNumber value:1]; }
-(void)rightMouseDown:(NSEvent*)event { [self mouseButton:event.buttonNumber value:1]; }
-(void)otherMouseDown:(NSEvent*)event { [self mouseButton:event.buttonNumber value:1]; }
-(void)mouseUp:(NSEvent*)event { [self mouseButton:event.buttonNumber value:0]; }
-(void)rightMouseUp:(NSEvent*)event { [self mouseButton:event.buttonNumber value:0]; }
-(void)otherMouseUp:(NSEvent*)event { [self mouseButton:event.buttonNumber value:0]; }

-(void)mouseMoved:(NSEvent*)event {
  int x=event.locationInWindow.x;
  int y=macwm->h-event.locationInWindow.y-1;
  if (macwm->delegate.mmotion) macwm->delegate.mmotion(macwm->delegate.userdata,x,y);

  int inbounds=((x>=0)&&(y>=0)&&(x<macwm->w)&&(y<macwm->h));
  if (inbounds&&!macwm->mouse_in_bounds) {
    [NSCursor hide];
    macwm->mouse_in_bounds=1;
  } else if (!inbounds&&macwm->mouse_in_bounds) {
    [NSCursor unhide];
    macwm->mouse_in_bounds=0;
  }
}

-(void)mouseDragged:(NSEvent*)event { [self mouseMoved:event]; }
-(void)rightMouseDragged:(NSEvent*)event { [self mouseMoved:event]; }
-(void)otherMouseDragged:(NSEvent*)event { [self mouseMoved:event]; }

-(void)scrollWheel:(NSEvent*)event {
  if (!macwm->delegate.mwheel) return;
  int dx=-event.scrollingDeltaX;
  int dy=-event.scrollingDeltaY;
  if (dx||dy) {
    macwm->delegate.mwheel(macwm->delegate.userdata,dx,dy);
  }
}

/* Keyboard events.
 */

-(void)key:(NSEvent*)event value:(int)value {
  if (value) macwm_register_key(macwm,event.keyCode);
  else macwm_unregister_key(macwm,event.keyCode);
  if (macwm->delegate.key) {
    int keycode=macwm_hid_from_mac_keycode(event.keyCode);
    if (keycode) {
      if (macwm->delegate.key(macwm->delegate.userdata,keycode,value)) return;
    }
  }
  if (value&&macwm->delegate.text) {
    const char *src=event.characters.UTF8String;
    // We pretty much decode UTF-8 correctly. Not checking 0x80 on the trailing bytes.
    if (src) while (*src) {
      if (!((*src)&0x80)) {
        macwm->delegate.text(macwm->delegate.userdata,*src);
        src+=1;
      } else if (!((*src)&0x40)) {
        // Invalid byte, skip it.
        src+=1;
      } else if (!(src[0]&0x20)&&src[1]) {
        int codepoint=((src[0]&0x1f)<<6)|(src[1]&0x3f);
        macwm->delegate.text(macwm->delegate.userdata,codepoint);
        src+=2;
      } else if (!(src[0]&0x10)&&src[1]&&src[2]) {
        int codepoint=((src[0]&0x0f)<<12)|((src[1]&0x3f)<<6)|(src[2]&0x3f);
        macwm->delegate.text(macwm->delegate.userdata,codepoint);
        src+=3;
      } else if (!(src[0]&0x08)&&src[1]&&src[2]&&src[3]) {
        int codepoint=((src[0]&0x07)<<18)|((src[1]&0x3f)<<12)|((src[2]&0x3f)<<6)|(src[3]&0x3f);
        macwm->delegate.text(macwm->delegate.userdata,codepoint);
        src+=4;
      } else {
        // Invalid byte, skip it.
        src+=1;
      }
    }
  }
}

-(void)keyDown:(NSEvent*)event { [self key:event value:event.ARepeat?2:1]; }
-(void)keyUp:(NSEvent*)event { [self key:event value:0]; }

-(void)flagsChanged:(NSEvent*)event {
  int prev=macwm->modifiers;
  macwm->modifiers=event.modifierFlags;
  if (!macwm->delegate.key) return;
  int bits;
  #define FAKEKEY(keycode,value) { \
    if (value) macwm_register_key(macwm,keycode); \
    else macwm_unregister_key(macwm,keycode); \
    macwm->delegate.key(macwm->delegate.userdata,keycode,value); \
  }
  if (bits=(macwm->modifiers&~prev)) {
    if (bits&NSEventModifierFlagCapsLock) FAKEKEY(0x00070039,1)
    if (bits&NSEventModifierFlagShift   ) FAKEKEY(0x000700e1,1)
    if (bits&NSEventModifierFlagCommand ) FAKEKEY(0x000700e3,1)
    if (bits&NSEventModifierFlagOption  ) FAKEKEY(0x000700e2,1)
    if (bits&NSEventModifierFlagControl ) FAKEKEY(0x000700e0,1)
    if (bits&NSEventModifierFlagFunction) FAKEKEY(0x00070065,1)
  }
  if (bits=(~macwm->modifiers&prev)) {
    if (bits&NSEventModifierFlagCapsLock) FAKEKEY(0x00070039,0)
    if (bits&NSEventModifierFlagShift   ) FAKEKEY(0x000700e1,0)
    if (bits&NSEventModifierFlagCommand ) FAKEKEY(0x000700e3,0)
    if (bits&NSEventModifierFlagOption  ) FAKEKEY(0x000700e2,0)
    if (bits&NSEventModifierFlagControl ) FAKEKEY(0x000700e0,0)
    if (bits&NSEventModifierFlagFunction) FAKEKEY(0x00070065,0)
  }
  #undef FAKEKEY
}

@end
