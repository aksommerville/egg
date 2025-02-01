#include "eggrt_internal.h"
#include "opt/image/image.h"
#include <time.h>
#include <sys/time.h>

/* For reference, here's all the functions of the Egg Platform API.
 * Structs and constants, refer to src/egg/egg.h.
 *
void egg_log(const char *msg);
void egg_terminate(int status);
double egg_time_real();
void egg_time_local(int *dstv,int dsta);
int egg_get_language();
void egg_set_language(int lang);
int egg_get_rom(void *dst,int dsta);
int egg_store_get(char *v,int va,const char *k,int kc);
int egg_store_set(const char *k,int kc,const char *v,int vc);
int egg_store_key_by_index(char *k,int ka,int p);
int egg_input_get_one(int playerid);
int egg_input_get_all(int *dst,int dsta);
int egg_input_configure();
void egg_play_sound(int rid);
void egg_play_song(int rid,int force,int repeat);
void egg_audio_event(int chid,int opcode,int a,int b,int durms);
double egg_audio_get_playhead();
void egg_audio_set_playhead(double s);
void egg_texture_del(int texid);
int egg_texture_new();
int egg_texture_get_status(int *w,int *h,int texid);
int egg_texture_get_pixels(void *dst,int dsta,int texid);
int egg_texture_load_image(int texid,int rid);
int egg_texture_load_raw(int texid,int fmt,int w,int h,int stride,const void *src,int srcc);
void egg_draw_globals(int tint,int alpha);
void egg_draw_clear(int dsttexid,uint32_t rgba);
void egg_draw_line(int dsttexid,const struct egg_draw_line *v,int c);
void egg_draw_rect(int dsttexid,const struct egg_draw_rect *v,int c);
void egg_draw_trig(int dsttexid,const struct egg_draw_trig *v,int c);
void egg_draw_decal(int dsttexid,int srctexid,const struct egg_draw_decal *v,int c);
void egg_draw_tile(int dsttexid,int srctexid,const struct egg_draw_tile *v,int c);
void egg_draw_mode7(int dsttexid,int srctexid,const struct egg_draw_mode7 *v,int c,int interpolate);
/**/

/* Log.
 */
 
void egg_log(const char *msg) {
  int msgc=0;
  if (msg) while ((msgc<256)&&msg[msgc]) msgc++;
  while (msgc&&((unsigned char)msg[msgc-1]<=0x20)) msgc--;
  fprintf(stderr,"GAME: %.*s\n",msgc,msg);
}

/* Terminate.
 */
 
void egg_terminate(int status) {
  fprintf(stderr,"%s: Terminating with status %d per game.\n",eggrt.rptname,status);
  eggrt.terminate=1;
  eggrt.exitstatus=status;
}

/* Current real time.
 */
 
double egg_time_real() {
  struct timeval tv={0};
  gettimeofday(&tv,0);
  return (double)tv.tv_sec+(double)tv.tv_usec/1000000.0;
}

/* Current local time, split for display.
 */
 
void egg_time_local(int *dstv,int dsta) {
  if (!dstv||(dsta<1)) return;
  time_t now=time(0);
  struct tm *tm=localtime(&now);
  if (!tm) return;
  dstv[0]=1900+tm->tm_year; if (dsta<2) return;
  dstv[1]=1+tm->tm_mon; if (dsta<3) return;
  dstv[2]=tm->tm_mday; if (dsta<4) return;
  dstv[3]=tm->tm_hour; if (dsta<5) return;
  dstv[4]=tm->tm_min; if (dsta<6) return;
  dstv[5]=tm->tm_sec; if (dsta<7) return;
  struct timeval tv={0};
  gettimeofday(&tv,0);
  dstv[6]=(tv.tv_usec/1000)%1000;
}

/* Language
 */
 
int egg_get_language() {
  return eggrt.lang;
}

/* Set language.
 */
 
