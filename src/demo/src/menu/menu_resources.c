#include "../demo.h"

#define ROWC 19

struct menu_resources {
  struct menu hdr;
  struct rom_res *resv;
  int resc,resa;
  int resp;
  int viewp; // First displayed resource.
  int texid; // Full screen of text (g.fbw,g.fbh)
};

#define MENU ((struct menu_resources*)menu)

/* Delete.
 */
 
static void _resources_del(struct menu *menu) {
  if (MENU->resv) free(MENU->resv);
  egg_texture_del(MENU->texid);
}

/* Generate text.
 * We'll always return something in 0..dsta.
 */

static int resources_repr_uint(char *dst,int dsta,int v) {
  int dstc=0;
  if (v>=1000000000) { if (dstc<dsta) dst[dstc]='0'+(v/1000000000)%10; dstc++; }
  if (v>=100000000 ) { if (dstc<dsta) dst[dstc]='0'+(v/100000000 )%10; dstc++; }
  if (v>=10000000  ) { if (dstc<dsta) dst[dstc]='0'+(v/10000000  )%10; dstc++; }
  if (v>=1000000   ) { if (dstc<dsta) dst[dstc]='0'+(v/1000000   )%10; dstc++; }
  if (v>=100000    ) { if (dstc<dsta) dst[dstc]='0'+(v/100000    )%10; dstc++; }
  if (v>=10000     ) { if (dstc<dsta) dst[dstc]='0'+(v/10000     )%10; dstc++; }
  if (v>=1000      ) { if (dstc<dsta) dst[dstc]='0'+(v/1000      )%10; dstc++; }
  if (v>=100       ) { if (dstc<dsta) dst[dstc]='0'+(v/100       )%10; dstc++; }
  if (v>=10        ) { if (dstc<dsta) dst[dstc]='0'+(v/10        )%10; dstc++; }
  if (dstc<dsta) dst[dstc]='0'+v%10; dstc++;
  return dstc;
}
 
static int resources_repr_tid(char *dst,int dsta,int tid) {
  if (dsta<1) return 0;
  const char *src=0;
  switch (tid) {
    #define _(tag) case EGG_TID_##tag: src=#tag; break;
    EGG_TID_FOR_EACH
    EGG_TID_FOR_EACH_CUSTOM
    #undef _
  }
  if (!src) return resources_repr_uint(dst,dsta,tid);
  int srcc=0; while (src[srcc]) srcc++;
  if (srcc>dsta) srcc=dsta;
  memcpy(dst,src,srcc);
  return srcc;
}

static int resources_repr_rid(char *dst,int dsta,int tid,int rid) {
  if (tid==EGG_TID_strings) { // Should we allow language for other resources too? Any resource can use it, but we can't tell whether it's accidental.
    int lang=rid>>6;
    char lname[2];
    EGG_STRING_FROM_LANG(lname,lang)
    if (lang&&(rid&0x3f)&&(lname[0]>='a')&&(lname[0]<='z')&&(lname[1]>='a')&&(lname[1]<='z')) {
      if (dsta>=5) {
        rid&=0x3f;
        dst[0]=lname[0];
        dst[1]=lname[1];
        dst[2]='-';
        if (rid>=10) {
          dst[3]='0'+rid/10;
          dst[4]='0'+rid%10;
          return 5;
        } else {
          dst[3]='0'+rid;
          return 4;
        }
      }
    }
  }
  return resources_repr_uint(dst,dsta,rid);
}

/* Render the current page into an RGBA buffer the size of the global framebuffer.
 */
 
static void resources_render_text(void *dst,struct menu *menu) {
  int rowh=font_get_line_height(g.font);
  int dsty=(g.fbh>>1)-((rowh*ROWC)>>1);
  int resp=MENU->viewp;
  int i=ROWC;
  for (;i-->0;dsty+=rowh,resp++) {
    if (resp>=MENU->resc) break;
    const struct rom_res *res=MENU->resv+resp;
    char tmp[32];
    int tmpc=resources_repr_tid(tmp,sizeof(tmp),res->tid);
    font_render_string(dst,g.fbw,g.fbh,g.fbw<<2,5,dsty,g.font,tmp,tmpc,0xffffffff);
    tmpc=resources_repr_rid(tmp,sizeof(tmp),res->tid,res->rid);
    font_render_string(dst,g.fbw,g.fbh,g.fbw<<2,60,dsty,g.font,tmp,tmpc,0xffffffff);
    tmpc=resources_repr_uint(tmp,sizeof(tmp),res->c);
    font_render_string(dst,g.fbw,g.fbh,g.fbw<<2,100,dsty,g.font,tmp,tmpc,0xffffffff);
  }
}

