#include "egg/egg.h"
#include "opt/stdlib/egg-stdlib.h"
#include "opt/text/text.h"
#include "opt/rom/rom.h"
#include "egg_rom_toc.h"

#define SCREENW 320
#define SCREENH 180

#define songid_last 14

static int texid_tiles8=0;
static int texid_qoi=0;
static int texid_rawimg=0;
static int texid_rlead=0;
static float xscale=1.0f;
static float yscale=1.0f;
static float dxscale=0.5f; // Absolute. The more logical multiplying method is complicated since we have to multiply by elapsed time.
static float dyscale=0.6f;
static float rotate=0.0f;
static float drotate=1.0f; // radian/s
static int pvinstate=0;
static int songid=0;
static struct font *font=0;
static int texid_label=0;
static int labelw=0,labelh=0;
static void *rom=0;
static int romc=0;

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
    //egg_set_language(EGG_LANG_FROM_STRING("fr"));
  }
  
  // Storage.
  {
    int romc=egg_get_rom(0,0);
    fprintf(stderr,"egg_get_rom(): %d\n",romc);
    uint8_t *rom=malloc(romc);
    if (rom) {
      egg_get_rom(rom,romc);
      fprintf(stderr,"ROM signature: %02x %02x %02x %02x\n",rom[0],rom[1],rom[2],rom[3]);
      free(rom);
    }
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
  
  // Audio.
  egg_play_sound(1);
  //egg_play_song(1,0,1);
  egg_audio_event(0,0x90,0x48,0x40,500);
  fprintf(stderr,"egg_audio_get_playhead(): %f\n",egg_audio_get_playhead());
  egg_audio_set_playhead(4.0);
  
  // Video.
  int screenw=0,screenh=0;
  if (egg_texture_get_status(&screenw,&screenh,1)<1) egg_terminate(1);
  if ((screenw!=SCREENW)||(screenh!=SCREENH)) {
    fprintf(stderr,"Got framebuffer %dx%d, expected %dx%d\n",screenw,screenh,SCREENW,SCREENH);
    egg_terminate(1);
  }
  if (egg_texture_load_image(texid_tiles8=egg_texture_new(),5)<0) {
    fprintf(stderr,"Failed to load image:5 (tiles8)\n");
    egg_terminate(1);
  }
  if (egg_texture_load_image(texid_qoi=egg_texture_new(),6)<0) {
    fprintf(stderr,"Failed to load image:6 (qoitest)\n");
    egg_terminate(1);
  }
  if (egg_texture_load_image(texid_rawimg=egg_texture_new(),7)<0) {
    fprintf(stderr,"Failed to load image:7 (rawimgtest)\n");
    egg_terminate(1);
  }
  if (egg_texture_load_image(texid_rlead=egg_texture_new(),2)<0) {
    fprintf(stderr,"Failed to load image:2 (font_9h_21)\n");
    egg_terminate(1);
  }
  /* TODO test video
void egg_texture_del(int texid);
int egg_texture_get_pixels(void *dst,int dsta,int texid);
int egg_texture_load_serial(int texid,const void *src,int srcc);
int egg_texture_load_raw(int texid,int fmt,int w,int h,int stride,const void *src,int srcc);
  */
  // All the egg_draw_* only work during egg_client_render(). See below.
  // Similarly, egg_get_events() only works during egg_client_update().
}

/* 2024-12-06: Testing map, tilesheet, and sprite as I build them out.
 ***************************************************************/
 
static void dump_map(const struct rom_res *res) {
  fprintf(stderr,"map:%d, %d bytes\n",res->rid,res->c);
  struct rom_map map;
  if (rom_map_decode(&map,res->v,res->c)<0) {
    fprintf(stderr,"map:%d failed to decode!\n",res->rid);
    return;
  }
  fprintf(stderr,"map:%d %dx%d\n",res->rid,map.w,map.h);
  struct rom_command_reader reader={.v=map.cmdv,.c=map.cmdc};
  struct rom_command cmd;
  while (rom_command_reader_next(&cmd,&reader)>0) {
    fprintf(stderr,"  cmd %02x, %d bytes payload\n",cmd.opcode,cmd.argc);
  }
}
 
