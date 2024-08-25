#include "eggrt_internal.h"

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
uint32_t egg_get_event_mask();
void egg_set_event_mask(uint32_t mask);
int egg_show_cursor(int show);
int egg_lock_cursor(int lock);
int egg_input_device_get_name(char *dst,int dsta,int devid);
void egg_input_device_get_ids(int *vid,int *pid,int *version,int devid);
int egg_input_device_devid_by_index(int p);
void egg_audio_set_limit(int samplec);
void egg_play_sound(int rid,int index);
void egg_play_song(int rid,int force,int repeat);
void egg_play_sound_binary(const void *src,int srcc);
void egg_play_song_binary(const void *src,int srcc,int force,int repeat);
void egg_audio_event(int chid,int opcode,int a,int b,int durms);
double egg_audio_get_playhead();
void egg_audio_set_playhead(double s);
void egg_texture_del(int texid);
int egg_texture_new();
int egg_texture_get_status(int *w,int *h,int texid);
int egg_texture_get_pixels(void *dst,int dsta,int texid);
int egg_texture_load_image(int texid,int rid);
int egg_texture_load_serial(int texid,const void *src,int srcc);
int egg_texture_load_raw(int texid,int fmt,int w,int h,int stride,const void *src,int srcc);
void egg_draw_clear(int dsttexid,uint32_t rgba);
void egg_draw_line(int dsttexid,const struct egg_draw_line *v,int c);
void egg_draw_rect(int dsttexid,const struct egg_draw_rect *v,int c);
void egg_draw_trig(int dsttexid,const struct egg_draw_trig *v,int c);
void egg_draw_decal(int dsttexid,int srctexid,const struct egg_draw_decal *v,int c);
void egg_draw_tile(int dsttexid,int srctexid,const struct egg_draw_tile *v,int c);
void egg_draw_mode7(int dsttexid,int srctexid,const struct egg_draw_mode7 *v,int c);
/**/

/* Log.
 */
 
void egg_log(const char *msg) {
  int msgc=0;
  if (msg) while (msg[msgc]) msgc++;
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
  fprintf(stderr,"TODO %s [%s:%d]\n",__func__,__FILE__,__LINE__);
  return 0.0;
}

/* Current local time, split for display.
 */
 
void egg_time_local(int *dstv,int dsta) {
  fprintf(stderr,"TODO %s [%s:%d]\n",__func__,__FILE__,__LINE__);
}

/* Language
 */
 
int egg_get_language() {
  return eggrt.lang;
}

/* Set language.
 */
 
void egg_set_language(int lang) {
  fprintf(stderr,"TODO %s [%s:%d] %d\n",__func__,__FILE__,__LINE__,lang);
  //TODO Not just setting eggrt.lang -- must update eg window title, maybe other stuff
}

/* ROM.
 */

int egg_get_rom(void *dst,int dsta) {
  if (dst&&(dsta>=eggrt.romserialc)) {
    memcpy(dst,eggrt.romserial,eggrt.romserialc);
  }
  return eggrt.romserialc;
}

/* Get store field.
 */
 
int egg_store_get(char *v,int va,const char *k,int kc) {
  fprintf(stderr,"TODO %s [%s:%d]\n",__func__,__FILE__,__LINE__);
  return 0;
}

/* Set store field.
 */
 
int egg_store_set(const char *k,int kc,const char *v,int vc) {
  fprintf(stderr,"TODO %s [%s:%d]\n",__func__,__FILE__,__LINE__);
  return 0;
}

/* List store fields.
 */
 
int egg_store_key_by_index(char *k,int ka,int p) {
  fprintf(stderr,"TODO %s [%s:%d]\n",__func__,__FILE__,__LINE__);
  return 0;
}

/* Event mask.
 */

uint32_t egg_get_event_mask() {
  fprintf(stderr,"TODO %s [%s:%d]\n",__func__,__FILE__,__LINE__);
  return 0;
}
 
void egg_set_event_mask(uint32_t mask) {
  fprintf(stderr,"TODO %s [%s:%d]\n",__func__,__FILE__,__LINE__);
  //TODO It's not just setting the field. Work with input manager to enable/disable features.
}

/* Show cursor.
 */
 
int egg_show_cursor(int show) {
  fprintf(stderr,"TODO %s [%s:%d]\n",__func__,__FILE__,__LINE__);
  return 0;
}

/* Lock cursor.
 */
 
int egg_lock_cursor(int lock) {
  fprintf(stderr,"TODO %s [%s:%d]\n",__func__,__FILE__,__LINE__);
  return 0;
}

/* Input device name.
 */
 
int egg_input_device_get_name(char *dst,int dsta,int devid) {
  fprintf(stderr,"TODO %s [%s:%d]\n",__func__,__FILE__,__LINE__);
  return 0;
}

/* Input device IDs.
 */
 
