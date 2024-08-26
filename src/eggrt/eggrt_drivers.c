/* eggrt_drivers.c
 * Interface to hostio, which does the heavier lifting for IO abstraction.
 * We are also responsible for the abstract layer on top of that: Render, Synth, Input Manager.
 */

#include "eggrt_internal.h"
#include "opt/image/image.h"

/* Hostio callbacks.
 */
 
static void hostio_cb_close(struct hostio_video *driver) {
  fprintf(stderr,"%s\n",__func__);
  eggrt.terminate++;
}

static void hostio_cb_focus(struct hostio_video *driver,int focus) {
  fprintf(stderr,"%s %d\n",__func__,focus);
}

static void hostio_cb_resize(struct hostio_video *driver,int w,int h) {
  //fprintf(stderr,"%s %dx%d\n",__func__,w,h);
}

static int hostio_cb_key(struct hostio_video *driver,int keycode,int value) {
  fprintf(stderr,"%s %#.8x=%d\n",__func__,keycode,value);
  return 0; // Or nonzero to suppress text.
}

static void hostio_cb_text(struct hostio_video *driver,int codepoint) {
  fprintf(stderr,"%s U+%x\n",__func__,codepoint);
}

static void hostio_cb_mmotion(struct hostio_video *driver,int x,int y) {
  //fprintf(stderr,"%s %d,%d\n",__func__,x,y);
}

static void hostio_cb_mbutton(struct hostio_video *driver,int btnid,int value) {
  fprintf(stderr,"%s %d=%d\n",__func__,btnid,value);
}

static void hostio_cb_mwheel(struct hostio_video *driver,int dx,int dy) {
  fprintf(stderr,"%s %+d,%+d\n",__func__,dx,dy);
}
 
static void hostio_cb_pcm_out(int16_t *v,int c,struct hostio_audio *driver) {
  memset(v,0,c<<1);//TODO synthesizer
}

static void hostio_cb_connect(struct hostio_input *driver,int devid) {
  fprintf(stderr,"%s %d\n",__func__,devid);
}

static void hostio_cb_disconnect(struct hostio_input *driver,int devid) {
  fprintf(stderr,"%s %d\n",__func__,devid);
}

static void hostio_cb_button(struct hostio_input *driver,int devid,int btnid,int value) {
  fprintf(stderr,"%s %d.%d=%d\n",__func__,devid,btnid,value);
}

/* Quit.
 */
 
void eggrt_drivers_quit() {
  hostio_del(eggrt.hostio);
  eggrt.hostio=0;
  if (eggrt.iconstorage) free(eggrt.iconstorage);
  eggrt.iconstorage=0;
}

/* Apply metadata field to video setup.
 * "title" is handled separately, not here.
 */
 
static int eggrt_video_setup_cb(const char *k,int kc,const char *v,int vc,void *userdata) {
  struct hostio_video_setup *setup=userdata;
  
  if ((kc==2)&&!memcmp(k,"fb",2)) {
    setup->fbw=setup->fbh=0;
    int *dst=&setup->fbw;
    int vp=0; for (;vp<vc;vp++) {
      if (v[vp]=='x') {
        if (dst==&setup->fbw) dst=&setup->fbh;
        else return -1;
      } else if ((v[vp]<'0')||(v[vp]>'9')) {
        return -1;
      } else {
        (*dst)*=10;
        (*dst)+=v[vp]-'0';
        if (*dst>4096) return -1;
      }
    }
    if (!setup->fbw||!setup->fbh) return -1;
    return 0;
  }
  
  if ((kc==9)&&!memcmp(k,"iconImage",9)) {
    int rid;
    if ((sr_int_eval(&rid,v,vc)<2)||(rid<1)||(rid>0xffff)) return 0;
    const void *serial=0;
    int serialc=eggrt_rom_get(&serial,EGG_TID_image,rid);
    if (serialc<1) return 0;
    struct image *image=image_decode(serial,serialc);
    if (!image) return 0;
    if (image_force_rgba(image)<0) {
      image_del(image);
      return 0;
    }
    if (eggrt.iconstorage) free(eggrt.iconstorage);
    eggrt.iconstorage=image->v;
    image->v=0;
    setup->iconrgba=eggrt.iconstorage;
    setup->iconw=image->w;
    setup->iconh=image->h;
    image_del(image);
    return 0;
  }
  
  return 0;
}

