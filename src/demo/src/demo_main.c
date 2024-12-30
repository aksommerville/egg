#include "demo.h"

struct g g={0};

/* Menu stack.
 **********************************************************************/
 
static void menu_kill(struct menu *menu) {
  if (!menu) return;
  int i=g.menuc;
  while (i-->0) {
    if (g.menuv[i]==menu) {
      g.menuc--;
      memmove(g.menuv+i,g.menuv+i+1,sizeof(void*)*(g.menuc-i));
      break;
    }
  }
  if (menu->del) menu->del(menu);
  free(menu);
}

struct menu *menu_spawn(int objlen) {
  if (objlen<(int)sizeof(struct menu)) return 0;
  if (g.menuc>=MENU_LIMIT) return 0;
  struct menu *menu=calloc(1,objlen);
  if (!menu) return 0;
  g.menuv[g.menuc++]=menu;
  return menu;
}

/* Entry points from platform: Quit, Init, Update, Render.
 *************************************************************************************/

void egg_client_quit(int status) {
}

int egg_client_init() {

  egg_texture_get_status(&g.fbw,&g.fbh,1);
  if ((g.fbw<1)||(g.fbh<1)) {
    fprintf(stderr,"Invalid size %d,%d for texture 1\n",g.fbw,g.fbh);
    return -1;
  }
  
  g.romc=egg_get_rom(0,0);
  if (!(g.rom=malloc(g.romc))) return -1;
  if (egg_get_rom(g.rom,g.romc)!=g.romc) return -1;
  strings_set_rom(g.rom,g.romc);
  
  if (!(g.font=font_new())) return -1;
  if (font_add_image_resource(g.font,0x0020,RID_image_font9_0020)<0) return -1;
  if (font_add_image_resource(g.font,0x00a1,RID_image_font9_00a1)<0) return -1;
  if (font_add_image_resource(g.font,0x0400,RID_image_font9_0400)<0) return -1;
  
  if (!menu_spawn_main()) return -1;
  
  srand_auto();

  return 0;
}

void egg_client_update(double elapsed) {

  // React to changed input state.
  int input=egg_input_get_one(0);
  if (input!=g.pvinput) {
    int pv=g.pvinput;
    g.pvinput=input;
    if (g.menuc>0) {
      struct menu *menu=g.menuv[g.menuc-1];
      if (menu->input) {
        menu->input(menu,input,pv);
      } else { // (input) not implemented; B dismisses.
        if ((input&EGG_BTN_WEST)&&!(pv&EGG_BTN_WEST)) menu_kill(menu);
      }
    }
  }
  
  // Routine maintenance for the top menu.
  if (g.menuc>0) {
    struct menu *menu=g.menuv[g.menuc-1];
    if (menu->update) menu->update(menu,elapsed);
  }
  
  // Drop defunct menus from the top and abort if we end up empty.
  while (g.menuc&&g.menuv[g.menuc-1]->defunct) {
    menu_kill(g.menuv[g.menuc-1]);
  }
  if (!g.menuc) {
    egg_terminate(0);
    return;
  }
}

void egg_client_render() {
  graf_reset(&g.graf);
  graf_draw_rect(&g.graf,0,0,g.fbw,g.fbh,0x102030ff);
  if (g.menuc>0) {
    struct menu *menu=g.menuv[g.menuc-1];
    if (menu->render) menu->render(menu);
  }
  graf_flush(&g.graf);
}