void egg_input_device_get_ids(int *vid,int *pid,int *version,int devid) {
  fprintf(stderr,"TODO %s [%s:%d]\n",__func__,__FILE__,__LINE__);
}

/* List input devices.
 */
 
int egg_input_device_devid_by_index(int p) {
  fprintf(stderr,"TODO %s [%s:%d]\n",__func__,__FILE__,__LINE__);
  return 0;
}

/* Audio limit.
 */

void egg_audio_set_limit(int samplec) {
  fprintf(stderr,"TODO %s [%s:%d]\n",__func__,__FILE__,__LINE__);
}

/* Play sound from resource.
 */
 
void egg_play_sound(int rid,int index) {
  fprintf(stderr,"TODO %s [%s:%d]\n",__func__,__FILE__,__LINE__);
}

/* Play song from resource.
 */
 
void egg_play_song(int rid,int force,int repeat) {
  fprintf(stderr,"TODO %s [%s:%d]\n",__func__,__FILE__,__LINE__);
}

/* Play sound from serial data.
 */
 
void egg_play_sound_binary(const void *src,int srcc) {
  fprintf(stderr,"TODO %s [%s:%d]\n",__func__,__FILE__,__LINE__);
}

/* Play song from serial data.
 */
 
void egg_play_song_binary(const void *src,int srcc,int force,int repeat) {
  fprintf(stderr,"TODO %s [%s:%d]\n",__func__,__FILE__,__LINE__);
}

/* Generic audio event.
 */
 
void egg_audio_event(int chid,int opcode,int a,int b,int durms) {
  fprintf(stderr,"TODO %s [%s:%d]\n",__func__,__FILE__,__LINE__);
}

/* Audio playhead.
 */
 
double egg_audio_get_playhead() {
  return 0.0;
}

void egg_audio_set_playhead(double s) {
  fprintf(stderr,"TODO %s [%s:%d]\n",__func__,__FILE__,__LINE__);
}

/* Textures.
 */
 
void egg_texture_del(int texid) {
  fprintf(stderr,"TODO %s [%s:%d]\n",__func__,__FILE__,__LINE__);
}

int egg_texture_new() {
  fprintf(stderr,"TODO %s [%s:%d]\n",__func__,__FILE__,__LINE__);
  return -1;
}

int egg_texture_get_status(int *w,int *h,int texid) {
  fprintf(stderr,"TODO %s [%s:%d]\n",__func__,__FILE__,__LINE__);
  return -1;
}

int egg_texture_get_pixels(void *dst,int dsta,int texid) {
  fprintf(stderr,"TODO %s [%s:%d]\n",__func__,__FILE__,__LINE__);
  return 0;
}

/* Load content to texture.
 */

int egg_texture_load_image(int texid,int rid) {
  fprintf(stderr,"TODO %s [%s:%d]\n",__func__,__FILE__,__LINE__);
  return -1;
}

int egg_texture_load_serial(int texid,const void *src,int srcc) {
  fprintf(stderr,"TODO %s [%s:%d]\n",__func__,__FILE__,__LINE__);
  return -1;
}

int egg_texture_load_raw(int texid,int fmt,int w,int h,int stride,const void *src,int srcc) {
  fprintf(stderr,"TODO %s [%s:%d]\n",__func__,__FILE__,__LINE__);
  return -1;
}

/* Rendering.
 */
 
void egg_draw_clear(int dsttexid,uint32_t rgba) {
  fprintf(stderr,"TODO %s [%s:%d]\n",__func__,__FILE__,__LINE__);
}

void egg_draw_line(int dsttexid,const struct egg_draw_line *v,int c) {
  fprintf(stderr,"TODO %s [%s:%d]\n",__func__,__FILE__,__LINE__);
}

void egg_draw_rect(int dsttexid,const struct egg_draw_rect *v,int c) {
  fprintf(stderr,"TODO %s [%s:%d]\n",__func__,__FILE__,__LINE__);
}

void egg_draw_trig(int dsttexid,const struct egg_draw_trig *v,int c) {
  fprintf(stderr,"TODO %s [%s:%d]\n",__func__,__FILE__,__LINE__);
}

void egg_draw_decal(int dsttexid,int srctexid,const struct egg_draw_decal *v,int c) {
  fprintf(stderr,"TODO %s [%s:%d]\n",__func__,__FILE__,__LINE__);
}

void egg_draw_tile(int dsttexid,int srctexid,const struct egg_draw_tile *v,int c) {
  fprintf(stderr,"TODO %s [%s:%d]\n",__func__,__FILE__,__LINE__);
}

void egg_draw_mode7(int dsttexid,int srctexid,const struct egg_draw_mode7 *v,int c) {
  fprintf(stderr,"TODO %s [%s:%d]\n",__func__,__FILE__,__LINE__);
}
