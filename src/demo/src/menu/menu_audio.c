#include "../demo.h"
#include "../ui/picklist.h"

#define OPTID_SONG 1
#define OPTID_FORCE 2
#define OPTID_REPEAT 3
#define OPTID_SOUND 4

struct menu_audio {
  struct menu hdr;
  struct picklist *picklist;
  int *songidv,*soundidv;
  int songidc,songida,soundidc,soundida;
  int songidp,soundidp;
  int force,repeat;
};

#define MENU ((struct menu_audio*)menu)

/* Delete.
 */
 
static void _audio_del(struct menu *menu) {
  picklist_del(MENU->picklist);
  if (MENU->songidv) free(MENU->songidv);
  if (MENU->soundidv) free(MENU->soundidv);
}

/* Input.
 */
 
static void audio_rewrite_song_label(struct menu *menu) {
  char tmp[32];
  memcpy(tmp,"Song ",5);
  int tmpc=5;
  if ((MENU->songidp>=0)&&(MENU->songidp<MENU->songidc)) {
    int rid=MENU->songidv[MENU->songidp];
    if (rid>=10000) tmp[tmpc++]='0'+(rid/10000)%10;
    if (rid>=1000 ) tmp[tmpc++]='0'+(rid/1000 )%10;
    if (rid>=100  ) tmp[tmpc++]='0'+(rid/100  )%10;
    if (rid>=10   ) tmp[tmpc++]='0'+(rid/10   )%10;
    tmp[tmpc++]='0'+rid%10;
  }
  picklist_replace_label(MENU->picklist,OPTID_SONG,tmp,tmpc);
}
 
static void audio_rewrite_sound_label(struct menu *menu) {
  char tmp[32];
  memcpy(tmp,"Sound ",6);
  int tmpc=6;
  if ((MENU->soundidp>=0)&&(MENU->soundidp<MENU->soundidc)) {
    int rid=MENU->soundidv[MENU->soundidp];
    if (rid>=10000) tmp[tmpc++]='0'+(rid/10000)%10;
    if (rid>=1000 ) tmp[tmpc++]='0'+(rid/1000 )%10;
    if (rid>=100  ) tmp[tmpc++]='0'+(rid/100  )%10;
    if (rid>=10   ) tmp[tmpc++]='0'+(rid/10   )%10;
    tmp[tmpc++]='0'+rid%10;
  }
  picklist_replace_label(MENU->picklist,OPTID_SOUND,tmp,tmpc);
}
 
static void audio_adjust(struct menu *menu,int d) {
  switch (picklist_get_optid(MENU->picklist)) {
  
    case OPTID_SONG: {
        if (MENU->songidc<1) return;
        MENU->songidp+=d;
        if (MENU->songidp<0) MENU->songidp=MENU->songidc-1;
        else if (MENU->songidp>=MENU->songidc) MENU->songidp=0;
        audio_rewrite_song_label(menu);
      } break;
      
    case OPTID_SOUND: {
        if (MENU->soundidc<1) return;
        MENU->soundidp+=d;
        if (MENU->soundidp<0) MENU->soundidp=MENU->soundidc-1;
        else if (MENU->soundidp>=MENU->soundidc) MENU->soundidp=0;
        audio_rewrite_sound_label(menu);
      } break;
  }
}
 
static void _audio_input(struct menu *menu,int input,int pvinput) {
  if ((input&EGG_BTN_WEST)&&!(pvinput&EGG_BTN_WEST)) menu->defunct=1;
  if ((input&EGG_BTN_LEFT)&&!(pvinput&EGG_BTN_LEFT)) audio_adjust(menu,-1);
  if ((input&EGG_BTN_RIGHT)&&!(pvinput&EGG_BTN_RIGHT)) audio_adjust(menu,1);
  picklist_input(MENU->picklist,input,pvinput);
}

/* Update.
 */
 
static void _audio_update(struct menu *menu,double elapsed) {
  picklist_update(MENU->picklist,elapsed);
}

/* Render.
 */
 
static void _audio_render(struct menu *menu) {
  picklist_render(MENU->picklist);
}

/* Read the ROM and populate (songidv,soundidv).
 */
 
