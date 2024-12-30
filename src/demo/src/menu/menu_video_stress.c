#include "../demo.h"

#define SPRITE_LIMIT 100000

struct menu_stress {
  struct menu hdr;
  struct sprite {
    double x,y;
    double dx,dy;
    char mode; // [td7]
    uint8_t tileid; // all modes
    uint8_t xform; // tile and decal
    double xscale,yscale; // mode7 only
    double dxscale,dyscale; // mode7 only
    double rotate; // mode7 only
    double drotate; // mode7 only
  } *spritev;
  int spritec,spritea;
  int tilec,decalc,mode7c; // sum = spritec
  int selp; // 0,1,2=tile,decal,mode7
};

#define MENU ((struct menu_stress*)menu)

/* Delete.
 */
 
static void _stress_del(struct menu *menu) {
  if (MENU->spritev) free(MENU->spritev);
}

/* Remove sprites.
 */
 
static void stress_remove_sprites(struct menu *menu,int p,int c) {
  if (c<1) return;
  if ((p<0)||(p>MENU->spritec-c)) return;
  MENU->spritec-=c;
  memmove(MENU->spritev+p,MENU->spritev+p+c,sizeof(struct sprite)*(MENU->spritec-p));
}

/* Add sprites.
 */
 
static int stress_add_sprites(struct menu *menu,int p,int c,char mode) {
  if (c<1) return -1;
  if ((p<0)||(p>MENU->spritec)) return -1;
  if (MENU->spritec>SPRITE_LIMIT-c) return -1;
  int nc=MENU->spritec+c;
  if (nc>MENU->spritea) {
    int na=(nc+256)&~255;
    void *nv=realloc(MENU->spritev,sizeof(struct sprite)*na);
    if (!nv) return -1;
    MENU->spritev=nv;
    MENU->spritea=na;
  }
  struct sprite *sprite=MENU->spritev+p;
  memmove(sprite+c,sprite,sizeof(struct sprite)*(MENU->spritec-p));
  MENU->spritec+=c;
  int i=c; for (;i-->0;sprite++) {
    sprite->x=rand()%g.fbw;
    sprite->y=rand()%g.fbh;
    sprite->dx=((rand()&0x7fff)*50.0)/32767.0-25.0;
    sprite->dy=((rand()&0x7fff)*50.0)/32767.0-25.0;
    sprite->mode=mode;
    switch (rand()%3) {
      case 0: sprite->tileid=0x41; break; // Dot
      case 1: sprite->tileid=0x43; break; // dragon
      case 2: sprite->tileid=0x45; break; // jelly sandwich
    }
    if (mode=='7') {
      sprite->xscale=0.25+((rand()&0x7fff)*4.0)/32767.0;
      sprite->yscale=0.25+((rand()&0x7fff)*4.0)/32767.0;
      sprite->rotate=((rand()&0x7fff)*6.28)/32767.0;
      sprite->drotate=((rand()&0x7fff)*8.0)/32767.0-4.0;
    } else {
      sprite->xform=rand()&7;
    }
  }
  return 0;
}

/* Create or destroy sprites to match (tilec,decalc,mode7c).
 * Sprites are always sorted by mode: t,d,7
 */
 
static int stress_rebuild_sprites(struct menu *menu) {
  
  int p=0,cactual=0;
  while ((p<MENU->spritec)&&(MENU->spritev[p].mode=='t')) { p++; cactual++; }
  if (cactual<MENU->tilec) {
    if (stress_add_sprites(menu,p,MENU->tilec-cactual,'t')<0) return -1;
  } else if (cactual>MENU->tilec) {
    stress_remove_sprites(menu,p-cactual,cactual-MENU->tilec);
  }
  
  p=MENU->tilec;
  cactual=0;
  while ((p<MENU->spritec)&&(MENU->spritev[p].mode=='d')) { p++; cactual++; }
  if (cactual<MENU->decalc) {
    if (stress_add_sprites(menu,p,MENU->decalc-cactual,'d')<0) return -1;
  } else if (cactual>MENU->decalc) {
    stress_remove_sprites(menu,p-cactual,cactual-MENU->decalc);
  }
  
  p=MENU->tilec+MENU->decalc;
  cactual=0;
  while ((p<MENU->spritec)&&(MENU->spritev[p].mode=='7')) { p++; cactual++; }
  if (cactual<MENU->mode7c) {
    if (stress_add_sprites(menu,p,MENU->mode7c-cactual,'7')<0) return -1;
  } else if (cactual>MENU->decalc) {
    stress_remove_sprites(menu,p-cactual,cactual-MENU->mode7c);
  }
  
  return 0;
}

/* Selection.
 */
 
static void stress_move(struct menu *menu,int d) {
  MENU->selp+=d;
  if (MENU->selp<0) MENU->selp=2;
  else if (MENU->selp>=3) MENU->selp=0;
}

static void stress_adjust(struct menu *menu,int d,int turbo) {
  int *v;
  switch (MENU->selp) {
    case 0: v=&MENU->tilec; break;
    case 1: v=&MENU->decalc; break;
    case 2: v=&MENU->mode7c; break;
    default: return;
  }
  int pv=*v;
  if (turbo) {
    if (d>0) (*v)<<=1;
    else (*v)>>=1;
  } else {
    (*v)+=d;
  }
  if (*v<0) *v=0;
  if (stress_rebuild_sprites(menu)<0) *v=pv;
}

/* Input.
 */
 
