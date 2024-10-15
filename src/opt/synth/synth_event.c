#include "synth_internal.h"

/* Play PCM.
 */

static void synth_play_pcm(struct synth *synth,struct synth_pcm *pcm) {
  if (!pcm||(pcm->c<1)) return;
  struct synth_node *player=synth_add_bus(synth,&synth_node_type_pcm);
  if (
    !player||
    (synth_node_pcm_setup(player,pcm,0.5f,0.0f)<0)||
    (synth_node_ready(player)<0)
  ) {
    synth_kill_bus(synth,player);
    return;
  }
}

/* Begin printing.
 * Returns WEAK.
 */
 
static struct synth_pcm *synth_begin_print(float *pan,struct synth *synth,const void *src,int srcc) {
  if (synth->printerc>=synth->printera) {
    int na=synth->printera+16;
    if (na>INT_MAX/sizeof(void*)) return 0;
    void *nv=realloc(synth->printerv,sizeof(void*)*na);
    if (!nv) return 0;
    synth->printerv=nv;
    synth->printera=na;
  }
  struct synth_printer *printer=synth_printer_new(synth,src,srcc);
  if (!printer) return 0;
  synth_printer_update(printer,synth->print_framec);
  synth->printerv[synth->printerc++]=printer;
  if (pan) *pan=synth_node_bus_get_default_pan(printer->bus);
  return printer->pcm;
}

/* Begin printing sound if needed.
 */
 
int synth_sound_require(struct synth *synth,struct synth_sound *sound) {
  if (!synth||!sound) return -1;
  if (sound->pcm) return 0;
  
  if ((sound->c>=4)&&!memcmp(sound->v,"\0EGS",4)) {
    struct synth_pcm *pcm=synth_begin_print(&sound->pan,synth,sound->v,sound->c);
    if (pcm) {
      if (synth_pcm_ref(pcm)<0) return -1;
      sound->pcm=pcm;
      return 0;
    }
    
  } else {
    /*TODO
    if (sound->pcm=synth_pcm_decode(synth->rate,sound->v,sound->c)) {
      return 0;
    }
    */
  }
  
  fprintf(stderr,"WARNING: Failed to decode %d-byte sound %d:%d. Stubbing.\n",sound->c,sound->rid,sound->index);
  if (sound->pcm=synth_pcm_new(1)) return 0;

  return -1;
}

/* Play sound.
 */

void synth_play_sound(struct synth *synth,int rid) {
  int p=synth_soundv_search(synth,rid);
  if (p<0) return;
  struct synth_sound *sound=synth->soundv+p;
  if (synth_sound_require(synth,sound)<0) return;
  synth_play_pcm(synth,sound->pcm);
}

/* Play song.
 */

void synth_play_song(struct synth *synth,int rid,int force,int repeat) {

  /* If we're not forcing, and this song is already playing, keep it.
   * If it's playing but fading out, cancel any waiting song and cancel the fade-out.
   */
  if (!force&&rid) {
    if (synth->song) {
      int pvrid=synth_node_bus_get_songid(synth->song);
      if (pvrid==rid) {
        return;
      }
    }
    int i=synth->busc;
    while (i-->0) {
      struct synth_node *bus=synth->busv[i];
      int brid=synth_node_bus_get_songid(bus);
      if (brid==rid) {
        synth_node_bus_cancel_fade(bus);
        synth_node_bus_fade_out(synth->song,synth->fade_time_quick,0); // Might not exist, but that's safe.
        synth->song=bus;
        return;
      }
    }
  }
  
  /* If a song is currently playing, schedule its completion and note it.
   */
  int outgoing=0;
  if (synth->song) {
    synth_node_bus_fade_out(synth->song,synth->fade_time_normal,0);
    synth->song=0;
    outgoing=1;
  }
  
  /* Find the song resource.
   * If it doesn't exist, we're done.
   */
  int p=synth_songv_search(synth,rid);
  if (p<0) {
    return;
  }
  struct synth_song *song=synth->songv+p;
  
  /* Stand a new bus for this song, install it, configure it.
   */
  struct synth_node *bus=synth_add_bus(synth,0);
  if (!bus) {
    return;
  }
  if (synth_node_bus_configure(bus,song->v,song->c)<0) {
    synth_kill_bus(synth,bus);
    return;
  }
  synth_node_bus_set_songid(bus,rid,repeat);
  if (synth_node_ready(bus)<0) {
    synth_kill_bus(synth,bus);
    return;
  }
  if (outgoing) {
    synth_node_bus_wait(bus,synth->new_song_delay);
  }
  synth->song=bus;
}

/* Send event.
 */

void synth_event(struct synth *synth,uint8_t chid,uint8_t opcode,uint8_t a,uint8_t b,int durms) {
  fprintf(stderr,"TODO %s %02x %02x %02x %02x dur=%d\n",__func__,chid,opcode,a,b,durms);
}
