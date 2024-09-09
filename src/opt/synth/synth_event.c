#include "synth_internal.h"

/* Play sound.
 */

void synth_play_sound(struct synth *synth,int rid,int index) {
  fprintf(stderr,"TODO %s rid=%d index=%d\n",__func__,rid,index);
}

/* Play song.
 */

void synth_play_song(struct synth *synth,int rid,int force,int repeat) {
  fprintf(stderr,"TODO %s %d\n",__func__,rid);
}

/* Send event.
 */

void synth_event(struct synth *synth,uint8_t chid,uint8_t opcode,uint8_t a,uint8_t b,int durms) {
  fprintf(stderr,"TODO %s %02x %02x %02x %02x dur=%d\n",__func__,chid,opcode,a,b,durms);
}
