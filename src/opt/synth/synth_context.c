#include "synth_internal.h"

/* Delete.
 */
 
static void synth_sounds_cleanup(struct synth_sounds *sounds) {
  if (sounds->pcmv) {
    while (sounds->pcmc-->0) sfg_pcm_del(sounds->pcmv[sounds->pcmc]);
    free(sounds->pcmv);
  }
}

void synth_del(struct synth *synth) {
  if (!synth) return;
  if (synth->qbuf) free(synth->qbuf);
  if (synth->soundsv) {
    while (synth->soundsc-->0) synth_sounds_cleanup(synth->soundsv+synth->soundsc);
    free(synth->soundsv);
  }
  if (synth->songv) free(synth->songv); // Nothing to clean up per song :)
  if (synth->printerv) {
    while (synth->printerc-->0) sfg_printer_del(synth->printerv[synth->printerc]);
    free(synth->printerv);
  }
  free(synth);
}

/* New.
 */

struct synth *synth_new(int rate,int chanc) {
  fprintf(stderr,"%s...\n",__func__);
  if ((rate<SYNTH_RATE_MIN)||(rate>SYNTH_RATE_MAX)) return 0;
  if ((chanc<SYNTH_CHANC_MIN)||(chanc>SYNTH_CHANC_MAX)) return 0;
  struct synth *synth=calloc(1,sizeof(struct synth));
  if (!synth) return 0;
  synth->rate=rate;
  synth->chanc=chanc;
  fprintf(stderr,"...%s\n",__func__);
  return synth;
}

/* Add printer.
 */
 
int synth_printerv_add(struct synth *synth,struct sfg_printer *printer) {
  if (!synth||!printer) return -1;
  if (synth->printerc>=synth->printera) {
    int na=synth->printera+16;
    if (na>INT_MAX/sizeof(void*)) return -1;
    void *nv=realloc(synth->printerv,sizeof(void*)*na);
    if (!nv) return -1;
    synth->printerv=nv;
    synth->printera=na;
  }
  synth->printerv[synth->printerc++]=printer;
  sfg_printer_update(printer,synth->preprintc);
  return 0;
}

/* Search sounds.
 */
 
int synth_soundsv_search(const struct synth *synth,int rid) {
  if (!synth->soundsc||(rid>synth->soundsv[synth->soundsc-1].rid)) return -synth->soundsc-1;
  int lo=0,hi=synth->soundsc;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
    int q=synth->soundsv[ck].rid;
         if (rid<q) hi=ck;
    else if (rid>q) lo=ck+1;
    else return ck;
  }
  return -lo-1;
}

/* Insert sounds.
 */
 
static struct synth_sounds *synth_soundsv_insert(struct synth *synth,int p,int rid) {
  if ((p<0)||(p>synth->soundsc)) return 0;
  if (p&&(rid<=synth->soundsv[p-1].rid)) return 0;
  if ((p<synth->soundsc)&&(rid>=synth->soundsv[p].rid)) return 0;
  if (synth->soundsc>=synth->soundsa) {
    int na=synth->soundsa+8;
    if (na>INT_MAX/sizeof(struct synth_sounds)) return 0;
    void *nv=realloc(synth->soundsv,sizeof(struct synth_sounds)*na);
    if (!nv) return 0;
    synth->soundsv=nv;
    synth->soundsa=na;
  }
  struct synth_sounds *sounds=synth->soundsv+p;
  memmove(sounds+1,sounds,sizeof(struct synth_sounds)*(synth->soundsc-p));
  synth->soundsc++;
  memset(sounds,0,sizeof(struct synth_sounds));
  sounds->rid=rid;
  return sounds;
}

/* Search pcms in sounds.
 */
 
int synth_sounds_pcmv_search(const struct synth_sounds *sounds,int id) {
  int lo=0,hi=sounds->pcmc;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
    int q=sounds->pcmv[ck]->id;
         if (id<q) hi=ck;
    else if (id>q) lo=ck+1;
    else return ck;
  }
  return -lo-1;
}

/* Insert pcm in sounds, handoff.
 */
 
int synth_sounds_pcmv_insert(struct synth_sounds *sounds,int p,struct sfg_pcm *pcm) {
  if ((p<0)||(p>sounds->pcmc)) return -1;
  if (p&&(pcm->id<=sounds->pcmv[p-1]->id)) return -1;
  if ((p<sounds->pcmc)&&(pcm->id>=sounds->pcmv[p]->id)) return -1;
  if (sounds->pcmc>=sounds->pcma) {
    int na=sounds->pcma+16;
    if (na>INT_MAX/sizeof(void*)) return -1;
    void *nv=realloc(sounds->pcmv,sizeof(void*)*na);
    if (!nv) return -1;
    sounds->pcmv=nv;
    sounds->pcma=na;
  }
  memmove(sounds->pcmv+p+1,sounds->pcmv+p,sizeof(void*)*(sounds->pcmc-p));
  sounds->pcmv[p]=pcm;
  sounds->pcmc++;
  return 0;
}

/* Install sounds.
 */

int synth_install_sounds(struct synth *synth,int rid,const void *src,int srcc) {
  fprintf(stderr,"%s rid=%d srcc=%d\n",__func__,rid,srcc);
  if (!src||(srcc<1)) return 0;
  int p=synth_soundsv_search(synth,rid);
  if (p>=0) {
    struct synth_sounds *sounds=synth->soundsv+p;
    sounds->v=src;
    sounds->c=srcc;
    while (sounds->pcmc>0) {
      sounds->pcmc--;
      free(sounds->pcmv[sounds->pcmc]);
    }
  } else {
    p=-p-1;
    struct synth_sounds *sounds=synth_soundsv_insert(synth,p,rid);
    if (!sounds) return -1;
    sounds->v=src;
    sounds->c=srcc;
  }
  return 0;
}

/* Search songs.
 */
 
int synth_songv_search(const struct synth *synth,int rid) {
  if (!synth->songc||(rid>synth->songv[synth->songc].rid)) return -synth->songc-1;
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
  fprintf(stderr,"%s rid=%d srcc=%d\n",__func__,rid,srcc);
  if (!src||(srcc<1)) return 0;
  if (!rid) return -1; // rid zero is forbidden, because that's our "no song" marker.
  int p=synth_songv_search(synth,rid);
  if (p>=0) {
    struct synth_song *song=synth->songv+p;
    song->v=src;
    song->c=srcc;
  } else {
    p=-p-1;
    struct synth_song *song=synth_songv_insert(synth,p,rid);
    if (!song) return -1;
    song->v=src;
    song->c=srcc;
  }
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
