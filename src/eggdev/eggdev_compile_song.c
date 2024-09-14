#include "eggdev_internal.h"
#include "opt/synth/synth_formats.h"

/* Fetch shared program.
 */
 
static int eggdev_compile_song_cb_program(void *dstpp,int fqpid,void *userdata) {
  struct eggdev_rom *rom=userdata;
  return eggdev_rom_get_instrument(dstpp,rom,fqpid);
}

/* Main entry points.
 */
 
int eggdev_compile_song(struct eggdev_res *res,struct eggdev_rom *rom) {

  if ((res->serialc>=4)&&!memcmp(res->serial,"\0EGS",4)) return 0;

  if ((res->serialc>=4)&&!memcmp(res->serial,"MThd",4)) {
    struct sr_encoder dst={0};
    int err=synth_egg_from_midi(&dst,res->serial,res->serialc,res->path,eggdev_compile_song_cb_program,rom);
    if (err<0) {
      sr_encoder_cleanup(&dst);
      return err;
    }
    eggdev_res_handoff_serial(res,dst.v,dst.c);
    return 0;
  }
  
  // Assume that anything else is EGS Text. We'll verify after converting.
  struct sr_encoder dst={0};
  int err=synth_egg_from_text(&dst,res->serial,res->serialc,res->path);
  if (err<0) {
    sr_encoder_cleanup(&dst);
    return err;
  }
  if ((dst.c<4)||memcmp(dst.v,"\0EGS",4)) {
    sr_encoder_cleanup(&dst);
    fprintf(stderr,"%s: Unknown format for song. Expected MIDI or Egg Song\n",res->path);
    return -2;
  }
  eggdev_res_handoff_serial(res,dst.v,dst.c);
  return 0;
}

int eggdev_uncompile_song(struct eggdev_res *res,struct eggdev_rom *rom) {

  if ((res->serialc>=4)&&!memcmp(res->serial,"\0EGS",4)) {
    struct sr_encoder dst={0};
    int err=synth_midi_from_egg(&dst,res->serial,res->serialc,res->path);
    if (err<0) {
      if (err!=-2) fprintf(stderr,"%s: Unspecified error converting song\n",res->path);
      sr_encoder_cleanup(&dst);
      return -2;
    }
    eggdev_res_handoff_serial(res,dst.v,dst.c);
    eggdev_res_set_format(res,"mid",3);
    return 0;
  }

  if ((res->serialc>=4)&&!memcmp(res->serial,"MThd",4)) return 0;

  // Not EGS or MIDI? Whatever, let it thru verbatim.
  return 0;
}
