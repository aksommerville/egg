#include "../demo.h"

struct menu_input {
  struct menu hdr;
  int escbtn;
  int escstrokec;
  int eschold;
  double escclock;
  int logw,logh;
  uint32_t *logbits;
  int logtexid;
};

#define MENU ((struct menu_input*)menu)

/* Delete.
 */
 
static void _input_del(struct menu *menu) {
  if (MENU->logbits) free(MENU->logbits);
  egg_texture_del(MENU->logtexid);
}

/* Log.
 */
 
static const char *input_button_name(int btnid) {
  switch (btnid) {
    #define _(tag) case EGG_BTN_##tag: return #tag;
    _(SOUTH)
    _(EAST)
    _(WEST)
    _(NORTH)
    _(L1)
    _(R1)
    _(L2)
    _(R2)
    _(AUX1)
    _(AUX2)
    _(AUX3)
    _(LEFT)
    _(RIGHT)
    _(UP)
    _(DOWN)
    #undef _
  }
  return 0;
}
 
static void input_log_line(struct menu *menu,const char *src,int srcc) {
  int rowh=font_get_line_height(g.font);
  if ((rowh<1)||(rowh>MENU->logh)) return; // wha?
  memmove(MENU->logbits,MENU->logbits+MENU->logw*rowh,MENU->logw*(MENU->logh-rowh)*4);
  memset(MENU->logbits+MENU->logw*(MENU->logh-rowh),0,MENU->logw*rowh*4);
  font_render_string(
    MENU->logbits,MENU->logw,MENU->logh,MENU->logw<<2,
    0,MENU->logh-rowh,
    g.font,src,srcc,0xffffffff
  );
  if (!MENU->logtexid) MENU->logtexid=egg_texture_new();
  egg_texture_load_raw(MENU->logtexid,MENU->logw,MENU->logh,MENU->logw<<2,MENU->logbits,MENU->logw*MENU->logh*4);
}
 
static void input_log_event(struct menu *menu,int btnid,int v) {
  const char *name=input_button_name(btnid);
  if (!name) return; // Error or CD -- don't log it.
  char tmp[256];
  int tmpc=snprintf(tmp,sizeof(tmp),"%f %s = %d",egg_time_real(),name,v);
  if ((tmpc<1)||(tmpc>=sizeof(tmp))) return;
  input_log_line(menu,tmp,tmpc);
}

/* Input.
 */
 
static int input_single_bit(int v) {
  while (v>1) v>>=1;
  return (v==1);
}
 
static void _input_input(struct menu *menu,int input,int pvinput) {
  /* If there is exactly one bit set, it qualifies for (escbtn).
   */
  int realbtn=input&(EGG_BTN_LEFT|EGG_BTN_RIGHT|EGG_BTN_UP|EGG_BTN_DOWN|EGG_BTN_SOUTH|EGG_BTN_NORTH|EGG_BTN_WEST|EGG_BTN_EAST|EGG_BTN_L1|EGG_BTN_R1|EGG_BTN_L2|EGG_BTN_R2|EGG_BTN_AUX1|EGG_BTN_AUX2|EGG_BTN_AUX3);
  if (!realbtn) {
    MENU->eschold=0;
  } else if (!MENU->escbtn) {
    if (input_single_bit(realbtn)) {
      MENU->escbtn=realbtn;
      MENU->escstrokec=1;
      MENU->eschold=1;
      MENU->escclock=1.0;
    }
  } else if (realbtn==MENU->escbtn) {
    if (!(pvinput&MENU->escbtn)) {
      MENU->escstrokec++;
      MENU->eschold=1;
      if (MENU->escstrokec>=3) menu->defunct=1;
    }
  } else {
    MENU->escbtn=0;
    MENU->escstrokec=0;
    MENU->eschold=0;
    MENU->escclock=0.0;
  }
  
  int bit=1,limit=(input>pvinput)?input:pvinput;
  for (;bit<=limit;bit<<=1) {
    if ((input&bit)&&!(pvinput&bit)) input_log_event(menu,bit,1);
    else if (!(input&bit)&&(pvinput&bit)) input_log_event(menu,bit,0);
  }
}

