#include "../demo.h"
#include "../ui/picklist.h"

#define OPTID_LOG 1
#define OPTID_TERM_OK 2
#define OPTID_TERM_ERR 3
#define OPTID_LANG 4

struct menu_misc {
  struct menu hdr;
  struct picklist *picklist;
  int *langv;
  int langc,langa;
  int langp;
  int text_texid,text_w,text_h;
};

#define MENU ((struct menu_misc*)menu)

/* Delete.
 */
 
static void _misc_del(struct menu *menu) {
  picklist_del(MENU->picklist);
  if (MENU->langv) free(MENU->langv);
  egg_texture_del(MENU->text_texid);
}

/* Replace label for the "Language" option.
 */
 
static void misc_replace_language_label(struct menu *menu) {
  if ((MENU->langp<0)||(MENU->langp>=MENU->langc)) return;
  int lang=MENU->langv[MENU->langp];
  char label[]="Language: XX";
  EGG_STRING_FROM_LANG(label+10,lang)
  picklist_replace_label(MENU->picklist,OPTID_LANG,label,sizeof(label)-1);
}

/* Replace the language-specific text dump.
 */
 
static void misc_replace_text(struct menu *menu) {
  strings_check_language();
  egg_texture_del(MENU->text_texid);
  MENU->text_texid=font_texres_multiline(g.font,1,10,g.fbw-4,g.fbh>>1,0xc0c0c0ff);
  egg_texture_get_status(&MENU->text_w,&MENU->text_h,MENU->text_texid);
}

/* Input.
 */
 
static void misc_adjust(struct menu *menu,int d) {
  switch (picklist_get_optid(MENU->picklist)) {
    case OPTID_LANG: {
        if (MENU->langc<1) return;
        MENU->langp+=d;
        if (MENU->langp<0) MENU->langp=MENU->langc-1;
        else if (MENU->langp>=MENU->langc) MENU->langp=0;
        egg_set_language(MENU->langv[MENU->langp]);
        misc_replace_language_label(menu);
        misc_replace_text(menu);
      } break;
  }
}
 
static void _misc_input(struct menu *menu,int input,int pvinput) {
  if ((input&EGG_BTN_WEST)&&!(pvinput&EGG_BTN_WEST)) menu->defunct=1;
  if ((input&EGG_BTN_LEFT)&&!(pvinput&EGG_BTN_LEFT)) { misc_adjust(menu,-1); return; }
  if ((input&EGG_BTN_RIGHT)&&!(pvinput&EGG_BTN_RIGHT)) { misc_adjust(menu,1); return; }
  picklist_input(MENU->picklist,input,pvinput);
}

/* Update.
 */
 
static void _misc_update(struct menu *menu,double elapsed) {
  picklist_update(MENU->picklist,elapsed);
}

/* Render.
 */
 
static void misc_render_string(struct menu *menu,int dstx,int dsty,int texid,const char *src,int srcc) {
  for (;srcc-->0;src++,dstx+=8) {
    int ch=*src;
    if ((ch>=0x61)&&(ch<=0x7a)) ch-=0x20;
    ch-=0x30;
    if ((ch<0)||(ch>=0x30)) continue;
    graf_draw_tile(&g.graf,texid,dstx,dsty,0xc0+ch,0);
  }
}
 
static void _misc_render(struct menu *menu) {
  picklist_render(MENU->picklist);
  graf_draw_decal(&g.graf,MENU->text_texid,2,g.fbh-MENU->text_h-2,0,0,MENU->text_w,MENU->text_h,0);
  int texid=texcache_get_image(&g.texcache,RID_image_tiles8);
  char tmp[64];
  int tmpc;
  tmpc=snprintf(tmp,sizeof(tmp),"%f",egg_time_real());
  if ((tmpc>0)&&(tmpc<sizeof(tmp))) {
    misc_render_string(menu,120,8,texid,tmp,tmpc);
  }
  int t[7]={0};
  egg_time_local(t,7);
  tmpc=snprintf(tmp,sizeof(tmp),"%04d-%02d-%02dT%02d:%02d:%02d.%03d",t[0],t[1],t[2],t[3],t[4],t[5],t[6]);
  if ((tmpc>0)&&(tmpc<sizeof(tmp))) {
    misc_render_string(menu,120,18,texid,tmp,tmpc);
  }
}