void egg_set_language(int lang) {
  if (lang==eggrt.lang) return;
  if (lang&~0x3ff) return;
  int hi=lang>>5;
  int lo=lang&0x1f;
  if ((hi>=26)||(lo>=26)) return;
  char ostr[2],nstr[2];
  EGG_STRING_FROM_LANG(ostr,eggrt.lang)
  EGG_STRING_FROM_LANG(nstr,lang)
  fprintf(stderr,"%s: Changing lang from %d (%.2s) to %d (%.2s) per game request.\n",eggrt.exename,eggrt.lang,ostr,lang,nstr);
  eggrt.lang=lang;
  
  // Update window title.
  if (eggrt.hostio->video->type->set_title) {
    const char *titlesrc=0;
    int titlesrcc=rom_lookup_metadata(&titlesrc,eggrt.romserial,eggrt.romserialc,"title",5,eggrt.lang);
    if (titlesrcc>0) {
      char *ztitle=malloc(titlesrcc+1);
      if (ztitle) {
        memcpy(ztitle,titlesrc,titlesrcc);
        ztitle[titlesrcc]=0;
        eggrt.hostio->video->type->set_title(eggrt.hostio->video,ztitle);
        free(ztitle);
      }
    }
  }

  //TODO Other state to update?
}

/* ROM and store.
 */

int egg_get_rom(void *dst,int dsta) {
  if (dst&&(dsta>=eggrt.romserialc)) {
    memcpy(dst,eggrt.romserial,eggrt.romserialc);
  }
  return eggrt.romserialc;
}
 
int egg_store_get(char *v,int va,const char *k,int kc) {
  if (!v) va=0;
  const struct eggrt_store_field *field=eggrt_store_get_field(k,kc,0);
  if (!field) {
    if (va>0) v[0]=0;
    return 0;
  }
  if (field->vc<=va) {
    memcpy(v,field->v,field->vc);
    if (field->vc<va) v[field->vc]=0;
  }
  return field->vc;
}
 
int egg_store_set(const char *k,int kc,const char *v,int vc) {
  struct eggrt_store_field *field=eggrt_store_get_field(k,kc,1);
  if (!field) return -1;
  if (eggrt_store_set_field(field,v,vc)<0) return -1;
  return 0;
}
 
int egg_store_key_by_index(char *k,int ka,int p) {
  if (!k) ka=0;
  const char *src=0;
  int srcc=0;
  if ((p>=0)&&(p<eggrt.storec)) {
    src=eggrt.storev[p].k;
    srcc=eggrt.storev[p].kc;
  }
  if (srcc<=ka) {
    memcpy(k,src,srcc);
    if (srcc<ka) k[srcc]=0;
  }
  return srcc;
}

/* Input.
 */
 
int egg_input_get_one(int playerid) {
  if ((playerid<0)||(playerid>=eggrt.inmgr->playerc)) return 0;
  return eggrt.inmgr->playerv[playerid];
}

int egg_input_get_all(int *dst,int dsta) {
  if (!dst||(dsta<1)) return 0;
  int dstc=eggrt.inmgr->playerc;
  if (dstc>dsta) dstc=dsta;
  memcpy(dst,eggrt.inmgr->playerv,sizeof(int)*dstc);
  return dstc;
}

int egg_input_configure() {
  if (eggrt.incfg) return 0; // Already doing it. How did we get called?
  if (!(eggrt.incfg=incfg_new())) return -1;
  return 0;
}

/* Play sound from resource.
 */
 
void egg_play_sound(int rid) {
  if (hostio_audio_lock(eggrt.hostio)>=0) {
    synth_play_sound(eggrt.synth,rid);
    hostio_audio_unlock(eggrt.hostio);
  }
}

/* Play song from resource.
 */
 
void egg_play_song(int rid,int force,int repeat) {
  if (hostio_audio_lock(eggrt.hostio)>=0) {
    synth_play_song(eggrt.synth,rid,force,repeat);
    hostio_audio_unlock(eggrt.hostio);
  }
}

/* Generic audio event.
 */
 
void egg_audio_event(int chid,int opcode,int a,int b,int durms) {
  if (hostio_audio_lock(eggrt.hostio)>=0) {
    synth_event(eggrt.synth,chid,opcode,a,b,durms);
    hostio_audio_unlock(eggrt.hostio);
  }
}

