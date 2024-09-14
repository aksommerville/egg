#include "synth_internal.h"

/* Play PCM.
 */
 
static void synth_play_pcm(struct synth *synth,struct synth_pcm *pcm) {
  fprintf(stderr,"%s %p,%p samplec=%d\n",__func__,synth,pcm,pcm?pcm->c:0);
  if (!pcm||(pcm->c<1)) return;
  //TODO PCM player.
  //TODO There might be an associated channel post. PCMs don't necessarily play direct onto the main.
}

/* Play sound.
 */

void synth_play_sound(struct synth *synth,int rid,int index) {
  fprintf(stderr,"TODO %s rid=%d index=%d soundc=%d\n",__func__,rid,index,synth->soundc);
  int p=synth_soundv_search(synth,rid,index);
  if (p<0) return;
  struct synth_sound *sound=synth->soundv+p;
  if (!sound->pcm) {
    if ((sound->c>=4)&&!memcmp(sound->v,"\0EGS",4)) {
      //TODO Create printer.
      //TODO Install printer.
      //TODO Initial printer run.
    } else {
      //TODO Decode PCM from raw format.
    }
    //TODO Retain pcm.
  }
  synth_play_pcm(synth,sound->pcm);
}

/* Play song.
 */

void synth_play_song(struct synth *synth,int rid,int force,int repeat) {
  fprintf(stderr,"TODO %s %d\n",__func__,rid);
  
  /* If we're not forcing, and this song is already playing, keep it.
   * If it's playing but fading out, cancel any waiting song and cancel the fade-out.
   */
  if (!force) {
    if (synth->song) {
      int pvrid=synth_node_bus_get_songid(synth->song);
      if (pvrid==rid) {
        fprintf(stderr,"...already playing.\n");
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
        fprintf(stderr,"...cancel fade out.\n");
        return;
      }
    }
  }
  
  /* If a song is currently playing, schedule its completion and note it.
   */
  int outgoing=0;
  if (synth->song) {
    fprintf(stderr,"...end current song...\n");
    synth_node_bus_fade_out(synth->song,synth->fade_time_normal,0);
    synth->song=0;
    outgoing=1;
  }
  
  /* Find the song resource.
   * If it doesn't exist, we're done.
   */
  int p=synth_songv_search(synth,rid);
  if (p<0) {
    fprintf(stderr,"...song %d not found. (songc=%d)\n",rid,synth->songc);
    return;
  }
  struct synth_song *song=synth->songv+p;
  
  /* Stand a new bus for this song, install it, configure it.
   */
  struct synth_node *bus=synth_add_bus(synth);
  if (!bus) {
    fprintf(stderr,"...FAILED TO ADD BUS\n");
    return;
  }
  if (synth_node_bus_configure(bus,song->v,song->c)<0) {
    fprintf(stderr,"...FAILED TO CONFIGURE\n");
    synth_kill_bus(synth,bus);
    return;
  }
  synth_node_bus_set_songid(bus,rid,repeat);
  if (synth_node_ready(bus)<0) {
    fprintf(stderr,"...FAILED TO READY\n");
    synth_kill_bus(synth,bus);
    return;
  }
  if (outgoing) {
    synth_node_bus_wait(bus,synth->new_song_delay);
  }
  fprintf(stderr,"...playing.\n");
}

/* Send event.
 */

void synth_event(struct synth *synth,uint8_t chid,uint8_t opcode,uint8_t a,uint8_t b,int durms) {
  fprintf(stderr,"TODO %s %02x %02x %02x %02x dur=%d\n",__func__,chid,opcode,a,b,durms);
}
