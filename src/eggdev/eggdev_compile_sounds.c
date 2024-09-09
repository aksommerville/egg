#include "eggdev_internal.h"
#include "opt/synth/synth_formats.h"

/* Main entry points.
 */
 
int eggdev_compile_sounds(struct eggdev_res *res) {

  // Already Egg Sounds or WAV, keep it.
  if ((res->serialc>=4)&&!memcmp(res->serial,"\0ESS",4)) return 0;
  if ((res->serialc>=4)&&!memcmp(res->serial,"\xbe\xee\xeep",4)) return 0;
  if ((res->serialc>=4)&&!memcmp(res->serial,"RIFF",4)) return 0;
  
  // Anything else, assume it's Egg Sounds Text.
  struct sr_encoder dst={0};
  int err=synth_egg_from_text(&dst,res->serial,res->serialc,res->path);
  if (err<0) {
    sr_encoder_cleanup(&dst);
    return err;
  }
  eggdev_res_handoff_serial(res,dst.v,dst.c);
  return 0;
}

int eggdev_uncompile_sounds(struct eggdev_res *res) {

  // Egg Sounds, let synth uncompile to text.
  if ((res->serialc>=4)&&!memcmp(res->serial,"\0ESS",4)) {
    struct sr_encoder dst={0};
    int err=synth_text_from_egg(&dst,res->serial,res->serialc,res->path);
    if (err<0) {
      sr_encoder_cleanup(&dst);
      return err;
    }
    eggdev_res_handoff_serial(res,dst.v,dst.c);
    return 0;
  }
  
  // Egg Song, let's assume they want MIDI.
  if ((res->serialc>=4)&&!memcmp(res->serial,"\xbe\xee\xeep",4)) {
    struct sr_encoder dst={0};
    int err=synth_midi_from_egg(&dst,res->serial,res->serialc,res->path);
    if (err<0) {
      sr_encoder_cleanup(&dst);
      return err;
    }
    eggdev_res_handoff_serial(res,dst.v,dst.c);
    return 0;
  }
  
  // Anything else, keep as is.
  return 0;
}
