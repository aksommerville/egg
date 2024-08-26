#include "macwm_internal.h"

// Sanity limit in pixels, applies to each axis.
#define AKFRAMEBUFFERVIEW_SIZE_LIMIT 4096

@implementation AKFramebufferView {
@public
  CGDataProviderRef provider;
  CGColorSpaceRef colorspace;
  int byte_size;
  int fbw,fbh;
  int layerInit;
}

/* CGDataProvider.
 */

static const void *AKFramebufferView_cb_getBytePointer(void *userdata) {
  AKFramebufferView *view=userdata;
  return view->fb;
}

static size_t AKFramebufferView_cb_getBytesAtPosition(void *userdata,void *v,off_t p,size_t c) {
  AKFramebufferView *view=userdata;
  size_t limit=view->byte_size;
  if (p>limit-c) c=limit-p;
  memcpy((uint8_t*)v+p,(uint8_t*)(view->fb)+p,c);
  if (p+c>=limit) view->read_in_progress=0;
  return c;
}

static void AKFramebufferView_cb_releaseBytePointer(void *userdata,const void *fb) {
  AKFramebufferView *view=userdata;
  view->read_in_progress=0;
}

/* Cleanup.
 */

-(void)dealloc {
  if (provider) CGDataProviderRelease(provider);
  if (colorspace) CGColorSpaceRelease(colorspace);
  [super dealloc];
}

/* Init.
 */

-(id)initWithWidth:(int)w height:(int)h {
  if ((w<1)||(w>AKFRAMEBUFFERVIEW_SIZE_LIMIT)) return 0;
  if ((h<1)||(h>AKFRAMEBUFFERVIEW_SIZE_LIMIT)) return 0;
  if (!(self=[super init])) return 0;
  
  byte_size=w*h*4;
  fbw=w;
  fbh=h;
  
  CGDataProviderDirectCallbacks providercb={
    .getBytePointer=AKFramebufferView_cb_getBytePointer,
    .getBytesAtPosition=AKFramebufferView_cb_getBytesAtPosition,
    .releaseBytePointer=AKFramebufferView_cb_releaseBytePointer,
  };
  provider=CGDataProviderCreateDirect(self,byte_size,&providercb);
  
  colorspace=CGColorSpaceCreateDeviceRGB();

  return self;
}

-(BOOL)wantsLayer { return 1; }
-(BOOL)wantsUpdateLayer { return 1; }
-(BOOL)canDrawSubviewsIntoLayer { return 0; }

/* Update.
 */

-(void)updateLayer {
  if (!fb) return;
  if (read_in_progress) return;

  if (!layerInit) {
    self.layer.contentsGravity=kCAGravityResizeAspect;
    // Set the background color to black. Seriously? We have to do all this?
    CGFloat blackv[4]={0.0f,0.0f,0.0f,1.0f};
    CGColorRef black=CGColorCreate(colorspace,blackv);
    self.layer.backgroundColor=black;
    CGColorRelease(black);
    self.layer.magnificationFilter=kCAFilterNearest;
    layerInit=1;
  }

  CGImageRef img=CGImageCreate(
    fbw,fbh,
    8,32,fbw*4,
    colorspace,
    kCGBitmapByteOrderDefault, // CGBitmapInfo
    provider,
    0, // decode
    0, // interpolate
    kCGRenderingIntentDefault
  );
  if (!img) return;

  read_in_progress=1;
  self.layer.contents=(id)img;
  CGImageRelease(img);
}

@end
