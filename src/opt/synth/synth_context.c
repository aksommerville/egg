#include "synth_internal.h"

/* PCM object lifecycle.
 */
 
void synth_pcm_del(struct synth_pcm *pcm) {
  if (!pcm) return;
  if (pcm->refc-->1) return;
  free(pcm);
}

int synth_pcm_ref(struct synth_pcm *pcm) {
  if (!pcm) return -1;
  if (pcm->refc<1) return -1;
  if (pcm->refc>=INT_MAX) return -1;
  pcm->refc++;
  return 0;
}

struct synth_pcm *synth_pcm_new(int samplec) {
  if (samplec<0) return 0; // Zero is legal.
  if (samplec>SYNTH_PCM_SIZE_LIMIT) return 0;
  struct synth_pcm *pcm=calloc(1,sizeof(struct synth_pcm)+sizeof(float)*samplec);
  if (!pcm) return 0;
  pcm->refc=1;
  pcm->c=samplec;
  return pcm;
}

/* Delete.
 */

void synth_del(struct synth *synth) {
  if (!synth) return;
  if (synth->qbuf) free(synth->qbuf);
  if (synth->soundv) {
    while (synth->soundc-->0) synth_pcm_del(synth->soundv[synth->soundc].pcm);
    free(synth->soundv);
  }
  if (synth->songv) free(synth->songv); // Nothing to clean up per song :)
  if (synth->busv) {
    while (synth->busc-->0) synth_node_del(synth->busv[synth->busc]);
    free(synth->busv);
  }
  free(synth);
}

/* New.
 */

struct synth *synth_new(int rate,int chanc) {
  if ((rate<SYNTH_RATE_MIN)||(rate>SYNTH_RATE_MAX)) return 0;
  if ((chanc<SYNTH_CHANC_MIN)||(chanc>SYNTH_CHANC_MAX)) return 0;
  struct synth *synth=calloc(1,sizeof(struct synth));
  if (!synth) return 0;
  synth->rate=rate;
  synth->chanc=chanc;
  
  synth->fade_time_normal=rate; // 1 s
  synth->fade_time_quick=rate>>2; // 250 ms
  synth->new_song_delay=rate>>1; // 500 ms
  
  synth_rates_generate_hz(synth->ratefv);
  synth_rates_normalizeip(synth->ratefv,synth->rate);
  //synth_rates_quantize(synth->rateiv,synth->ratefv);
  synth_wave_generate_sine(&synth->sine);
  
  return synth;
}

/* Unlist bus.
 */
 
void synth_unlist_bus(struct synth *synth,struct synth_node *bus) {
  if (synth->song==bus) synth->song=0;
}

void synth_kill_bus(struct synth *synth,struct synth_node *bus) {
  int i=synth->busc;
  while (i-->0) {
    if (synth->busv[i]==bus) {
      synth->busc--;
      memmove(synth->busv+i,synth->busv+i+1,sizeof(void*)*(synth->busc-i));
    }
  }
  synth_unlist_bus(synth,bus);
  synth_node_del(bus);
}

/* Add bus.
 */
 
struct synth_node *synth_add_bus(struct synth *synth) {
  if (!synth) return 0;
  if (synth->busc>=synth->busa) {
    int na=synth->busa+16;
    if (na>INT_MAX/sizeof(void*)) return 0;
    void *nv=realloc(synth->busv,sizeof(void*)*na);
    if (!nv) return 0;
    synth->busv=nv;
    synth->busa=na;
  }
  struct synth_node *bus=synth_node_new(synth,&synth_node_type_bus,synth->chanc);
  if (!bus) return 0;
  synth->busv[synth->busc++]=bus;
  return bus;
}

/* Begin printing sound if needed.
 */
 
int synth_sound_require(struct synth *synth,struct synth_sound *sound) {
  if (!synth||!sound) return -1;
  if (sound->pcm) return 0;
  fprintf(stderr,"%s:%d:%s: TODO id=%d:%d c=%d\n",__FILE__,__LINE__,__func__,sound->rid,sound->index,sound->c);
  return -1;
}

/* Search sounds.
 */
 
int synth_soundv_search(const struct synth *synth,int rid,int index) {
  int lo=0,hi=synth->soundc;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
    const struct synth_sound *q=synth->soundv+ck;
         if (rid<q->rid) hi=ck;
    else if (rid>q->rid) lo=ck+1;
    else if (index<q->index) hi=ck;
    else if (index>q->index) lo=ck+1;
    else return ck;
  }
  return -lo-1;
}

/* Insert sound.
 */
 
