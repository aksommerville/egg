#include "../demo.h"
#include "picklist.h"

/* Delete.
 */
 
static void picklist_option_cleanup(struct picklist_option *option) {
  egg_texture_del(option->texid);
}
 
void picklist_del(struct picklist *picklist) {
  if (!picklist) return;
  if (picklist->optionv) {
    while (picklist->optionc-->0) picklist_option_cleanup(picklist->optionv+picklist->optionc);
    free(picklist->optionv);
  }
  free(picklist);
}

/* New.
 */
 
struct picklist *picklist_new() {
  struct picklist *picklist=calloc(1,sizeof(struct picklist));
  if (!picklist) return 0;
  picklist->optionp=0;
  return picklist;
}

/* Input.
 */

void picklist_activate(struct picklist *picklist) {
  if (picklist->optionp<0) return;
  if (picklist->optionp>=picklist->optionc) return;
  const struct picklist_option *option=picklist->optionv+picklist->optionp;
  void *userdata=option->userdata?option->userdata:picklist->userdata;
  if (option->cb) option->cb(option->optid,userdata);
  else if (picklist->cb) picklist->cb(option->optid,userdata);
}

void picklist_move(struct picklist *picklist,int dx,int dy) {
  if (dy) {
    picklist->optionp+=dy;
    if (picklist->optionc<1) picklist->optionp=-1;
    else if (picklist->optionp<0) picklist->optionp=picklist->optionc-1;
    else if (picklist->optionp>=picklist->optionc) picklist->optionp=0;
  }
}

void picklist_input(struct picklist *picklist,int input,int pvinput) {
  if ((input&EGG_BTN_SOUTH)&&!(pvinput&EGG_BTN_SOUTH)) picklist_activate(picklist);
  if ((input&EGG_BTN_LEFT)&&!(pvinput&EGG_BTN_LEFT)) picklist_move(picklist,-1,0);
  if ((input&EGG_BTN_RIGHT)&&!(pvinput&EGG_BTN_RIGHT)) picklist_move(picklist,1,0);
  if ((input&EGG_BTN_UP)&&!(pvinput&EGG_BTN_UP)) picklist_move(picklist,0,-1);
  if ((input&EGG_BTN_DOWN)&&!(pvinput&EGG_BTN_DOWN)) picklist_move(picklist,0,1);
}

/* Update.
 */

void picklist_update(struct picklist *picklist,double elapsed) {
}

/* Render.
 */
 
void picklist_render(struct picklist *picklist) {
  int16_t dstx=4,dsty=4;
  if ((picklist->optionp>=0)&&(picklist->optionp<picklist->optionc)) {
    const struct picklist_option *selected=picklist->optionv+picklist->optionp;
    int16_t boxy=dsty;
    const struct picklist_option *option=picklist->optionv;
    int i=picklist->optionp;
    for (;i-->0;option++) boxy+=option->texh;
    graf_draw_rect(&g.graf,0,boxy-1,g.fbw,selected->texh+1,0x800000ff);
  }
  const struct picklist_option *option=picklist->optionv;
  int i=picklist->optionc;
  for (;i-->0;option++) {
    graf_draw_decal(&g.graf,option->texid,dstx,dsty,0,0,option->texw,option->texh,0);
    dsty+=option->texh;
  }
}

/* Get option.
 */
 
static struct picklist_option *picklist_option_by_optid(struct picklist *picklist,int optid) {
  struct picklist_option *option=picklist->optionv;
  int i=picklist->optionc;
  for (;i-->0;option++) {
    if (option->optid==optid) return option;
  }
  return 0;
}

/* Unused optid.
 */
 
static int picklist_unused_optid(struct picklist *picklist) {
  // A good first guess is (optionc+1). We're not required to produce a contiguous set or anything.
  int optid=picklist->optionc+1;
  for (;;optid++) {
    if (!picklist_option_by_optid(picklist,optid)) return optid;
  }
}

/* Add option.
 */
 
int picklist_add_option(
  struct picklist *picklist,
  const char *label,int labelc,
  int optid,void *userdata,
  void (*cb)(int optid,void *userdata)
) {
  if (!picklist) return -1;
  if (!label) labelc=0; else if (labelc<0) { labelc=0; while (label[labelc]) labelc++; }
  if (!labelc) { label=" "; labelc=1; }
  if (picklist->optionc>=picklist->optiona) {
    int na=picklist->optiona+8;
    if (na>INT_MAX/sizeof(struct picklist_option)) return -1;
    void *nv=realloc(picklist->optionv,sizeof(struct picklist_option)*na);
    if (!nv) return -1;
    picklist->optionv=nv;
    picklist->optiona=na;
  }
  if (optid<1) optid=picklist_unused_optid(picklist);
  struct picklist_option *option=picklist->optionv+picklist->optionc++;
  memset(option,0,sizeof(struct picklist_option));
  option->optid=optid;
  option->userdata=userdata;
  option->cb=cb;
  if ((option->texid=font_tex_oneline(g.font,label,labelc,g.fbw,0xffffffff))<1) {
    picklist->optionc--;
    return -1;
  }
  egg_texture_get_status(&option->texw,&option->texh,option->texid);
  return option->optid;
}

/* Replace label.
 */

int picklist_replace_label(struct picklist *picklist,int optid,const char *src,int srcc) {
  if (!picklist) return -1;
  struct picklist_option *option=picklist_option_by_optid(picklist,optid);
  if (!option) return -1;
  if (!src) srcc=0; else if (src<0) { srcc=0; while (src[srcc]) srcc++; }
  if (!srcc) { src=" "; srcc=1; }
  int texid=font_tex_oneline(g.font,src,srcc,g.fbw,0xffffffff);
  if (texid<1) return -1;
  egg_texture_del(option->texid);
  option->texid=texid;
  egg_texture_get_status(&option->texw,&option->texh,option->texid);
  return 0;
}

/* Get selected option.
 */

int picklist_get_optid(const struct picklist *picklist) {
  if (!picklist) return 0;
  if ((picklist->optionp<0)||(picklist->optionp>=picklist->optionc)) return 0;
  return picklist->optionv[picklist->optionp].optid;
}