/* Initialize list of available languages.
 * We also call egg_get_language() to set the initial value.
 */
 
static int misc_add_language(struct menu *menu,int lang) {
  int i=MENU->langc;
  while (i-->0) if (MENU->langv[i]==lang) return 0;
  if (MENU->langc>=MENU->langa) {
    int na=MENU->langa+8;
    if (na>INT_MAX/sizeof(int)) return -1;
    void *nv=realloc(MENU->langv,sizeof(int)*na);
    if (!nv) return -1;
    MENU->langv=nv;
    MENU->langa=na;
  }
  MENU->langv[MENU->langc++]=lang;
  return 0;
}
 
static void misc_gather_languages(struct menu *menu) {
  const char *src=0;
  int srcc=rom_lookup_metadata(&src,g.rom,g.romc,"lang",4,0);
  int srcp=0;
  while (srcp<srcc) {
    if (src[srcp]==',') { srcp++; continue; }
    const char *token=src+srcp;
    int tokenc=0;
    while ((srcp<srcc)&&(src[srcp++]!=',')) tokenc++;
    while (tokenc&&((unsigned char)token[tokenc-1]<=0x20)) tokenc--;
    while (tokenc&&((unsigned char)token[0]<=0x20)) { token++; tokenc--; }
    if ((tokenc==2)&&(token[0]>='a')&&(token[0]<='z')&&(token[1]>='a')&&(token[1]<='z')) {
      misc_add_language(menu,EGG_LANG_FROM_STRING(token));
    } else {
      fprintf(stderr,"!!! Invalid token in metadata:1:lang: '%.*s'\n",tokenc,token);
    }
  }
  // Also, the current language must be in the list. eg if there was no 'lang' field.
  int current=egg_get_language();
  misc_add_language(menu,current);
  MENU->langp=0;
  int i=MENU->langc;
  while (i-->0) {
    if (MENU->langv[i]==current) {
      MENU->langp=i;
      break;
    }
  }
}

/* Picklist callback.
 */
 
static void misc_cb_picklist(int optid,void *userdata) {
  struct menu *menu=userdata;
  switch (optid) {
    case OPTID_LOG: egg_log("This message was logged by the demo's Miscellaneous menu."); break;
    case OPTID_TERM_OK: egg_terminate(0); break;
    case OPTID_TERM_ERR: egg_terminate(1); break;
    case OPTID_LANG: break;
  }
}

/* New.
 */
 
struct menu *menu_spawn_misc() {
  struct menu *menu=menu_spawn(sizeof(struct menu_misc));
  if (!menu) return 0;
  
  menu->del=_misc_del;
  menu->input=_misc_input;
  menu->update=_misc_update;
  menu->render=_misc_render;
  
  if (!(MENU->picklist=picklist_new())) { menu->defunct=1; return 0; }
  MENU->picklist->cb=misc_cb_picklist;
  MENU->picklist->userdata=menu;
  picklist_add_option(MENU->picklist,"Log",-1,OPTID_LOG,0,0);
  picklist_add_option(MENU->picklist,"Exit Success",-1,OPTID_TERM_OK,0,0);
  picklist_add_option(MENU->picklist,"Exit Error (1)",-1,OPTID_TERM_ERR,0,0);
  picklist_add_option(MENU->picklist,"",0,OPTID_LANG,0,0);
  
  misc_gather_languages(menu);
  misc_replace_language_label(menu);
  misc_replace_text(menu);
  
  return menu;
}