static void dump_tilesheet(const struct rom_res *res) {
  fprintf(stderr,"tilesheet:%d, %d bytes\n",res->rid,res->c);
  struct rom_tilesheet_reader reader;
  if (rom_tilesheet_reader_init(&reader,res->v,res->c)<0) {
    fprintf(stderr,"tilesheet:%d failed to decode!\n",res->rid);
    return;
  }
  struct rom_tilesheet_entry entry;
  while (rom_tilesheet_reader_next(&entry,&reader)>0) {
    fprintf(stderr,"  tableid=0x%02x tileid=0x%02x c=%d\n",entry.tableid,entry.tileid,entry.c);
  }
}
 
static void dump_sprite(const struct rom_res *res) {
  fprintf(stderr,"sprite:%d, %d bytes\n",res->rid,res->c);
  struct rom_sprite sprite;
  if (rom_sprite_decode(&sprite,res->v,res->c)<0) {
    fprintf(stderr,"sprite:%d failed to decode!\n",res->rid);
    return;
  }
  struct rom_command_reader reader={.v=sprite.cmdv,.c=sprite.cmdc};
  struct rom_command cmd;
  while (rom_command_reader_next(&cmd,&reader)>0) {
    fprintf(stderr,"  cmd %02x, %d bytes payload\n",cmd.opcode,cmd.argc);
  }
}
 
static void dump_new_standard_resources() {
  romc=egg_get_rom(0,0);
  if (!(rom=malloc(romc))) return;
  if (egg_get_rom(rom,romc)!=romc) return;
  struct rom_reader reader={0};
  if (rom_reader_init(&reader,rom,romc)<0) return;
  struct rom_res *res;
  while (res=rom_reader_next(&reader)) {
    //fprintf(stderr,"%d:%d %d\n",res->tid,res->rid,res->c);
    switch (res->tid) {
      case EGG_TID_map: dump_map(res); break;
      case EGG_TID_tilesheet: dump_tilesheet(res); break;
      case EGG_TID_sprite: dump_sprite(res); break;
    }
  }
}

/* Entry points.
 **************************************************************/

void egg_client_quit(int status) {
  egg_log(__func__);
}

int egg_client_init() {

  if (1) {
    dump_new_standard_resources();
    fprintf(stderr,"%s aborting, we're just dumping the new resources\n",__func__);
    return -1;
  }

  fprintf(stderr,"%d function %s was called, around %s:%d. %d!\n",123,__func__,__FILE__,__LINE__,789);
  test_full_api();
  fprintf(stderr,"test_full_api ok\n");
  //egg_play_song(songid=1,0,1);
  
  romc=egg_get_rom(0,0);
  if (!(rom=malloc(romc))) return -1;
  if (egg_get_rom(rom,romc)!=romc) return -1;
  strings_set_rom(rom,romc);
  
  if (!(font=font_new())) return -1;
  // Plain Jane:
  //if (font_add_image_resource(font,0x0020,RID_image_font9_0020)<0) return -1;
  //if (font_add_image_resource(font,0x00a1,RID_image_font9_00a1)<0) return -1;
  //if (font_add_image_resource(font,0x0400,RID_image_font9_0400)<0) return -1;
  // Tiny:
  //if (font_add_image_resource(font,0x0020,RID_image_font6_0020)<0) return -1;
  // Cursive:
  //if (font_add_image_resource(font,0x0020,RID_image_cursive_0020)<0) return -1;
  // Witchy:
  if (font_add_image_resource(font,0x0020,RID_image_witchy_0020)<0) return -1;
  //if ((texid_label=font_texres_oneline(font,1,5,200,0xffffffff))<0) return -1;
  if ((texid_label=font_texres_multiline(font,1,10,200,200,0xffffffff))<0) return -1;
  egg_texture_get_status(&labelw,&labelh,texid_label);
  
  return 0;
}

static int pv1=0,pv2=0;

