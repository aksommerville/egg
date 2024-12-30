#include "../demo.h"
#include "../ui/picklist.h"

struct menu_storage {
  struct menu hdr;
  struct picklist *picklist;
  int texid,texw,texh;
};

#define MENU ((struct menu_storage*)menu)

/* Delete.
 */
 
static void _storage_del(struct menu *menu) {
  picklist_del(MENU->picklist);
  egg_texture_del(MENU->texid);
}

/* Input.
 */
 
static void _storage_input(struct menu *menu,int input,int pvinput) {
  if ((input&EGG_BTN_WEST)&&!(pvinput&EGG_BTN_WEST)) menu->defunct=1;
  picklist_input(MENU->picklist,input,pvinput);
}

/* Update.
 */
 
static void _storage_update(struct menu *menu,double elapsed) {
  picklist_update(MENU->picklist,elapsed);
}

/* Render.
 */
 
static void _storage_render(struct menu *menu) {
  picklist_render(MENU->picklist);
  graf_draw_decal(&g.graf,MENU->texid,g.fbw-5-MENU->texw,g.fbh-5-MENU->texh,0,0,MENU->texw,MENU->texh,0);
}

/* Read entire store and print to a private texture.
 */
 
static void storage_read_store(struct menu *menu) {
  MENU->texw=180;
  MENU->texh=80;
  int stride=MENU->texw<<2;
  char *pixels=calloc(stride,MENU->texh);
  if (!pixels) return;
  int rowh=font_get_line_height(g.font);
  int dsty=0;
  int i=0; for (;;i++,dsty+=rowh) {
    char k[256];
    int kc=egg_store_key_by_index(k,sizeof(k),i);
    if ((kc<1)||(kc>sizeof(k))) break;
    char v[256];
    int vc=egg_store_get(v,sizeof(v),k,kc);
    if ((vc<0)||(vc>sizeof(v))) vc=0;
    int dstx=font_render_string(pixels,MENU->texw,MENU->texh,stride,0,dsty,g.font,k,kc,0xa0a0a0ff);
    dstx+=font_render_string(pixels,MENU->texw,MENU->texh,stride,dstx,dsty,g.font,": ",2,0xffffffff);
    dstx+=font_render_string(pixels,MENU->texw,MENU->texh,stride,dstx,dsty,g.font,v,vc,0xffffc0ff);
  }
  if (!MENU->texid) MENU->texid=egg_texture_new();
  egg_texture_load_raw(MENU->texid,EGG_TEX_FMT_RGBA,MENU->texw,MENU->texh,stride,pixels,stride*MENU->texh);
  free(pixels);
}

/* Picklist callbacks.
 */
 
static void storage_cb_resources(int optid,void *userdata) {
  menu_spawn_resources();
}

// optid: 0x01=field(0=A,1=B), 0x02=value(0=clear,1=set)
static void storage_cb_store(int optid,void *userdata) {
  struct menu *menu=userdata;
  const char *key=(optid&1)?"fieldB":"fieldA";
  char value[32];
  int valuec=0;
  if (optid&2) {
    int bits[7]={0};
    egg_time_local(bits,7);
    value[ 0]='0'+(bits[0]/1000)%10;
    value[ 1]='0'+(bits[0]/100 )%10;
    value[ 2]='0'+(bits[0]/10  )%10;
    value[ 3]='0'+(bits[0]     )%10;
    value[ 4]='-';
    value[ 5]='0'+(bits[1]/10  )%10;
    value[ 6]='0'+(bits[1]     )%10;
    value[ 7]='-';
    value[ 8]='0'+(bits[2]/10  )%10;
    value[ 9]='0'+(bits[2]     )%10;
    value[10]='T';
    value[11]='0'+(bits[3]/10  )%10;
    value[12]='0'+(bits[3]     )%10;
    value[13]=':';
    value[14]='0'+(bits[4]/10  )%10;
    value[15]='0'+(bits[4]     )%10;
    value[16]=':';
    value[17]='0'+(bits[5]/10  )%10;
    value[18]='0'+(bits[5]     )%10;
    value[19]='.';
    value[20]='0'+(bits[3]/100 )%10;
    value[21]='0'+(bits[3]/10  )%10;
    value[22]='0'+(bits[3]     )%10;
    valuec=23;
  }
  if (egg_store_set(key,6,value,valuec)<0) {
    fprintf(stderr,"!!! egg_store_set failed: '%.*s' = '%.*s'\n",6,key,valuec,value);
  }
  storage_read_store(menu);
}

/* New.
 */
 
struct menu *menu_spawn_storage() {
  struct menu *menu=menu_spawn(sizeof(struct menu_storage));
  if (!menu) return 0;
  
  menu->del=_storage_del;
  menu->input=_storage_input;
  menu->update=_storage_update;
  menu->render=_storage_render;
  
  if (!(MENU->picklist=picklist_new())) { menu->defunct=1; return 0; }
  picklist_add_option(MENU->picklist,"Resources",   -1,0x80,menu,storage_cb_resources);
  picklist_add_option(MENU->picklist,"Set fieldA",  -1,0x42,menu,storage_cb_store);
  picklist_add_option(MENU->picklist,"Clear fieldA",-1,0x40,menu,storage_cb_store);
  picklist_add_option(MENU->picklist,"Set fieldB",  -1,0x43,menu,storage_cb_store);
  picklist_add_option(MENU->picklist,"Clear fieldB",-1,0x41,menu,storage_cb_store);
  
  storage_read_store(menu);
  
  return menu;
}