static struct synth_sound *synth_soundv_insert(struct synth *synth,int p,int rid,int index) {
  if ((p<0)||(p>synth->soundc)) return 0;
  if (synth->soundc>=synth->sounda) {
    int na=synth->sounda+8;
    if (na>INT_MAX/sizeof(struct synth_sound)) return 0;
    void *nv=realloc(synth->soundv,sizeof(struct synth_sound)*na);
    if (!nv) return 0;
    synth->soundv=nv;
    synth->sounda=na;
  }
  struct synth_sound *sound=synth->soundv+p;
  memmove(sound+1,sound,sizeof(struct synth_sound)*(synth->soundc-p));
  synth->soundc++;
  memset(sound,0,sizeof(struct synth_sound));
  sound->rid=rid;
  sound->index=index;
  return sound;
}

/* Install one sound.
 */
 
static int synth_install_sound(struct synth *synth,int rid,int index,const void *src,int srcc) {
  
  // It is extremely likely that every time we ever get called, insertion point is at the end.
  // So it's worth checking for that case, to avoid the search.
  // But perfectly legal to insert anywhere, as long as the ID isn't in use yet.
  int p;
  if (
    !synth->soundc||
    (rid>synth->soundv[synth->soundc-1].rid)||
    ((rid==synth->soundv[synth->soundc-1].rid)&&(index>synth->soundv[synth->soundc-1].index))
  ) {
    p=synth->soundc;
  } else {
    if ((p=synth_soundv_search(synth,rid,index))>=0) return -1;
    p=-p-1;
  }
  
  struct synth_sound *sound=synth_soundv_insert(synth,p,rid,index);
  if (!sound) return -1;
  sound->v=src;
  sound->c=srcc;
  
  return 0;
}

/* Install sounds.
 */

int synth_install_sounds(struct synth *synth,int rid,const void *src,int srcc) {
  if (!src||(srcc<1)) return 0;
  struct synth_sounds_reader reader;
  if (synth_sounds_reader_init(&reader,src,srcc)<0) return -1;
  int index,subsrcc;
  const void *subsrc;
  while ((subsrcc=synth_sounds_reader_next(&index,&subsrc,&reader))>0) {
    if (synth_sound_is_empty(subsrc,subsrcc)) continue;
    int err=synth_install_sound(synth,rid,index,subsrc,subsrcc);
    if (err<0) return err;
  }
  return 0;
}

/* Search songs.
 */
 
int synth_songv_search(const struct synth *synth,int rid) {
  if (!synth->songc||(rid>synth->songv[synth->songc-1].rid)) return -synth->songc-1;
  int lo=0,hi=synth->songc;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
    int q=synth->songv[ck].rid;
         if (rid<q) hi=ck;
    else if (rid>q) lo=ck+1;
    else return ck;
  }
  return -lo-1;
}

/* Insert song.
 */
 
static struct synth_song *synth_songv_insert(struct synth *synth,int p,int rid) {
  if ((p<0)||(p>synth->songc)) return 0;
  if (p&&(rid<=synth->songv[p-1].rid)) return 0;
  if ((p<synth->songc)&&(rid>=synth->songv[p].rid)) return 0;
  if (synth->songc>=synth->songa) {
    int na=synth->songa+16;
    if (na>INT_MAX/sizeof(struct synth_song)) return 0;
    void *nv=realloc(synth->songv,sizeof(struct synth_song)*na);
    if (!nv) return 0;
    synth->songv=nv;
    synth->songa=na;
  }
  struct synth_song *song=synth->songv+p;
  memmove(song+1,song,sizeof(struct synth_song)*(synth->songc-p));
  synth->songc++;
  memset(song,0,sizeof(struct synth_song));
  song->rid=rid;
  return song;
}

/* Install song.
 */
 
int synth_install_song(struct synth *synth,int rid,const void *src,int srcc) {
  if (synth_sound_is_empty(src,srcc)) return 0;
  if (!rid) return -1; // rid zero is forbidden, because that's our "no song" marker.
  int p=synth_songv_search(synth,rid);
  if (p>=0) return -1;
  p=-p-1;
  struct synth_song *song=synth_songv_insert(synth,p,rid);
  if (!song) return -1;
  song->v=src;
  song->c=srcc;
  return 0;
}

/* Get current song id.
 */

int synth_get_song(const struct synth *synth) {
  return 0;//TODO
}

/* Get playhead.
 */

double synth_get_playhead(const struct synth *synth) {
  return 0.0;//TODO
}

/* Set playhead.
 */
 
void synth_set_playhead(struct synth *synth,double s) {
  fprintf(stderr,"TODO %s %f\n",__func__,s);
}