void egg_client_update(double elapsed) {

  int pstate;
  if ((pstate=egg_input_get_one(1))!=pv1) fprintf(stderr,"Player 1: 0x%04x\n",pv1=pstate);
  if ((pstate=egg_input_get_one(2))!=pv2) fprintf(stderr,"Player 2: 0x%04x\n",pv2=pstate);

  int instate=egg_input_get_one(0);
  if (instate!=pvinstate) {
    char msg[256];
    memcpy(msg,"INPUT(aggregate): ",18);
    int msgc=18;
    #define _(tag) if (instate&EGG_BTN_##tag) { memcpy(msg+msgc,#tag,sizeof(#tag)-1); msgc+=sizeof(#tag)-1; msg[msgc++]=' '; }
    _(LEFT)
    _(RIGHT)
    _(UP)
    _(DOWN)
    _(SOUTH)
    _(WEST)
    _(EAST)
    _(NORTH)
    _(L1)
    _(R1)
    _(L2)
    _(R2)
    _(AUX1)
    _(AUX2)
    _(AUX3)
    _(CD)
    #undef _
    fprintf(stderr,"%.*s\n",msgc,msg);
    if ((instate&EGG_BTN_SOUTH)&&!(pvinstate&EGG_BTN_SOUTH)) egg_play_sound(1);
    if ((instate&EGG_BTN_EAST)&&!(pvinstate&EGG_BTN_EAST)) egg_play_sound(2);
    if ((instate&EGG_BTN_WEST)&&!(pvinstate&EGG_BTN_WEST)) {
      //if (songid==5) songid=8; else if (songid==8) songid=6; else songid=5;
      if (++songid>songid_last) songid=1;
      fprintf(stderr,"Play song %d...\n",songid);
      egg_play_song(songid,0,songid!=8);
    }
    pvinstate=instate;
  }

  xscale+=dxscale*elapsed;
       if ((xscale>3.5f)&&(dxscale>0.0f)) dxscale=-dxscale;
  else if ((xscale<0.5f)&&(dxscale<0.0f)) dxscale=-dxscale;
  yscale+=dyscale*elapsed;
       if ((yscale>3.5f)&&(dyscale>0.0f)) dyscale=-dyscale;
  else if ((yscale<0.5f)&&(dyscale<0.0f)) dyscale=-dyscale;
  rotate+=drotate*elapsed;
  if (rotate>M_PI) rotate-=M_PI*2.0f;
}

