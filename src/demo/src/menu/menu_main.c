#include "../demo.h"
#include "../ui/picklist.h"

struct menu_main {
  struct menu hdr;
  struct picklist *picklist;
};

#define MENU ((struct menu_main*)menu)

/* Delete.
 */
 
static void _main_del(struct menu *menu) {
  picklist_del(MENU->picklist);
}

/* Input.
 */
 
static void _main_input(struct menu *menu,int input,int pvinput) {
  if ((input&EGG_BTN_WEST)&&!(pvinput&EGG_BTN_WEST)) menu->defunct=1;
  picklist_input(MENU->picklist,input,pvinput);
}

/* Update.
 */
 
static void _main_update(struct menu *menu,double elapsed) {
  picklist_update(MENU->picklist,elapsed);
}

/* Render.
 */
 
static void _main_render(struct menu *menu) {
  picklist_render(MENU->picklist);
}

/* Simple callbacks.
 */
 
static void main_cb_video(int optid,void *userdata) {
  menu_spawn_video();
}
 
static void main_cb_audio(int optid,void *userdata) {
  menu_spawn_audio();
}
 
static void main_cb_input(int optid,void *userdata) {
  menu_spawn_input();
}

static void main_cb_input_config(int optid,void *userdata) {
  if (egg_input_configure()<0) {
    fprintf(stderr,"egg_input_configure() failed\n");
  }
}
 
static void main_cb_storage(int optid,void *userdata) {
  menu_spawn_storage();
}
 
static void main_cb_misc(int optid,void *userdata) {
  menu_spawn_misc();
}
 
static void main_cb_regression(int optid,void *userdata) {
  menu_spawn_regression();
}
 
static void main_cb_quit(int optid,void *userdata) {
  egg_terminate(0);
}

/* New.
 */
 
struct menu *menu_spawn_main() {
  struct menu *menu=menu_spawn(sizeof(struct menu_main));
  if (!menu) return 0;
  
  menu->del=_main_del;
  menu->input=_main_input;
  menu->update=_main_update;
  menu->render=_main_render;
  
  if (!(MENU->picklist=picklist_new())) { menu->defunct=1; return 0; }
  picklist_add_option(MENU->picklist,"Video",-1,0,0,main_cb_video);
  picklist_add_option(MENU->picklist,"Audio",-1,0,0,main_cb_audio);
  picklist_add_option(MENU->picklist,"Input",-1,0,0,main_cb_input);
  picklist_add_option(MENU->picklist,"Input Config",-1,0,0,main_cb_input_config);
  picklist_add_option(MENU->picklist,"Storage",-1,0,0,main_cb_storage);
  picklist_add_option(MENU->picklist,"Miscellaneous",-1,0,0,main_cb_misc);
  picklist_add_option(MENU->picklist,"Regression",-1,0,0,main_cb_regression);
  picklist_add_option(MENU->picklist,"Quit",-1,0,0,main_cb_quit);
  
  return menu;
}