/* Audio playhead.
 */
 
double egg_audio_get_playhead() {
  if (!eggrt.synth) return 0.0;
  // No need to lock.
  double s=synth_get_playhead(eggrt.synth);
  if (s<=0.0) return s;
  s-=hostio_audio_estimate_remaining_buffer(eggrt.hostio->audio);
  if (s<=0.0) return 0.001; // This is entirely possible, when it loops. We ought to add the song's length instead.
  return s;
}

void egg_audio_set_playhead(double s) {
  if (hostio_audio_lock(eggrt.hostio)>=0) {
    synth_set_playhead(eggrt.synth,s);
    hostio_audio_unlock(eggrt.hostio);
  }
}

/* Textures.
 */
 
void egg_texture_del(int texid) {
  render_texture_del(eggrt.render,texid);
}

int egg_texture_new() {
  return render_texture_new(eggrt.render);
}

int egg_texture_get_status(int *w,int *h,int texid) {
  int fmt=0;
  render_texture_get_header(w,h,&fmt,eggrt.render,texid);
  if (fmt>=0) return 1;
  return -1;
}

int egg_texture_get_pixels(void *dst,int dsta,int texid) {
  return render_texture_get_pixels(dst,dsta,eggrt.render,texid);
}

/* Load content to texture.
 */

int egg_texture_load_image(int texid,int rid) {
  const void *src=0;
  int srcc=eggrt_rom_get(&src,EGG_TID_image,rid);
  struct image *image=image_decode(src,srcc);
  if (!image) return -1;
  int fmt=EGG_TEX_FMT_RGBA;
  switch (image->pixelsize) {
    case 1: fmt=EGG_TEX_FMT_A1; break;
    case 8: fmt=EGG_TEX_FMT_A8; break;
    case 32: break;
    default: {
        if (image_force_rgba(image)<0) {
          image_del(image);
          return -1;
        }
      }
  }
  int err=egg_texture_load_raw(texid,fmt,image->w,image->h,image->stride,image->v,image->stride*image->h);
  image_del(image);
  return err;
}

int egg_texture_load_raw(int texid,int fmt,int w,int h,int stride,const void *src,int srcc) {
  return render_texture_load(eggrt.render,texid,w,h,stride,fmt,src,srcc);
}

/* Rendering.
 */
 
void egg_draw_globals(int tint,int alpha) {
  render_tint(eggrt.render,tint);
  render_alpha(eggrt.render,alpha);
}
 
void egg_draw_clear(int dsttexid,uint32_t rgba) {
  if (rgba) {
    struct egg_draw_rect vtx={
      .x=0,.y=0,.w=4096,.h=4096,
      .r=rgba>>24,.g=rgba>>16,.b=rgba>>8,.a=rgba,
    };
    render_draw_rect(eggrt.render,dsttexid,&vtx,1);
  } else {
    render_texture_clear(eggrt.render,dsttexid);
  }
}

void egg_draw_line(int dsttexid,const struct egg_draw_line *v,int c) {
  render_draw_line(eggrt.render,dsttexid,v,c);
}

void egg_draw_rect(int dsttexid,const struct egg_draw_rect *v,int c) {
  render_draw_rect(eggrt.render,dsttexid,v,c);
}

void egg_draw_trig(int dsttexid,const struct egg_draw_trig *v,int c) {
  render_draw_trig(eggrt.render,dsttexid,v,c);
}

void egg_draw_decal(int dsttexid,int srctexid,const struct egg_draw_decal *v,int c) {
  render_draw_decal(eggrt.render,dsttexid,srctexid,v,c);
}

void egg_draw_tile(int dsttexid,int srctexid,const struct egg_draw_tile *v,int c) {
  render_draw_tile(eggrt.render,dsttexid,srctexid,v,c);
}

void egg_draw_mode7(int dsttexid,int srctexid,const struct egg_draw_mode7 *v,int c,int interpolate) {
  render_draw_mode7(eggrt.render,dsttexid,srctexid,v,c,interpolate);
}
