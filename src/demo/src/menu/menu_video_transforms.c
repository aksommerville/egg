#include "../demo.h"

#define LABELC 12 /* 8 columns and 4 rows */

#define COLW 30
#define ROWH 12
#define TOTALW (COLW*9)
#define TOTALH (ROWH*5)

struct menu_transforms {
  struct menu hdr;
  struct label {
    int texid,texw,texh;
    int x,y;
  } labelv[LABELC];
};

#define MENU ((struct menu_transforms*)menu)

/* Delete.
 */
 
static void _transforms_del(struct menu *menu) {
  struct label *label=MENU->labelv;
  int i=LABELC;
  for (;i-->0;label++) egg_texture_del(label->texid);
}

/* Render.
 */
 
static void _transforms_render(struct menu *menu) {
  const int x0=(g.fbw>>1)-(TOTALW>>1);
  const int y0=(g.fbh>>1)-(TOTALH>>1);
  int texid=texcache_get_image(&g.texcache,RID_image_tiles8);

  // A label on each column and row.
  struct label *label=MENU->labelv;
  int i=LABELC;
  for (;i-->0;label++) graf_draw_decal(&g.graf,label->texid,label->x,label->y,0,0,label->texw,label->texh,0);
  
  // Reference tiles, pre-transformed.
  {
    int16_t dstx=x0+(COLW>>1);
    int16_t dsty=y0+ROWH+(ROWH>>1);
    uint8_t xform=0;
    for (;xform<8;xform++,dstx+=COLW) graf_draw_tile(&g.graf,texid,dstx,dsty,0x20+xform,0);
  }
  
  // Transformed tiles.
  {
    int16_t dstx=x0+(COLW>>1);
    int16_t dsty=y0+ROWH*2+(ROWH>>1);
    uint8_t xform=0;
    for (;xform<8;xform++,dstx+=COLW) graf_draw_tile(&g.graf,texid,dstx,dsty,0x20,xform);
  }
  
  // Transformed decals.
  {
    int16_t dstx=x0+(COLW>>1)-4;
    int16_t dsty=y0+ROWH*3+(ROWH>>1)-4;
    uint8_t xform=0;
    for (;xform<8;xform++,dstx+=COLW) {
      graf_draw_decal(&g.graf,texid,dstx,dsty,0,16,8,8,xform);
    }
  }

  // Decals with equivalent mode7 transforms.
  {
    int16_t dstx=x0+(COLW>>1);
    int16_t dsty=y0+ROWH*4+(ROWH>>1);
    uint8_t xform=0;
    for (;xform<8;xform++,dstx+=COLW) {
      double xscale=1.0,yscale=1.0,rotate=0.0;
      if (xform&EGG_XFORM_XREV) xscale=-1.0;
      if (xform&EGG_XFORM_YREV) yscale=-1.0;
      if (xform&EGG_XFORM_SWAP) { rotate=M_PI*0.5; yscale*=-1.0; }
      graf_draw_mode7(&g.graf,texid,dstx,dsty,0,16,8,8,xscale,yscale,rotate,0);
    }
  }
}

/* Initialize label.
 */
 
static void transforms_init_label(struct label *label,int col,int row,const char *text) {
  const int x0=(g.fbw>>1)-(TOTALW>>1);
  const int y0=(g.fbh>>1)-(TOTALH>>1);
  label->texid=font_tex_oneline(g.font,text,-1,100,0xffffffff);
  egg_texture_get_status(&label->texw,&label->texh,label->texid);
  label->x=x0+col*COLW+(COLW>>1)-(label->texw>>1);
  label->y=y0+row*ROWH+(ROWH>>1)-(label->texh>>1);
}

/* New.
 */
 
struct menu *menu_spawn_video_transforms() {
  struct menu *menu=menu_spawn(sizeof(struct menu_transforms));
  if (!menu) return 0;
  
  menu->del=_transforms_del;
  menu->render=_transforms_render;
  
  struct label *label=MENU->labelv;
  transforms_init_label(label++,0,0,"None");
  transforms_init_label(label++,1,0,"X");
  transforms_init_label(label++,2,0,"Y");
  transforms_init_label(label++,3,0,"X|Y");
  transforms_init_label(label++,4,0,"S");
  transforms_init_label(label++,5,0,"X|S");
  transforms_init_label(label++,6,0,"Y|S");
  transforms_init_label(label++,7,0,"X|Y|S");
  transforms_init_label(label++,8,1,"Ref");
  transforms_init_label(label++,8,2,"Tile");
  transforms_init_label(label++,8,3,"Decal");
  transforms_init_label(label++,8,4,"Mode7");
  
  return menu;
}
