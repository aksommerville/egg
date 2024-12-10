#include "egg/egg.h"
#include "opt/stdlib/egg-stdlib.h"
#include "opt/graf/graf.h"
#include "opt/text/text.h"
#include "egg_rom_toc.h"
#include "shared_symbols.h"

static void *rom=0;
static int romc=0;
static struct graf graf={0};
static struct font *font=0;
static int texid_message=0;
static int messagew=0,messageh=0;
static int fbw=0,fbh=0;

void egg_client_quit(int status) {
  // Flush save state, log performance... Usually noop.
  // Full cleanup really isn't necessary, but if you like:
  if (rom) free(rom);
  font_del(font);
  egg_texture_del(texid_message);
}

int egg_client_init() {
  fprintf(stderr,"%s:%d:%s: My game is starting up...\n",__FILE__,__LINE__,__func__);
  
  egg_texture_get_status(&fbw,&fbh,1);
  
  if ((romc=egg_get_rom(0,0))<=0) return -1;
  if (!(rom=malloc(romc))) return -1;
  if (egg_get_rom(rom,romc)!=romc) return -1;
  strings_set_rom(rom,romc);
  
  if (!(font=font_new())) return -1;
  if (font_add_image_resource(font,0x0020,RID_image_font9_0020)<0) return -1;
  if ((texid_message=font_tex_oneline(font,"Game is running!",-1,200,0xffffffff))<0) return -1;
  egg_texture_get_status(&messagew,&messageh,texid_message);
  
  srand_auto();
  
  return 0;
}

void egg_client_update(double elapsed) {
  // TODO
}

void egg_client_render() {
  graf_reset(&graf);
  graf_draw_decal(&graf,texid_message,(fbw>>1)-(messagew>>1),(fbh>>1)-(messageh>>1),0,0,messagew,messageh,0);
  graf_flush(&graf);
}