/* Generate the text image for current page.
 * Caller prepares (resv,resc,resp), we manage (viewp,texid).
 * Noop if we end up on the page we're already showing.
 */
 
static int resources_refresh(struct menu *menu) {
  if (MENU->resc<1) return 0;
  if (MENU->resp<0) MENU->resp=0;
  else if (MENU->resp>=MENU->resc) MENU->resp=MENU->resc-1;
  int viewp=(MENU->resp/ROWC)*ROWC;
  if (viewp==MENU->viewp) return 0;
  MENU->viewp=viewp;
  char *pixels=calloc(g.fbw<<2,g.fbh);
  if (!pixels) return -1;
  resources_render_text(pixels,menu);
  if (!MENU->texid) MENU->texid=egg_texture_new();
  egg_texture_load_raw(MENU->texid,g.fbw,g.fbh,g.fbw<<2,pixels,g.fbw*g.fbh*4);
  free(pixels);
  return 0;
}

/* Move cursor.
 * Horizontal for pages, vertical for resources.
 */
 
static void resources_move(struct menu *menu,int dx,int dy) {
  MENU->resp+=dy+dx*ROWC;
  resources_refresh(menu);
}

/* Input.
 */
 
static void _resources_input(struct menu *menu,int input,int pvinput) {
  if ((input&EGG_BTN_WEST)&&!(pvinput&EGG_BTN_WEST)) menu->defunct=1;
  if ((input&EGG_BTN_LEFT)&&!(pvinput&EGG_BTN_LEFT)) resources_move(menu,-1,0);
  if ((input&EGG_BTN_RIGHT)&&!(pvinput&EGG_BTN_RIGHT)) resources_move(menu,1,0);
  if ((input&EGG_BTN_UP)&&!(pvinput&EGG_BTN_UP)) resources_move(menu,0,-1);
  if ((input&EGG_BTN_DOWN)&&!(pvinput&EGG_BTN_DOWN)) resources_move(menu,0,1);
}

/* Update.
 */
 
static void _resources_update(struct menu *menu,double elapsed) {
}

/* Render.
 */
 
static void _resources_render(struct menu *menu) {
  int cursorrow=MENU->resp-MENU->viewp;
  if ((cursorrow>=0)&&(cursorrow<ROWC)) {
    int rowh=font_get_line_height(g.font);
    int y=(g.fbh>>1)-((rowh*ROWC)>>1)+rowh*cursorrow;
    graf_draw_rect(&g.graf,0,y,g.fbw,rowh,0x800000ff);
  }
  graf_draw_decal(&g.graf,MENU->texid,0,0,0,0,g.fbw,g.fbh,0);
}

/* Add resource to private list.
 */
 
static int menu_add_res(struct menu *menu,const struct rom_res *src) {
  if (MENU->resc>=MENU->resa) {
    int na=MENU->resa+128;
    if (na>INT_MAX/sizeof(struct rom_res)) return -1;
    void *nv=realloc(MENU->resv,sizeof(struct rom_res)*na);
    if (!nv) return -1;
    MENU->resv=nv;
    MENU->resa=na;
  }
  memcpy(MENU->resv+MENU->resc++,src,sizeof(struct rom_res));
  return 0;
}

/* New.
 */
 
struct menu *menu_spawn_resources() {
  struct menu *menu=menu_spawn(sizeof(struct menu_resources));
  if (!menu) return 0;
  
  menu->del=_resources_del;
  menu->input=_resources_input;
  menu->update=_resources_update;
  menu->render=_resources_render;
  
  // We could call egg_get_rom(), but it's already been done by main, use that.
  struct rom_reader reader;
  if (rom_reader_init(&reader,g.rom,g.romc)>=0) {
    struct rom_res *src;
    while (src=rom_reader_next(&reader)) {
      if (menu_add_res(menu,src)<0) { menu->defunct=1; return 0; }
    }
  }
  
  MENU->viewp=-1; // Force redraw.
  resources_refresh(menu);
  
  return menu;
}
