#include "../demo.h"
#include "../ui/picklist.h"

struct menu_video {
  struct menu hdr;
  struct picklist *picklist;
};

#define MENU ((struct menu_video*)menu)

/* Delete.
 */
 
static void _video_del(struct menu *menu) {
  picklist_del(MENU->picklist);
}

/* Input.
 */
 
static void _video_input(struct menu *menu,int input,int pvinput) {
  if ((input&EGG_BTN_WEST)&&!(pvinput&EGG_BTN_WEST)) menu->defunct=1;
  picklist_input(MENU->picklist,input,pvinput);
}

/* Update.
 */
 
static void _video_update(struct menu *menu,double elapsed) {
  picklist_update(MENU->picklist,elapsed);
}

/* Render.
 */
 
static void _video_render(struct menu *menu) {
  picklist_render(MENU->picklist);
}

/* Simple callbacks.
 */
 
static void video_cb_primitives(int optid,void *userdata) {
  menu_spawn_video_primitives();
}

static void video_cb_transforms(int optid,void *userdata) {
  menu_spawn_video_transforms();
}

static void video_cb_aux(int optid,void *userdata) {
  menu_spawn_video_aux();
}

static void video_cb_stress(int optid,void *userdata) {
  menu_spawn_video_stress();
}

/* New.
 */
 
struct menu *menu_spawn_video() {
  struct menu *menu=menu_spawn(sizeof(struct menu_video));
  if (!menu) return 0;
  
  menu->del=_video_del;
  menu->input=_video_input;
  menu->update=_video_update;
  menu->render=_video_render;
  
  if (!(MENU->picklist=picklist_new())) { menu->defunct=1; return 0; }
  picklist_add_option(MENU->picklist,"Primitives",-1,0,0,video_cb_primitives);
  picklist_add_option(MENU->picklist,"Transforms",-1,0,0,video_cb_transforms);
  picklist_add_option(MENU->picklist,"Aux Buffer",-1,0,0,video_cb_aux);
  picklist_add_option(MENU->picklist,"Stress",-1,0,0,video_cb_stress);
  
  return menu;
}