void egg_client_render() {
  egg_draw_clear(1,0x102040ff);
  /**/
  {
    struct egg_draw_line vtxv[]={
      {1,1,SCREENW-2,SCREENH-2,0xff,0xff,0xff,0xff}, // White NW to SE.
      {SCREENW-2,1,1,SCREENH-2,0xff,0xff,0xff,0x80}, // Partial white NE to SW.
      {1,SCREENH>>1,SCREENW-2,SCREENH>>1,0xff,0x00,0x00,0xff}, // Red horizontal at vertical mid.
      {SCREENW>>1,1,SCREENW>>1,SCREENH-2,0x00,0x00,0xff,0xff}, // Blue vertical at horizontal mid.
    };
    egg_draw_globals(0x00000000,0xff); // Fiddle with globals if you like. Verified.
    egg_draw_line(1,vtxv,sizeof(vtxv)/sizeof(vtxv[0]));
  }
  {
    struct egg_draw_rect vtxv[]={
      {10,10,30,20,0xff,0x00,0x00,0xff},
      {15,15,30,20,0x00,0x00,0xff,0x80},
    };
    egg_draw_globals(0x00000000,0xff);
    egg_draw_rect(1,vtxv,sizeof(vtxv)/sizeof(vtxv[0]));
  }
  {
    struct egg_draw_trig vtxv[]={
      {100,20,150,30,120,50,0xff,0x00,0x00,0xff},
      {150,30,200,20,180,50,0x00,0xff,0x00,0xff},
    };
    egg_draw_globals(0x00000000,0xff);
    egg_draw_trig(1,vtxv,sizeof(vtxv)/sizeof(vtxv[0]));
  }
  {
    struct egg_draw_tile vtxv[]={
      { 50,80,0x00,0},
      { 60,80,0x01,0},
      { 70,80,0x02,0},
      { 80,80,0x10,0},
      { 90,80,0x11,0},
      {100,80,0xff,0},
      {110,80,0x20,0}, // xform references...
      {120,80,0x21,0},
      {130,80,0x22,0},
      {140,80,0x23,0},
      {150,80,0x24,0},
      {160,80,0x25,0},
      {170,80,0x26,0},
      {180,80,0x27,0},
      {110,90,0x20,0}, // xforms for validation...
      {120,90,0x20,1}, // XREV
      {130,90,0x20,2}, // YREV
      {140,90,0x20,3}, // XREV|YREV
      {150,90,0x20,4}, // SWAP
      {160,90,0x20,5}, // XREV|SWAP
      {170,90,0x20,6}, // SWAP|YREV
      {180,90,0x20,7}, // XREV|YREV|SWAP
    };
    egg_draw_globals(0x00000000,0xff);
    egg_draw_tile(1,texid_tiles8,vtxv,sizeof(vtxv)/sizeof(vtxv[0]));
  }
  {
    struct egg_draw_decal vtxv[]={
      {106, 96,0,16,8,8,0}, // xforms for validation...
      {116, 96,0,16,8,8,1}, // XREV
      {126, 96,0,16,8,8,2}, // YREV
      {136, 96,0,16,8,8,3}, // XREV|YREV
      {146, 96,0,16,8,8,4}, // SWAP
      {156, 96,0,16,8,8,5}, // XREV|SWAP
      {166, 96,0,16,8,8,6}, // SWAP|YREV
      {176, 96,0,16,8,8,7}, // XREV|YREV|SWAP
    };
    egg_draw_globals(0x00000000,0xff);
    egg_draw_decal(1,texid_tiles8,vtxv,sizeof(vtxv)/sizeof(vtxv[0]));
  }
  {
    struct egg_draw_mode7 vtxv[]={
      {110,110,0,16,8,8, 1.0f, 1.0f, 0.0f}, // xforms for validation...
      {120,110,0,16,8,8,-1.0f, 1.0f, 0.0f}, // XREV
      {130,110,0,16,8,8, 1.0f,-1.0f, 0.0f}, // YREV
      {140,110,0,16,8,8,-1.0f,-1.0f, 0.0f}, // XREV|YREV
      {150,110,0,16,8,8, 1.0f,-1.0f, M_PI/2.0f}, // SWAP
      {160,110,0,16,8,8,-1.0f,-1.0f, M_PI/2.0f}, // XREV|SWAP
      {170,110,0,16,8,8, 1.0f, 1.0f, M_PI/2.0f}, // SWAP|YREV
      {180,110,0,16,8,8,-1.0f, 1.0f, M_PI/2.0f}, // XREV|YREV|SWAP
      {210, 95,72,32,8,8,xscale,yscale,rotate}, // Demo continuous scale and rotation.
    };
    egg_draw_globals(0x00000000,0xff);
    egg_draw_mode7(1,texid_tiles8,vtxv,sizeof(vtxv)/sizeof(vtxv[0]),0);
  }
  {
    struct egg_draw_mode7 vtxv[]={
      {120,140,0,64,32,32,0.75f,0.75f,rotate},
    };
    egg_draw_globals(0x00000000,0xff);
    egg_draw_mode7(1,texid_tiles8,vtxv,sizeof(vtxv)/sizeof(vtxv[0]),1);
  }
  {
    struct egg_draw_decal vtxv={200, 20,0,0,32,32,0};
    egg_draw_decal(1,texid_qoi,&vtxv,1);
  }
  {
    struct egg_draw_decal vtxv={200, 60,0,0,32,32,0};
    egg_draw_decal(1,texid_rawimg,&vtxv,1);
  }
  {
    struct egg_draw_decal vtxv={200,100,0,0,84,54,0};
    egg_draw_decal(1,texid_rlead,&vtxv,1);
  }
  /**/
  {
    struct egg_draw_decal vtxv={20,20,0,0,labelw,labelh,0};
    egg_draw_decal(1,texid_label,&vtxv,1);
  }
}
