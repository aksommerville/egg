/* eggrt_drivers.c
 * Interface to hostio, which does the heavier lifting for IO abstraction.
 * We are also responsible for the abstract layer on top of that: Render, Synth, Input Manager.
 */

#include "eggrt_internal.h"
#include "opt/image/image.h"

/* Hostio callbacks.
 */
 
static void hostio_cb_close(struct hostio_video *driver) {
  eggrt.terminate++;
}

static void hostio_cb_focus(struct hostio_video *driver,int focus) {
  eggrt.focus=focus;
}

static void hostio_cb_resize(struct hostio_video *driver,int w,int h) {
  // I think we don't care. We need video size for the final transfer, but we poll for that.
}

static int hostio_cb_key(struct hostio_video *driver,int keycode,int value) {
  inmgr_event_key(eggrt.inmgr,keycode,value);
  return 1;
}
 
static void hostio_cb_pcm_out(int16_t *v,int c,struct hostio_audio *driver) {
  if (eggrt.synth) synth_updatei(v,c,eggrt.synth);
  else memset(v,0,c<<1);
}

static void hostio_cb_connect(struct hostio_input *driver,int devid) {
  inmgr_event_connect(eggrt.inmgr,driver,devid);
}

static void hostio_cb_disconnect(struct hostio_input *driver,int devid) {
  inmgr_event_disconnect(eggrt.inmgr,driver,devid);
}

static void hostio_cb_button(struct hostio_input *driver,int devid,int btnid,int value) {
  inmgr_event_button(eggrt.inmgr,driver,devid,btnid,value);
}

/* Quit.
 */
 
void eggrt_drivers_quit() {
  if (hostio_audio_lock(eggrt.hostio)>=0) {
    hostio_audio_play(eggrt.hostio,0);
    hostio_audio_unlock(eggrt.hostio);
  }
  render_del(eggrt.render); eggrt.render=0;
  inmgr_del(eggrt.inmgr); eggrt.inmgr=0;
  hostio_del(eggrt.hostio); eggrt.hostio=0;
  synth_del(eggrt.synth); eggrt.synth=0;
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
  struct hostio_video_setup setup={
    .title=eggrt.rptname,
    .w=eggrt.window_w,
    .h=eggrt.window_h,
    .fullscreen=eggrt.fullscreen,
    .device=eggrt.video_device,
  };
  
  char *titlestorage=0;
  if (!eggrt.romserial&&eggrt.configure_input) {
    // --configure-input with no ROM file. Make up some geometry.
    setup.fbw=INCFG_FBW;
    setup.fbh=INCFG_FBH;
  } else {
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
  }
  eggrt.fbw=setup.fbw;
  eggrt.fbh=setup.fbh;
  eggrt.focus=1;
  
  // Bring video driver up.
  int err=hostio_init_video(eggrt.hostio,eggrt.video_drivers,&setup);
  if (titlestorage) free(titlestorage);
  if (err<0) return -1;
  if (!eggrt.hostio->video->type->gx_begin||!eggrt.hostio->video->type->gx_end) {
    fprintf(stderr,"%s: Video driver '%s' must implement both GX fences.\n",eggrt.exename,eggrt.hostio->video->type->name);
    return -2;
  }
  
  // Create renderer.
  if (!(eggrt.render=render_new())) {
    fprintf(stderr,"%s: Failed to initialize OpenGL.\n",eggrt.exename);
    return -2;
  }
  if (render_texture_require(eggrt.render,1)<0) return -1;
  if (render_texture_load(eggrt.render,1,setup.fbw,setup.fbh,setup.fbw<<2,EGG_TEX_FMT_RGBA,0,0)<0) return -1;

  return 0;
}

/* Init audio and synth.
 */
 
static int eggrt_drivers_init_audio() {
  struct hostio_audio_setup setup={
    .rate=eggrt.audio_rate,
    .chanc=eggrt.audio_chanc,
    .device=eggrt.audio_device,
    .buffer_size=eggrt.audio_buffer,
  };
  if (hostio_init_audio(eggrt.hostio,eggrt.audio_drivers,&setup)<0) return -1;
  if (!(eggrt.synth=synth_new(eggrt.hostio->audio->rate,eggrt.hostio->audio->chanc))) {
    fprintf(stderr,"%s: Failed to initialize synthesizer.\n",eggrt.exename);
    return -2;
  }
  
  const struct rom_res *res=eggrt.resv;
  int i=eggrt.resc;
  for (;i-->0;res++) {
    if (res->tid==EGG_TID_sound) {
      if (synth_install_sound(eggrt.synth,res->rid,res->v,res->c)<0) return -1;
    } else if (res->tid==EGG_TID_song) {
      if (synth_install_song(eggrt.synth,res->rid,res->v,res->c)<0) return -1;
    } else if ((res->tid>EGG_TID_sound)&&(res->tid>EGG_TID_song)) {
      break;
    }
  }
  
  return 0;
}

/* Init input driver and manager.
 */
 
static int eggrt_drivers_init_input() {
  struct hostio_input_setup setup={0};
  if (hostio_init_input(eggrt.hostio,eggrt.input_drivers,&setup)<0) return -1;
  if (!(eggrt.inmgr=inmgr_new())) return -1;
  
  if (eggrt.hostio->video&&eggrt.hostio->video->type->provides_input) {
    if (inmgr_enable_system_keyboard(eggrt.inmgr,1)<0) return -1;
  }
  
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
  int err;
  if ((err=hostio_update(eggrt.hostio))<0) return -1;
  if ((err=inmgr_update(eggrt.inmgr))<0) return -1;
  return 0;
}