/* Update.
 */
 
static void _input_update(struct menu *menu,double elapsed) {
  if (MENU->escclock>0.0) {
    if ((MENU->escclock-=elapsed)<=0.0) {
      MENU->escbtn=0;
      MENU->escstrokec=0;
      MENU->eschold=0;
    }
  }
}

/* Render state of one player.
 */
 
static void input_draw_state(struct menu *menu,int x,int y,int state,int valid,int texid) {
  if (!(state&EGG_BTN_CD)) graf_set_alpha(&g.graf,0x80);
  graf_draw_decal(&g.graf,texid,x,y,72,64,24,16,0);
  graf_set_alpha(&g.graf,0xff);
  if (state&EGG_BTN_LEFT ) graf_draw_rect(&g.graf,x+ 3,y+ 8,3,2,0xffff00ff);
  if (state&EGG_BTN_RIGHT) graf_draw_rect(&g.graf,x+ 8,y+ 8,3,2,0xffff00ff);
  if (state&EGG_BTN_UP   ) graf_draw_rect(&g.graf,x+ 6,y+ 5,2,3,0xffff00ff);
  if (state&EGG_BTN_DOWN ) graf_draw_rect(&g.graf,x+ 6,y+10,2,3,0xffff00ff);
  if (state&EGG_BTN_SOUTH) graf_draw_rect(&g.graf,x+17,y+11,2,2,0xffff00ff);
  if (state&EGG_BTN_WEST ) graf_draw_rect(&g.graf,x+14,y+ 8,2,2,0xffff00ff);
  if (state&EGG_BTN_EAST ) graf_draw_rect(&g.graf,x+20,y+ 8,2,2,0xffff00ff);
  if (state&EGG_BTN_NORTH) graf_draw_rect(&g.graf,x+17,y+ 5,2,2,0xffff00ff);
  if (state&EGG_BTN_L1   ) graf_draw_rect(&g.graf,x+ 3,y+ 1,3,1,0xffff00ff);
  if (state&EGG_BTN_R1   ) graf_draw_rect(&g.graf,x+18,y+ 1,3,1,0xffff00ff);
  if (state&EGG_BTN_L2   ) graf_draw_rect(&g.graf,x+ 7,y+ 1,2,1,0xffff00ff);
  if (state&EGG_BTN_R2   ) graf_draw_rect(&g.graf,x+15,y+ 1,2,1,0xffff00ff);
  if (state&EGG_BTN_AUX1 ) graf_draw_rect(&g.graf,x+13,y+12,2,1,0xffff00ff);
  if (state&EGG_BTN_AUX2 ) graf_draw_rect(&g.graf,x+10,y+12,2,1,0xffff00ff);
  if (state&EGG_BTN_AUX3 ) graf_draw_rect(&g.graf,x+11,y+ 4,2,2,0xffff00ff);
}

/* Render.
 */
 
static void _input_render(struct menu *menu) {
  int texid=texcache_get_image(&g.texcache,RID_image_tiles8);
  if (MENU->escstrokec>=2) {
    graf_draw_rect(&g.graf,0,0,g.fbw,g.fbh,0x400000ff);
  }

  /* States for player zero thru seven along the bottom.
   */
  int statev[8]={0};
  int statec=egg_input_get_all(statev,8);
  int i=0,dstx=8; for (;i<8;i++,dstx+=40) {
    input_draw_state(menu,dstx,160,statev[i],i<statec,texid);
  }
  
  graf_draw_decal(&g.graf,MENU->logtexid,(g.fbw>>1)-(MENU->logw>>1),80-(MENU->logh>>1),0,0,MENU->logw,MENU->logh,0);
}

/* New.
 */
 
struct menu *menu_spawn_input() {
  struct menu *menu=menu_spawn(sizeof(struct menu_input));
  if (!menu) return 0;
  
  menu->del=_input_del;
  menu->input=_input_input;
  menu->update=_input_update;
  menu->render=_input_render;
  
  MENU->logw=g.fbw>>1;
  MENU->logh=150;
  if (!(MENU->logbits=calloc(MENU->logw<<2,MENU->logh))) { menu->defunct=1; return 0; }
  
  return menu;
}