static void _stress_input(struct menu *menu,int input,int pvinput) {
  if ((input&EGG_BTN_WEST)&&!(pvinput&EGG_BTN_WEST)) menu->defunct=1;
  if ((input&EGG_BTN_LEFT)&&!(pvinput&EGG_BTN_LEFT)) stress_move(menu,-1);
  if ((input&EGG_BTN_RIGHT)&&!(pvinput&EGG_BTN_RIGHT)) stress_move(menu,1);
  if ((input&EGG_BTN_UP)&&!(pvinput&EGG_BTN_UP)) stress_adjust(menu,1,input&EGG_BTN_SOUTH);
  if ((input&EGG_BTN_DOWN)&&!(pvinput&EGG_BTN_DOWN)) stress_adjust(menu,-1,input&EGG_BTN_SOUTH);
}

/* Update.
 */
 
static void _stress_update(struct menu *menu,double elapsed) {
  struct sprite *sprite=MENU->spritev;
  int i=MENU->spritec;
  for (;i-->0;sprite++) {
    sprite->x+=sprite->dx*elapsed; if ((sprite->x<0.0)&&(sprite->dx<0.0)) sprite->dx*=-1.0; else if ((sprite->x>g.fbw)&&(sprite->dx>0.0)) sprite->dx*=-1.0;
    sprite->y+=sprite->dy*elapsed; if ((sprite->y<0.0)&&(sprite->dy<0.0)) sprite->dy*=-1.0; else if ((sprite->y>g.fbh)&&(sprite->dy>0.0)) sprite->dy*=-1.0;
    if (sprite->mode=='7') {
      sprite->rotate+=sprite->drotate*elapsed;
      if (sprite->rotate>M_PI) sprite->rotate-=M_PI*2.0;
      else if (sprite->rotate<-M_PI) sprite->rotate+=M_PI*2.0;
    }
  }
}

/* Render text bits.
 */
 
static void stress_draw_string(int texid,int dstx,int dsty,const char *src,int srcc) {
  int w=8*srcc;
  dstx-=w>>1;
  for (;srcc-->0;src++,dstx+=8) {
    char ch=*src;
    if (ch<0x30) continue;
    if ((ch>=0x61)&&(ch<=0x7a)) ch-=0x20;
    if (ch>0x5f) continue;
    graf_draw_tile(&g.graf,texid,dstx,dsty,0xc0+ch-0x30,0);
  }
}

static void stress_draw_int(int texid,int dstx,int dsty,int v) {
  char tmp[16];
  int tmpc=0;
  if (v<0) v=0;
  if (v>=1000000000) tmp[tmpc++]='0'+(v/1000000000)%10;
  if (v>=100000000 ) tmp[tmpc++]='0'+(v/100000000 )%10;
  if (v>=10000000  ) tmp[tmpc++]='0'+(v/10000000  )%10;
  if (v>=1000000   ) tmp[tmpc++]='0'+(v/1000000   )%10;
  if (v>=100000    ) tmp[tmpc++]='0'+(v/100000    )%10;
  if (v>=10000     ) tmp[tmpc++]='0'+(v/10000     )%10;
  if (v>=1000      ) tmp[tmpc++]='0'+(v/1000      )%10;
  if (v>=100       ) tmp[tmpc++]='0'+(v/100       )%10;
  if (v>=10        ) tmp[tmpc++]='0'+(v/10        )%10;
  tmp[tmpc++]='0'+v%10;
  stress_draw_string(texid,dstx,dsty,tmp,tmpc);
}

/* Render.
 */
 
static void _stress_render(struct menu *menu) {
  int texid=texcache_get_image(&g.texcache,RID_image_tiles8);
  struct sprite *sprite=MENU->spritev;
  int i=MENU->spritec;
  for (;i-->0;sprite++) {
    switch (sprite->mode) {
      case 't': graf_draw_tile(&g.graf,texid,(int16_t)sprite->x,(int16_t)sprite->y,sprite->tileid,sprite->xform); break;
      case 'd': graf_draw_decal(&g.graf,texid,(int16_t)sprite->x-4,(int16_t)sprite->y-4,(sprite->tileid&15)*8,(sprite->tileid>>4)*8,8,8,sprite->xform); break;
      case '7': graf_draw_mode7(&g.graf,texid,(int16_t)sprite->x,(int16_t)sprite->y,(sprite->tileid&15)*8,(sprite->tileid>>4)*8,8,8,sprite->xscale,sprite->yscale,sprite->rotate,1); break;
    }
  }
  graf_draw_rect(&g.graf,40+80*MENU->selp,150,80,30,0xffff0080);
  stress_draw_string(texid, 80,160,"Tile",4);
  stress_draw_string(texid,160,160,"Decal",5);
  stress_draw_string(texid,240,160,"Mode7",5);
  stress_draw_int(texid, 80,170,MENU->tilec);
  stress_draw_int(texid,160,170,MENU->decalc);
  stress_draw_int(texid,240,170,MENU->mode7c);
}

/* New.
 */
 
struct menu *menu_spawn_video_stress() {
  struct menu *menu=menu_spawn(sizeof(struct menu_stress));
  if (!menu) return 0;
  
  menu->del=_stress_del;
  menu->input=_stress_input;
  menu->update=_stress_update;
  menu->render=_stress_render;
  
  MENU->tilec=6;
  MENU->decalc=6;
  MENU->mode7c=6;
  stress_rebuild_sprites(menu);
  
  return menu;
}
