#include "synth_internal.h"

/* Play sound.
 */

void synth_play_sound(struct synth *synth,int rid,int index) {
  fprintf(stderr,"TODO %s rid=%d index=%d\n",__func__,rid,index);
  int p=synth_soundsv_search(synth,rid);
  if (p<0) return;
  struct sfg_pcm *pcm=0;
  struct synth_sounds *sounds=synth->soundsv+p;
  p=synth_sounds_pcmv_search(sounds,index);
  if (p<0) {
    p=-p-1;
    const void *serial=0;
    int serialc=sfg_get_sound_serial(&serial,sounds->v,sounds->c,index);
    struct sfg_printer *printer=0;
    if (serialc>0) {
      if (!(printer=sfg_printer_new(serial,serialc,synth->rate))) return;
      if (synth_printerv_add(synth,printer)<0) {
        sfg_printer_del(printer);
        return;
      }
      pcm=sfg_printer_get_pcm(printer);
      if (sfg_pcm_ref(pcm)<0) return;
    } else {
      if (!(pcm=sfg_pcm_new(0))) return;
    }
    if (synth_sounds_pcmv_insert(sounds,p,pcm)<0) {
      sfg_pcm_del(pcm);
      return;
    }
  } else {
    pcm=sounds->pcmv[p];
  }
  fprintf(stderr,"TODO Create player for %d-sample PCM dump.\n",pcm->c);
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
