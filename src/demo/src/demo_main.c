#include "egg/egg.h"
#include "opt/stdlib/egg-stdlib.h"

/* Call all 39 functions of the Egg Platform API, to ensure everything is hooked up.
 *******************************************************************/
 
static void test_full_api() {

  // System utilities.
  egg_log("This string is emitted via egg_log. I'm not including a newline.");
  //egg_terminate(123); // Works but inconvenient to test :)
  fprintf(stderr,"egg_time_real: %f\n",egg_time_real());
  {
    int v[10]={0};
    egg_time_local(v,10);
    fprintf(stderr,"egg_time_local: %.4d-%.2d-%.2dT%.2d:%.2d:%.2d:%.3d\n",v[0],v[1],v[2],v[3],v[4],v[5],v[6]);
  }
  {
    int lang=egg_get_language();
    char rlang[2]={0};
    EGG_STRING_FROM_LANG(rlang,lang)
    fprintf(stderr,"egg_get_language: %d (%.2s)\n",lang,rlang);
    egg_set_language(EGG_LANG_FROM_STRING("fr"));
  }
  
  // Storage.
  {
    uint8_t rom[4]; // won't be long enough.
    int romc=egg_get_rom(&rom,sizeof(rom));
    fprintf(stderr,"egg_get_rom(): %d\n",romc);
  }
  {
    char tmp[256];
    int tmpc=egg_store_get(tmp,sizeof(tmp),"myPersistentField",17);
    fprintf(stderr,"egg_store_get('myPersistentField'): '%.*s'\n",tmpc,tmp);
    int err=egg_store_set("myPersistentField",17,"Some Value",10);
    fprintf(stderr,"egg_store_set: %d\n",err);
    int p=0; for (;;p++) {
      if ((tmpc=egg_store_key_by_index(tmp,sizeof(tmp),p))<1) break;
      if (tmpc>sizeof(tmp)) fprintf(stderr,"egg_store_key_by_index(%d) Too Long! %d\n",p,tmpc);
      else fprintf(stderr,"egg_store_key_by_index(%d): '%.*s'\n",p,tmpc,tmp);
    }
  }
  
  // Input.
  fprintf(stderr,"egg_get_event_mask(): %#.8x\n",egg_get_event_mask());
  egg_set_event_mask(0xffffffff);
  fprintf(stderr,"egg_show_cursor(1): %d\n",egg_show_cursor(1));
  fprintf(stderr,"egg_lock_cursor(1): %d\n",egg_lock_cursor(1));
  {
    int p=0; for (;;p++) {
      int devid=egg_input_device_devid_by_index(p);
      if (devid<=0) break;
      fprintf(stderr,"egg_input_device_devid_by_index(%d): %d\n",p,devid);
      char name[256];
      int namec=egg_input_device_get_name(name,sizeof(name),devid);
      if ((namec<1)||(namec>sizeof(name))) namec=0;
      int vid=0,pid=0,version=0;
      egg_input_device_get_ids(&vid,&pid,&version,devid);
      fprintf(stderr,"...%#.4x:%#.4x:%#.4x '%.*s'\n",vid,pid,version,namec,name);
    }
  }
  
  // Audio.
  egg_play_sound(1,2);
  egg_play_song(1,0,1);
  egg_play_sound_binary(0,0);
  egg_play_song_binary(0,0,0,1);
  egg_audio_event(0,0x90,0x48,0x40,500);
  fprintf(stderr,"egg_audio_get_playhead(): %f\n",egg_audio_get_playhead());
  egg_audio_set_playhead(4.0);
  
  // Video.
  /* TODO test video
void egg_texture_del(int texid);
int egg_texture_new();
int egg_texture_get_status(int *w,int *h,int texid);
int egg_texture_get_pixels(void *dst,int dsta,int texid);
int egg_texture_load_image(int texid,int rid);
int egg_texture_load_serial(int texid,const void *src,int srcc);
int egg_texture_load_raw(int texid,int fmt,int w,int h,int stride,const void *src,int srcc);
  */
  /* These won't work here. Test all during egg_client_render():
void egg_draw_clear(int dsttexid,uint32_t rgba);
void egg_draw_line(int dsttexid,const struct egg_draw_line *v,int c);
void egg_draw_rect(int dsttexid,const struct egg_draw_rect *v,int c);
void egg_draw_trig(int dsttexid,const struct egg_draw_trig *v,int c);
void egg_draw_decal(int dsttexid,int srctexid,const struct egg_draw_decal *v,int c);
void egg_draw_tile(int dsttexid,int srctexid,const struct egg_draw_tile *v,int c);
void egg_draw_mode7(int dsttexid,int srctexid,const struct egg_draw_mode7 *v,int c);
  */
}

/* Entry points.
 **************************************************************/

void egg_client_quit(int status) {
  egg_log(__func__);
}

int egg_client_init() {
  fprintf(stderr,"%d function %s was called, around %s:%d. %d!\n",123,__func__,__FILE__,__LINE__,789);
  test_full_api();
  return 0;
}

void egg_client_update(double elapsed) {
  //egg_log(__func__);
}

void egg_client_render() {
  //egg_log(__func__);
}