/* Init video and render.
 */
 
static int eggrt_drivers_init_video() {
  fprintf(stderr,"%s\n",__func__);
  struct hostio_video_setup setup={
    .title=eggrt.rptname,
    .w=eggrt.window_w,
    .h=eggrt.window_h,
    .fullscreen=eggrt.fullscreen,
    .device=eggrt.video_device,
  };
  
  char *titlestorage=0;
  { // Window's title is metadata:1:title, which is usually stringed.
    const char *titlesrc=0;
    int titlesrcc=rom_lookup_metadata(&titlesrc,eggrt.romserial,eggrt.romserialc,"title",5,eggrt.lang);
    if (titlesrcc>0) {
      if (titlestorage=malloc(titlesrcc+1)) {
        memcpy(titlestorage,titlesrc,titlesrcc);
        titlestorage[titlesrcc]=0;
        setup.title=titlestorage;
      }
    }
  }
  
  { // Search metadata for "iconImage" and "fb", which are not stringed.
    const void *metadata=0;
    int metadatac=eggrt_rom_get(&metadata,EGG_TID_metadata,1);
    if (rom_read_metadata(metadata,metadatac,eggrt_video_setup_cb,&setup)<0) return -1;
  }
  if (!setup.fbw||!setup.fbh) {
    fprintf(stderr,"%s: Framebuffer size not defined by ROM.\n",eggrt.rptname);
    return -2;
  }
  
  // Bring video driver up.
  int err=hostio_init_video(eggrt.hostio,eggrt.video_drivers,&setup);
  if (titlestorage) free(titlestorage);
  if (err<0) return -1;
  
  //TODO Render.
  return 0;
}

/* Init audio and synth.
 */
 
static int eggrt_drivers_init_audio() {
  fprintf(stderr,"%s\n",__func__);
  struct hostio_audio_setup setup={
    .rate=eggrt.audio_rate,
    .chanc=eggrt.audio_chanc,
    .device=eggrt.audio_device,
    .buffer_size=eggrt.audio_buffer,
  };
  if (hostio_init_audio(eggrt.hostio,eggrt.audio_drivers,&setup)<0) return -1;
  //TODO Synth.
  return 0;
}

/* Init input driver and manager.
 */
 
static int eggrt_drivers_init_input() {
  fprintf(stderr,"%s\n",__func__);
  struct hostio_input_setup setup={0};
  if (hostio_init_input(eggrt.hostio,eggrt.input_drivers,&setup)<0) return -1;
  //TODO Input Manager.
  return 0;
}

/* Init.
 */
 
int eggrt_drivers_init() {
  int err;
  struct hostio_video_delegate vdel={
    .cb_close=hostio_cb_close,
    .cb_focus=hostio_cb_focus,
    .cb_resize=hostio_cb_resize,
    .cb_key=hostio_cb_key,
    .cb_text=hostio_cb_text,
    .cb_mmotion=hostio_cb_mmotion,
    .cb_mbutton=hostio_cb_mbutton,
    .cb_mwheel=hostio_cb_mwheel,
  };
  struct hostio_audio_delegate adel={
    .cb_pcm_out=hostio_cb_pcm_out,
  };
  struct hostio_input_delegate idel={
    .cb_connect=hostio_cb_connect,
    .cb_disconnect=hostio_cb_disconnect,
    .cb_button=hostio_cb_button,
  };
  if (!(eggrt.hostio=hostio_new(&vdel,&adel,&idel))) return -1;
  if ((err=eggrt_drivers_init_video())<0) return err;
  if ((err=eggrt_drivers_init_audio())<0) return err;
  if ((err=eggrt_drivers_init_input())<0) return err;
  hostio_log_driver_names(eggrt.hostio);
  return 0;
}

/* Update.
 */
 
int eggrt_drivers_update() {
  return hostio_update(eggrt.hostio);
}
