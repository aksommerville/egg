#include "macwm_internal.h"

/* Delete.
 */

void macwm_del(struct macwm *macwm) {
  if (!macwm) return;
  [macwm->window release];
  free(macwm);
}

/* Set initial window size based on framebuffer size and screen size, or make something up.
 */

static void macwm_guess_initial_window_size(struct macwm *macwm) {

  // First capture the main screen's dimensions, or assume 1920x1080.
  NSScreen *screen=NSScreen.mainScreen;
  int screenw=1920,screenh=1080;
  if (screen) {
    screenw=screen.frame.size.width;
    screenh=screen.frame.size.height;
  }

  // If a framebuffer is in use, maintain its aspect ratio and fill most of the screen.
  if ((macwm->fbw>0)&&(macwm->fbh>0)) {
    int usew=(screenw*7)>>3;
    int useh=(screenh*7)>>3;
    int wforh=(macwm->fbw*useh)/macwm->fbh;
    if (wforh<=usew) {
      macwm->w=wforh;
      macwm->h=useh;
    } else {
      macwm->w=usew;
      macwm->h=(macwm->fbh*usew)/macwm->fbw;
    }
    return;
  }

  // No framebuffer, so we have no guidance re aspect ratio. Use exactly half of the screen.
  macwm->w=screenw>>1;
  macwm->h=screenh>>1;
}

/* New.
 */

struct macwm *macwm_new(
  const struct macwm_delegate *delegate,
  const struct macwm_setup *setup
) {
  struct macwm *macwm=calloc(1,sizeof(struct macwm));
  if (!macwm) return 0;

  if (delegate) macwm->delegate=*delegate;

  const char *title=0;
  if (setup) {
    macwm->w=setup->w;
    macwm->h=setup->h;
    macwm->fullscreen=setup->fullscreen?1:0;
    macwm->rendermode=setup->rendermode;
    macwm->fbw=setup->fbw;
    macwm->fbh=setup->fbh;
    title=setup->title;
  }
  if ((macwm->w<1)||(macwm->h<1)) {
    macwm_guess_initial_window_size(macwm);
  }
  if (!macwm->rendermode) macwm->rendermode=MACWM_RENDERMODE_FRAMEBUFFER;

  if (!(macwm->window=[[AKWindow alloc] initWithOwner:macwm title:title])) {
    macwm_del(macwm);
    return 0;
  }

  if (delegate&&delegate->key&&FMN_CREATE_MONITOR_WINDOW) {
    // +32 here because the main framebuffer has a 16-pixel border on all sides. We don't bother stripping that.
    struct macwm_setup monitor_setup={
      .w=setup->fbw+32,
      .h=setup->fbh+32,
      .fullscreen=0,
      .rendermode=MACWM_RENDERMODE_FRAMEBUFFER,
      .fbw=setup->fbw+32,
      .fbh=setup->fbh+32,
      .title="Full Moon Monitor",
    };
    if (macwm->monitor=macwm_new(0,&monitor_setup)) {
      macwm->monitorfb=malloc(monitor_setup.fbw*monitor_setup.fbh*4);
    }
  }

  return macwm;
}

/* Trivial accessors.
 */

void macwm_get_size(int *w,int *h,const struct macwm *macwm) {
  if (!macwm) return;
  if (w) *w=macwm->w;
  if (h) *h=macwm->h;
}

int macwm_get_fullscreen(const struct macwm *macwm) {
  if (!macwm) return 0;
  return macwm->fullscreen;
}

/* Fullscreen.
 */

void macwm_set_fullscreen(struct macwm *macwm,int state) {
  if (!macwm) return;
  if (state<0) state=macwm->fullscreen?0:1;
  if (state) {
    if (macwm->fullscreen) return;
    [macwm->window toggleFullScreen:0];
    macwm->fullscreen=1;
  } else {
    if (!macwm->fullscreen) return;
    [macwm->window toggleFullScreen:0];
    macwm->fullscreen=0;
  }
}

/* Update.
 */

int macwm_update(struct macwm *macwm) {
  if (!macwm) return -1;
  // Probably nothing to do here? Events are driven by Apple's IoC layer.
  return 0;
}

/* Release all held keys.
 */

void macwm_release_keys(struct macwm *macwm) {
  if (macwm->delegate.key) {
    while (macwm->keyc>0) {
      macwm->keyc--;
      int keycode=macwm_hid_from_mac_keycode(macwm->keyv[macwm->keyc]);
      if (keycode) {
        macwm->delegate.key(macwm->delegate.userdata,keycode,0);
      }
    }
  } else {
    macwm->keyc=0;
  }
}

void macwm_register_key(struct macwm *macwm,int keycode) {
  if (!keycode) return;
  if (macwm->keyc>=MACWM_KEY_LIMIT) return;
  int i=macwm->keyc; while (i-->0) {
    if (macwm->keyv[i]==keycode) return;
  }
  macwm->keyv[macwm->keyc++]=keycode;
}

void macwm_unregister_key(struct macwm *macwm,int keycode) {
  int i=macwm->keyc; while (i-->0) {
    if (macwm->keyv[i]==keycode) {
      macwm->keyc--;
      memmove(macwm->keyv+i,macwm->keyv+i+1,sizeof(int)*(macwm->keyc-i));
      return;
    }
  }
}

/* Send framebuffer.
 */
 
void macwm_send_framebuffer(struct macwm *macwm,const void *fb) {
  if (!macwm||!fb) return;
  if (macwm->rendermode!=MACWM_RENDERMODE_FRAMEBUFFER) return;
  AKFramebufferView *view=macwm->window.contentView;
  if (view->read_in_progress) return;
  view->fb=fb;
  view.needsDisplay=1;
}

/* Begin/end GX frame.
 */
 
int macwm_render_begin(struct macwm *macwm) {
  if (!macwm) return -1;
  switch (macwm->rendermode) {
    case MACWM_RENDERMODE_OPENGL: {
        [(AKOpenGLView*)macwm->window.contentView beginFrame];
        return 0;
      }
    case MACWM_RENDERMODE_METAL: {
        break;//TODO
      }
  }
  return -1;
}

void macwm_render_end(struct macwm *macwm) {
  if (!macwm) return;
  switch (macwm->rendermode) {
  
    case MACWM_RENDERMODE_OPENGL: {
        [(AKOpenGLView*)macwm->window.contentView endFrame];
        #if FMN_CREATE_MONITOR_WINDOW
        if (macwm->monitor&&macwm->monitorfb) {
          if (--(macwm->monitorclock)<=0) {
            macwm->monitorclock=6; // Don't draw the monitor every frame. I think 10 Hz should be just fine. It's expensive.
            [(AKOpenGLView*)macwm->window.contentView readFrame:macwm->monitorfb];
            macwm_send_framebuffer(macwm->monitor,macwm->monitorfb);
          }
        }
        #endif
        break;
      }
      
    case MACWM_RENDERMODE_METAL: {
        break;//TODO
      }
  }
}

float NSScreen_backingScale() {
  return NSScreen.mainScreen.backingScaleFactor;
}