static int audio_add_songid(struct menu *menu,int rid) {
  if (MENU->songidc>=MENU->songida) {
    int na=MENU->songida+32;
    if (na>INT_MAX/sizeof(int)) return -1;
    void *nv=realloc(MENU->songidv,sizeof(int)*na);
    if (!nv) return -1;
    MENU->songidv=nv;
    MENU->songida=na;
  }
  MENU->songidv[MENU->songidc++]=rid;
  return 0;
}

static int audio_add_soundid(struct menu *menu,int rid) {
  if (MENU->soundidc>=MENU->soundida) {
    int na=MENU->soundida+32;
    if (na>INT_MAX/sizeof(int)) return -1;
    void *nv=realloc(MENU->soundidv,sizeof(int)*na);
    if (!nv) return -1;
    MENU->soundidv=nv;
    MENU->soundida=na;
  }
  MENU->soundidv[MENU->soundidc++]=rid;
  return 0;
}
 
static int audio_gather_ids(struct menu *menu) {
  MENU->songidc=0;
  MENU->soundidc=0;
  MENU->songidp=0;
  MENU->soundidp=0;
  if (audio_add_songid(menu,0)<0) return -1; // song:0 is a real thing, to play silence
  struct rom_reader reader={0};
  if (rom_reader_init(&reader,g.rom,g.romc)>=0) {
    struct rom_res *res;
    while (res=rom_reader_next(&reader)) {
      if (res->tid==EGG_TID_song) {
        if (audio_add_songid(menu,res->rid)<0) return -1;
      } else if (res->tid==EGG_TID_sound) {
        if (audio_add_soundid(menu,res->rid)<0) return -1;
      }
    }
  }
  return 0;
}

/* Picklist callbacks.
 */
 
static void audio_cb_song(int optid,void *userdata) {
  struct menu *menu=userdata;
  if ((MENU->songidp<0)||(MENU->songidp>=MENU->songidc)) return;
  egg_play_song(MENU->songidv[MENU->songidp],MENU->force,MENU->repeat);
}
 
static void audio_cb_force(int optid,void *userdata) {
  struct menu *menu=userdata;
  MENU->force=MENU->force?0:1;
  picklist_replace_label(MENU->picklist,OPTID_FORCE,MENU->force?"Force: YES":"Force: NO",-1);
}
 
static void audio_cb_repeat(int optid,void *userdata) {
  struct menu *menu=userdata;
  MENU->repeat=MENU->repeat?0:1;
  picklist_replace_label(MENU->picklist,OPTID_REPEAT,MENU->repeat?"Repeat: YES":"Repeat: NO",-1);
}
 
static void audio_cb_sound(int optid,void *userdata) {
  struct menu *menu=userdata;
  if ((MENU->soundidp<0)||(MENU->soundidp>=MENU->soundidc)) return;
  egg_play_sound(MENU->soundidv[MENU->soundidp]);
}

/* New.
 */
 
struct menu *menu_spawn_audio() {
  struct menu *menu=menu_spawn(sizeof(struct menu_audio));
  if (!menu) return 0;
  
  menu->del=_audio_del;
  menu->input=_audio_input;
  menu->update=_audio_update;
  menu->render=_audio_render;
  
  audio_gather_ids(menu);
  
  if (!(MENU->picklist=picklist_new())) { menu->defunct=1; return 0; }
  if (MENU->songidc>0) {
    picklist_add_option(MENU->picklist,"",0,OPTID_SONG,menu,audio_cb_song);
    picklist_add_option(MENU->picklist,MENU->force?"Force: YES":"Force: NO",-1,OPTID_FORCE,menu,audio_cb_force);
    picklist_add_option(MENU->picklist,MENU->repeat?"Repeat: YES":"Repeat: NO",-1,OPTID_REPEAT,menu,audio_cb_repeat);
    audio_rewrite_song_label(menu);
  } else {
    picklist_add_option(MENU->picklist,"No songs!",-1,0,0,0);
  }
  if (MENU->soundidc>0) {
    picklist_add_option(MENU->picklist,"",0,OPTID_SOUND,menu,audio_cb_sound);
    audio_rewrite_sound_label(menu);
  } else {
    picklist_add_option(MENU->picklist,"No sounds!",-1,0,0,0);
  }
  
  //TODO Report and adjust playhead.
  
  return menu;
}
