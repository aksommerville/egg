#include "../demo.h"

struct menu_aux {
  struct menu hdr;
  int texid,texw,texh;
};

#define MENU ((struct menu_aux*)menu)

/* Delete.
 */
 
static void _aux_del(struct menu *menu) {
}

/* Render.
 */
 
static void aux_draw_scene(int dsttexid,int dstx,int dsty) {
  graf_set_output(&g.graf,dsttexid);
  graf_draw_rect(&g.graf,dstx,dsty,160,90,0x004020ff);
  graf_draw_rect(&g.graf,dstx+1,dsty+1,158,88,0x206030ff);
  graf_draw_line(&g.graf,dstx+1,dsty+1,dstx+158,dsty+88,0xffffffff);
  graf_draw_line(&g.graf,dstx+158,dsty+1,dstx+1,dsty+88,0xffffffff);
  graf_draw_decal(&g.graf,texcache_get_image(&g.texcache,RID_image_tiles8),dstx+64,dsty+29,0,64,32,32,0);
  graf_set_output(&g.graf,1);
}
 
static void _aux_render(struct menu *menu) {
  aux_draw_scene(1,0,45);
  aux_draw_scene(MENU->texid,0,0);
  graf_draw_decal(&g.graf,MENU->texid,160,45,0,0,MENU->texw,MENU->texh,0);
}

/* New.
 */
 
struct menu *menu_spawn_video_aux() {
  struct menu *menu=menu_spawn(sizeof(struct menu_aux));
  if (!menu) return 0;
  
  menu->del=_aux_del;
  menu->render=_aux_render;
  
  MENU->texw=160;
  MENU->texh=90;
  MENU->texid=egg_texture_new();
  if (egg_texture_load_raw(MENU->texid,EGG_TEX_FMT_RGBA,MENU->texw,MENU->texh,MENU->texw<<2,0,0)<0) { menu->defunct=1; return 0; }
  
  return menu;
}
