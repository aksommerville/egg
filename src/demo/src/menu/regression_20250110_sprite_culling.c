/* regression_20250110_sprite_culling.c
 * On my iMac (oddly not on my MacBook), point sprites get culled when their center is OOB.
 */
 
#include "../demo.h"

struct menu_20250110 {
  struct menu hdr;
  int texid_msg;
  int msgw,msgh;
};

#define MENU ((struct menu_20250110*)menu)

static void _20250110_del(struct menu *menu) {
  egg_texture_del(MENU->texid_msg);
}

static void _20250110_render(struct menu *menu) {
  graf_draw_rect(&g.graf,0,0,g.fbw,g.fbh,0x008000ff);
  int texid=texcache_get_image(&g.texcache,RID_image_tiles8);
  graf_draw_tile(&g.graf,texid,1,g.fbh>>1,0x00,0);
  graf_draw_tile(&g.graf,texid,0,(g.fbh>>1)+8,0x00,0);
  graf_draw_tile(&g.graf,texid,-1,(g.fbh>>1)+16,0x00,0);
  graf_draw_tile(&g.graf,texid,(g.fbw>>1),1,0x00,0);
  graf_draw_tile(&g.graf,texid,(g.fbw>>1)+8,0,0x00,0);
  graf_draw_tile(&g.graf,texid,(g.fbw>>1)+16,-1,0x00,0);
  graf_draw_tile(&g.graf,texid,g.fbw-1,(g.fbh>>1),0x00,0);
  graf_draw_tile(&g.graf,texid,g.fbw,(g.fbh>>1)+8,0x00,0);
  graf_draw_tile(&g.graf,texid,g.fbw+1,(g.fbh>>1)+16,0x00,0);
  graf_draw_tile(&g.graf,texid,(g.fbw>>1),g.fbh-1,0x00,0);
  graf_draw_tile(&g.graf,texid,(g.fbw>>1)+8,g.fbh,0x00,0);
  graf_draw_tile(&g.graf,texid,(g.fbw>>1)+16,g.fbh+1,0x00,0);
  graf_draw_decal(&g.graf,MENU->texid_msg,(g.fbw>>1)-(MENU->msgw>>1),(g.fbh>>1)-(MENU->msgh>>1),0,0,MENU->msgw,MENU->msgh,0);
}

struct menu *menu_spawn_20250110() {
  struct menu *menu=menu_spawn(sizeof(struct menu_20250110));
  if (!menu) return 0;
  menu->del=_20250110_del;
  menu->render=_20250110_render;
  MENU->texid_msg=font_tex_oneline(g.font,"Three tiles on each side.",-1,g.fbh,0xffffffff);
  egg_texture_get_status(&MENU->msgw,&MENU->msgh,MENU->texid_msg);
  return menu;
}
