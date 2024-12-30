/* picklist.h
 * Manages the entire UI for a menu, with a list of selectable text labels.
 * I expect most menus to work this way, especially intermediate ones.
 * We don't do scrolling. The 180-pixel framebuffer can display 19 options.
 */

#ifndef PICKLIST_H
#define PICKLIST_H

struct picklist {
  struct picklist_option {
    int texid;
    int texw,texh;
    int optid; // Unique. Owner may provide it, or we make one up.
    void *userdata; // Opaque, owner provides.
    void (*cb)(int optid,void *userdata);
  } *optionv;
  int optionc,optiona;
  int optionp;
  // Default callback for options that don't have one. Set directly:
  void *userdata;
  void (*cb)(int optid,void *userdata);
};

void picklist_del(struct picklist *picklist);
struct picklist *picklist_new();

/* Input events.
 * If you want the defaults (SOUTH to activate, dpad to move), use the convenience picklist_input().
 */
void picklist_activate(struct picklist *picklist);
void picklist_move(struct picklist *picklist,int dx,int dy);
void picklist_input(struct picklist *picklist,int input,int pvinput);

void picklist_update(struct picklist *picklist,double elapsed);
void picklist_render(struct picklist *picklist);

/* Add a new option at the end of the list.
 * If (optid>0), we assert it is not in use yet and use it.
 * Otherwise we make up an ID >0.
 * Either way, we return the effective optid. Or <0 on errors.
 * (cb) fires when this option gets activated.
 * We do not provide a cleanup hook for (userdata), you have to figure it out if you need that.
 */
int picklist_add_option(
  struct picklist *picklist,
  const char *label,int labelc,
  int optid,void *userdata,
  void (*cb)(int optid,void *userdata)
);

int picklist_replace_label(struct picklist *picklist,int optid,const char *src,int srcc);

int picklist_get_optid(const struct picklist *picklist);

#endif
