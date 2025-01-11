/* demo.h
 * Egg demo ROM, internal header.
 */
 
#ifndef DEMO_H
#define DEMO_H

#include "egg/egg.h"
#include "demo_symbols.h"
#include "egg_rom_toc.h" /* Generated per data files during build. */
#include "opt/stdlib/egg-stdlib.h"
#include "opt/graf/graf.h"
#include "opt/text/text.h"
#include "opt/rom/rom.h"

#define MENU_LIMIT 8 /* Maximum depth of menu stack. Completely arbitrary. */

/* Menus that don't implement (input) will be dismissed on B.
 * If you do implement (input), you must self-dismiss, recommend on B.
 * The preferred way to self-dismiss is to set (defunct) nonzero and let main delete you at a safe time.
 * The background is cleared before (render), to a sensible dark blue.
 * Only the topmost menu receives events and renders.
 */
struct menu {
  int defunct;
  void (*del)(struct menu *menu);
  void (*input)(struct menu *menu,int input,int pvinput);
  void (*update)(struct menu *menu,double elapsed);
  void (*render)(struct menu *menu);
};

extern struct g {
  int fbw,fbh;
  void *rom;
  int romc;
  struct graf graf;
  struct texcache texcache;
  struct font *font;
  int pvinput;
  struct menu *menuv[MENU_LIMIT];
  int menuc;
} g;

struct menu *menu_spawn(int objlen);
struct menu *menu_spawn_main();
// From main:
struct menu *menu_spawn_video();
struct menu *menu_spawn_audio();
struct menu *menu_spawn_input();
struct menu *menu_spawn_storage();
struct menu *menu_spawn_misc();
struct menu *menu_spawn_regression();
// From video:
struct menu *menu_spawn_video_primitives();
struct menu *menu_spawn_video_transforms();
struct menu *menu_spawn_video_aux();
struct menu *menu_spawn_video_stress();
// From storage:
struct menu *menu_spawn_resources();
// From regression:
struct menu *menu_spawn_20250110();

#endif
