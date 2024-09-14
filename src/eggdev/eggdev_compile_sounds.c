#include "eggdev_internal.h"
#include "opt/synth/synth_formats.h"

/* Binary from text or whatever.
 */
 
int eggdev_compile_sounds(struct eggdev_res *res,struct eggdev_rom *rom) {

  // Already Egg Sounds or WAV, keep it.
  if ((res->serialc>=4)&&!memcmp(res->serial,"\0MSF",4)) return 0;
  if ((res->serialc>=4)&&!memcmp(res->serial,"\0EGS",4)) return 0;
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

/* Text from binary or whatever.
 */

int eggdev_uncompile_sounds(struct eggdev_res *res,struct eggdev_rom *rom) {

  // Egg Sounds, let synth uncompile to text.
  // Check if there are any non-EGGSND members (eg WAV), and if so, just keep it as is.
  // We're not in a position to produce the multiple output files that one would expect from WAV members.
  if ((res->serialc>=4)&&!memcmp(res->serial,"\0MSF",4)) {
  
    struct synth_sounds_reader reader={0};
    if (synth_sounds_reader_init(&reader,res->serial,res->serialc)<0) return -1;
    const void *tmp=0;
    int tmpc,index=0;
    while ((tmpc=synth_sounds_reader_next(&index,&tmp,&reader))>0) {
      if ((tmpc<4)||memcmp(tmp,"\0EGS",4)) return 0; // Don't try converting.
    }
    
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
  if ((res->serialc>=4)&&!memcmp(res->serial,"\0EGS",4)) {
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
