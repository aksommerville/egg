#include "egg_native_internal.h"
#include "opt/ico/ico.h"

/* Initialize video.
 */
 
static int egg_native_video_init() {
  struct hostio_video_delegate delegate={
    .cb_close=egg_native_cb_close,
    .cb_resize=egg_native_cb_resize,
    .cb_focus=egg_native_cb_focus,
    .cb_key=egg_native_cb_key,
    .cb_char=egg_native_cb_char,
    .cb_mmotion=egg_native_cb_mmotion,
    .cb_mbutton=egg_native_cb_mbutton,
    .cb_mwheel=egg_native_cb_mwheel,
  };
  struct hostio_video_setup setup={
    .w=egg.video_w,
    .h=egg.video_h,
    .ratemin=egg.video_rate,
    .ratemax=egg.video_rate,
    .fullscreen=egg.video_fullscreen,
    .device=egg.video_device,
  };
  
  // Flesh out (setup) with some things from the ROM details if available.
  char title[256];
  int titlec=egg_native_rom_get_detail(title,sizeof(title),"title",5);
  if ((titlec>0)&&(titlec<sizeof(title))) setup.title=title;
  egg_native_rom_get_framebuffer(&setup.fbw,&setup.fbh);
  struct ico *ico=0;
  char iconserial[4096];
  int iconserialc=egg_native_rom_get_detail(iconserial,sizeof(iconserial),"icon",4);
  if ((iconserialc>0)&&(iconserialc<=sizeof(iconserial))) {
    ico=ico_new(iconserial,iconserialc);
    if (ico) {
      struct ico_image *image=ico_get_image(ico,32,32,1,1);
      if (image) {
        setup.iconrgba=image->v;
        setup.iconw=image->w;
        setup.iconh=image->h;
      }
    }
  }

  if (hostio_video_init(egg.hostio,egg.video_driver,-1,&delegate,&setup)<0) {
    ico_del(ico);
    return -1;
  }
  ico_del(ico);
  fprintf(stderr,"%s: Using video driver '%s'.\n",egg.exename,egg.hostio->video->type->name);
  return 0;
}

/* Initialize audio.
 */
 
static int egg_native_audio_init() {
  struct hostio_audio_delegate delegate={
    .cb_pcm_out=egg_native_cb_pcm_out,
  };
  struct hostio_audio_setup setup={
    .rate=egg.audio_rate,
    .chanc=egg.audio_chanc,
    .device=egg.audio_device,
    .buffersize=egg.audio_buffersize,
  };
  if (hostio_audio_init(egg.hostio,egg.audio_driver,-1,&delegate,&setup)<0) return -1;
  fprintf(stderr,
    "%s: Using audio driver '%s' rate=%d chanc=%d.\n",
    egg.exename,egg.hostio->audio->type->name,egg.hostio->audio->rate,egg.hostio->audio->chanc
  );
  return 0;
}

/* Initialize input.
 */
 
static int egg_native_input_init() {
  struct hostio_input_delegate delegate={
    .cb_connect=egg_native_cb_connect,
    .cb_disconnect=egg_native_cb_disconnect,
    .cb_button=egg_native_cb_button,
  };
  struct hostio_input_setup setup={0};
  if (hostio_input_init(egg.hostio,egg.input_driver,-1,&delegate,&setup)<0) return -1;
  fprintf(stderr,"%s: Using input driver '%s'.\n",egg.exename,egg.hostio->input->type->name);
  return 0;
}

/* Initialize drivers, main entry point.
 */
 
int egg_native_drivers_init() {
  int err;
  if ((err=egg_native_video_init())<0) {
    if (err!=-2) fprintf(stderr,"%s: Unspecified error initializing video.\n",egg.exename);
    return -2;
  }
  if ((err=egg_native_audio_init())<0) {
    if (err!=-2) fprintf(stderr,"%s: Unspecified error initializing audio.\n",egg.exename);
    return -2;
  }
  if ((err=egg_native_input_init())<0) {
    if (err!=-2) fprintf(stderr,"%s: Unspecified error initializing input.\n",egg.exename);
    return -2;
  }
  return 0;
}
