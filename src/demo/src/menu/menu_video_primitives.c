#include "../demo.h"

struct menu_primitives {
  struct menu hdr;
};

#define MENU ((struct menu_primitives*)menu)

/* Delete.
 */
 
static void _primitives_del(struct menu *menu) {
}

/* Input.
 */
 
static void _primitives_input(struct menu *menu,int input,int pvinput) {
  if ((input&EGG_BTN_WEST)&&!(pvinput&EGG_BTN_WEST)) menu->defunct=1;
}

/* Update.
 */
 
static void _primitives_update(struct menu *menu,double elapsed) {
}

/* Render.
 */
 
static void _primitives_render(struct menu *menu) {

  // We're not going to use graf. Flush it and forget about it.
  graf_flush(&g.graf);
  egg_draw_globals(0,0xff);
  egg_draw_clear(1,0xc0d0ffff);
  
  /* Your basic primitives, bunched in the top left.
   */
  { // Black outlined rectangle. There's a visible fault here (Linux, Intel graphics): The bottom-right pixel doesn't get drawn.
    struct egg_draw_line vtxv[]={
      { 1, 1, 1,10, 0x00,0x00,0x00,0xff},
      { 1,10,10,10, 0x00,0x00,0x00,0xff},
      {10,10,10, 1, 0x00,0x00,0x00,0xff},
      {10, 1, 1, 1, 0x00,0x00,0x00,0xff},
    };
    egg_draw_line(1,vtxv,sizeof(vtxv)/sizeof(vtxv[0]));
  }
  { // Outlined triangle with different colors.
    struct egg_draw_line vtxv[]={
      {12, 1,12,10,0xff,0x00,0x00,0xff},
      {12,10,21, 1,0x00,0xff,0x00,0xff},
      {21, 1,12, 1,0x00,0x00,0xff,0xff},
    };
    egg_draw_line(1,vtxv,sizeof(vtxv)/sizeof(vtxv[0]));
  }
  { // Filled rectangle.
    struct egg_draw_rect vtxv[]={
      { 1,12,10,10,0x40,0x30,0x20,0xff},
      {12,12,10,10,0x20,0x30,0x40,0xff},
    };
    egg_draw_rect(1,vtxv,sizeof(vtxv)/sizeof(vtxv[0]));
  }
  { // Two triangles separated by a very thin gap.
    struct egg_draw_trig vtxv[]={
      { 1,23, 1,43,20,23,0x80,0x00,0x00,0xff},
      { 3,43,22,23,22,43,0x00,0x00,0x80,0xff},
    };
    egg_draw_trig(1,vtxv,sizeof(vtxv)/sizeof(vtxv[0]));
  }
  
  /* Top and left of center: The same primitives, drawn with per-vertex alpha.
   */
  { // Bunch of diagonal lines to provide a background.
    struct egg_draw_line vtxv[]={
      { 50, 1, 50, 1, 0x20,0x00,0x00,0xff},
      { 54, 1, 50, 5, 0x24,0x00,0x00,0xff},
      { 58, 1, 50, 9, 0x28,0x00,0x00,0xff},
      { 62, 1, 50,13, 0x2c,0x00,0x00,0xff},
      { 66, 1, 50,17, 0x30,0x00,0x00,0xff},
      { 70, 1, 50,21, 0x34,0x00,0x00,0xff},
      { 74, 1, 50,25, 0x38,0x00,0x00,0xff},
      { 78, 1, 50,29, 0x3c,0x00,0x00,0xff},
      { 82, 1, 50,33, 0x40,0x00,0x00,0xff},
      { 86, 1, 50,37, 0x44,0x00,0x00,0xff},
      { 90, 1, 50,41, 0x48,0x00,0x00,0xff},
      { 94, 1, 50,45, 0x4c,0x00,0x00,0xff},
      { 94, 5, 54,45, 0x50,0x00,0x00,0xff},
      { 94, 9, 58,45, 0x54,0x00,0x00,0xff},
      { 94,13, 62,45, 0x58,0x00,0x00,0xff},
      { 94,17, 66,45, 0x5c,0x00,0x00,0xff},
      { 94,21, 70,45, 0x60,0x00,0x00,0xff},
      { 94,25, 74,45, 0x64,0x00,0x00,0xff},
      { 94,29, 78,45, 0x68,0x00,0x00,0xff},
      { 94,33, 82,45, 0x6c,0x00,0x00,0xff},
      { 94,37, 86,45, 0x70,0x00,0x00,0xff},
      { 94,41, 90,45, 0x74,0x00,0x00,0xff},
      { 94,45, 94,45, 0x78,0x00,0x00,0xff},
    };
    egg_draw_line(1,vtxv,sizeof(vtxv)/sizeof(vtxv[0]));
  }
  { // Black outlined rectangle.
    struct egg_draw_line vtxv[]={
      {51, 1,51,10, 0x00,0x00,0x00,0xe0},
      {51,10,60,10, 0x00,0x00,0x00,0xe0},
      {60,10,60, 1, 0x00,0x00,0x00,0xe0},
      {60, 1,51, 1, 0x00,0x00,0x00,0xe0},
    };
    egg_draw_line(1,vtxv,sizeof(vtxv)/sizeof(vtxv[0]));
  }
  { // Outlined triangle with different colors.
    struct egg_draw_line vtxv[]={
      {62, 1,62,10,0xff,0x00,0x00,0xe0},
      {62,10,71, 1,0x00,0xff,0x00,0xe0},
      {71, 1,62, 1,0x00,0x00,0xff,0xe0},
    };
    egg_draw_line(1,vtxv,sizeof(vtxv)/sizeof(vtxv[0]));
  }
  { // Filled rectangle.
    struct egg_draw_rect vtxv[]={
      {51,12,10,10,0x40,0x30,0x20,0xe0},
      {62,12,10,10,0x20,0x30,0x40,0xe0},
    };
    egg_draw_rect(1,vtxv,sizeof(vtxv)/sizeof(vtxv[0]));
  }
  { // Two triangles separated by a very thin gap.
    struct egg_draw_trig vtxv[]={
      {51,23,51,43,70,23,0x80,0x00,0x00,0xe0},
      {53,43,72,23,72,43,0x00,0x00,0x80,0xe0},
    };
    egg_draw_trig(1,vtxv,sizeof(vtxv)/sizeof(vtxv[0]));
  }
  
  /* Top and right of center: The same primitives, drawn with global alpha. Should match the per-pixel run exactly.
   */
  { // Bunch of diagonal lines to provide a background.
    struct egg_draw_line vtxv[]={
      {100, 1,100, 1, 0x20,0x00,0x00,0xff},
      {104, 1,100, 5, 0x24,0x00,0x00,0xff},
      {108, 1,100, 9, 0x28,0x00,0x00,0xff},
      {112, 1,100,13, 0x2c,0x00,0x00,0xff},
      {116, 1,100,17, 0x30,0x00,0x00,0xff},
      {120, 1,100,21, 0x34,0x00,0x00,0xff},
      {124, 1,100,25, 0x38,0x00,0x00,0xff},
      {128, 1,100,29, 0x3c,0x00,0x00,0xff},
      {132, 1,100,33, 0x40,0x00,0x00,0xff},
      {136, 1,100,37, 0x44,0x00,0x00,0xff},
      {140, 1,100,41, 0x48,0x00,0x00,0xff},
      {144, 1,100,45, 0x4c,0x00,0x00,0xff},
      {144, 5,104,45, 0x50,0x00,0x00,0xff},
      {144, 9,108,45, 0x54,0x00,0x00,0xff},
      {144,13,112,45, 0x58,0x00,0x00,0xff},
      {144,17,116,45, 0x5c,0x00,0x00,0xff},
      {144,21,120,45, 0x60,0x00,0x00,0xff},
      {144,25,124,45, 0x64,0x00,0x00,0xff},
      {144,29,128,45, 0x68,0x00,0x00,0xff},
      {144,33,132,45, 0x6c,0x00,0x00,0xff},
      {144,37,136,45, 0x70,0x00,0x00,0xff},
      {144,41,140,45, 0x74,0x00,0x00,0xff},
      {144,45,144,45, 0x78,0x00,0x00,0xff},
    };
    egg_draw_line(1,vtxv,sizeof(vtxv)/sizeof(vtxv[0]));
  }
  egg_draw_globals(0,0xe0);
  { // Black outlined rectangle.
    struct egg_draw_line vtxv[]={
      {101, 1,101,10, 0x00,0x00,0x00,0xff},
      {101,10,110,10, 0x00,0x00,0x00,0xff},
      {110,10,110, 1, 0x00,0x00,0x00,0xff},
      {110, 1,101, 1, 0x00,0x00,0x00,0xff},
    };
    egg_draw_line(1,vtxv,sizeof(vtxv)/sizeof(vtxv[0]));
  }
  { // Outlined triangle with different colors.
    struct egg_draw_line vtxv[]={
      {112, 1,112,10,0xff,0x00,0x00,0xff},
      {112,10,121, 1,0x00,0xff,0x00,0xff},
      {121, 1,112, 1,0x00,0x00,0xff,0xff},
    };
    egg_draw_line(1,vtxv,sizeof(vtxv)/sizeof(vtxv[0]));
  }
  { // Filled rectangle.
    struct egg_draw_rect vtxv[]={
      {101,12,10,10,0x40,0x30,0x20,0xff},
      {112,12,10,10,0x20,0x30,0x40,0xff},
    };
    egg_draw_rect(1,vtxv,sizeof(vtxv)/sizeof(vtxv[0]));
  }
  { // Two triangles separated by a very thin gap.
    struct egg_draw_trig vtxv[]={
      {101,23,101,43,120,23,0x80,0x00,0x00,0xff},
      {103,43,122,23,122,43,0x00,0x00,0x80,0xff},
    };
    egg_draw_trig(1,vtxv,sizeof(vtxv)/sizeof(vtxv[0]));
  }
  egg_draw_globals(0,0xff);
  
  /* Horizontal strip of rectangles with increasing global tint.
   */
  {
    struct egg_draw_rect vtx={1,50,20,20,0xff,0xff,0x00,0xff};
    uint8_t tint=0x00;
    int i=8; while (i-->0) {
      egg_draw_globals(tint,0xff);
      egg_draw_rect(1,&vtx,1);
      tint+=0x20;
      vtx.x+=25;
    }
    egg_draw_globals(0,0xff);
  }
}

/* New.
 */
 
struct menu *menu_spawn_video_primitives() {
  struct menu *menu=menu_spawn(sizeof(struct menu_primitives));
  if (!menu) return 0;
  
  menu->del=_primitives_del;
  menu->input=_primitives_input;
  menu->update=_primitives_update;
  menu->render=_primitives_render;
  
  return menu;
}
