/* incfg.h
 * Interactive input configuration.
 */
 
#ifndef INCFG_H
#define INCFG_H

#define INCFG_FBW 160
#define INCFG_FBH 90

struct incfg_device;

struct incfg {
  int texid_main;
  int texid_bits;
  int time_warning; // 1..9, or zero for none
  
  /* (devid) goes nonzero after we pick one, and after that (devicev) will have exactly one member.
   * Before selecting a device, there can be multiple in (devicev). Those are devices that have delivered inconsequential events.
   */
  int devid;
  struct incfg_device *devicev;
  int devicec,devicea;
  
  /* If buttonp>=0, we are waiting for an event on that button. Index in private incfg_buttonv.
   * (waitstage) is (0,1)=(press,release). We'll only ask for one press, unlike previous times I've done this.
   */
  int buttonp;
  int waitstage;
  int waitbtnid;
  double waitelapsed;
};

void incfg_del(struct incfg *incfg);

struct incfg *incfg_new();

/* If we are installed as (eggrt.incfg), we are allowed to self-delete during update.
 */
int incfg_update(struct incfg *incfg,double elapsed);

/* incfg will use the same render context as the game, but will manage its own textures.
 * Including its main!
 * So don't call render_draw_to_main(); we do it.
 */
int incfg_render(struct incfg *incfg);

#endif
