#include "../demo.h"
#include "../ui/picklist.h"

struct menu_regression {
  struct menu hdr;
  struct picklist *picklist;
};

#define MENU ((struct menu_regression*)menu)

/* Delete.
 */
 
static void _regression_del(struct menu *menu) {
  picklist_del(MENU->picklist);
}

/* Input.
 */
 
static void _regression_input(struct menu *menu,int input,int pvinput) {
  if ((input&EGG_BTN_WEST)&&!(pvinput&EGG_BTN_WEST)) menu->defunct=1;
  picklist_input(MENU->picklist,input,pvinput);
}

/* Update.
 */
 
static void _regression_update(struct menu *menu,double elapsed) {
  picklist_update(MENU->picklist,elapsed);
}

/* Render.
 */
 
static void _regression_render(struct menu *menu) {
  picklist_render(MENU->picklist);
}

/* New.
 */
 
struct menu *menu_spawn_regression() {
  struct menu *menu=menu_spawn(sizeof(struct menu_regression));
  if (!menu) return 0;
  
  menu->del=_regression_del;
  menu->input=_regression_input;
  menu->update=_regression_update;
  menu->render=_regression_render;
  
  if (!(MENU->picklist=picklist_new())) { menu->defunct=1; return 0; }
  picklist_add_option(MENU->picklist,"TODO Regression",-1,0,0,0);
  
  return menu;
}
