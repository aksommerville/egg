#include "synth_internal.h"

/* Delete.
 */
 
static void synth_res_cleanup(struct synth_res *res) {
  if (res->pcm) synth_pcm_del(res->pcm);
}

void synth_del(struct synth *synth) {
  if (!synth) return;
  if (synth->qbuf) free(synth->qbuf);
  if (synth->busv) {
    while (synth->busc-->0) synth_node_del(synth->busv[synth->busc]);
    free(synth->busv);
  }
  if (synth->printerv) {
    while (synth->printerc-->0) synth_printer_del(synth->printerv[synth->printerc]);
    free(synth->printerv);
  }
  if (synth->resv) {
    while (synth->resc-->0) synth_res_cleanup(synth->resv+synth->resc);
    free(synth->resv);
  }
  if (synth->song_storage) free(synth->song_storage);
  if (synth->sine) free(synth->sine);
  free(synth);
}

/* Generate rates table.
 */
 
static void synth_generate_rates(struct synth *synth) {
  // Abstractly: hz=440.0f*powf(2.0f,(noteid-69.0f)/12.0f)
  // But we'll cheat it a little. Generate the octave starting at A4 (0x45, 440 Hz),
  // then half and double it the rest of the way.
  // We could multiply our way up that first octave too, but I think the risk of rounding error is too high.
  
  float *ref=synth->freq_by_noteid+0x45;
  *ref=440.0f;
  int i=1;
  for (;i<12;i++) ref[i]=ref[0]*powf(2.0f,(float)i/12.0f);
  
  float *p=ref+12;
  for (i=0x45+12;i<128;i++,p++) *p=p[-12]*2.0f;
  for (i=0x45,p=ref-1;i-->0;p--) *p=p[12]*0.5f;
  
  float adjust=1.0f/(float)synth->rate;
  for (i=128,p=synth->freq_by_noteid;i-->0;p++) {
    (*p)*=adjust;
    if (*p>0.5f) *p=0.5f;
  }
}

/* New.
 */
 
struct synth *synth_new(int rate,int chanc) {
  if ((rate<200)||(rate>200000)) return 0;
  if ((chanc<1)||(chanc>8)) return 0;
  struct synth *synth=calloc(1,sizeof(struct synth));
  if (!synth) return 0;
  
  synth->rate=rate;
  synth->chanc=chanc;
  
  synth_generate_rates(synth);
  
  return synth;
}

/* Resource list.
 */
 
static int synth_resv_search(const struct synth *synth,int tid,int rid) {
  // Extremely likely that the initial load will happen in order, and every search would be worst-case without this cudgel:
  if (synth->resc&&(tid==synth->resv[synth->resc-1].tid)&&(rid>synth->resv[synth->resc-1].rid)) return -synth->resc-1;
  int lo=0,hi=synth->resc;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
    const struct synth_res *res=synth->resv+ck;
         if (tid<res->tid) hi=ck;
    else if (tid>res->tid) lo=ck+1;
    else if (rid<res->rid) hi=ck;
    else if (rid>res->rid) lo=ck+1;
    else return ck;
  }
  return -lo-1;
}

static struct synth_res *synth_resv_insert(struct synth *synth,int p,int tid,int rid) {
  if ((p<0)||(p>synth->resc)) return 0;
  if (synth->resc>=synth->resa) {
    int na=synth->resa+32;
    if (na>INT_MAX/sizeof(struct synth_res)) return 0;
    void *nv=realloc(synth->resv,sizeof(struct synth_res)*na);
    if (!nv) return 0;
    synth->resv=nv;
    synth->resa=na;
  }
  struct synth_res *res=synth->resv+p;
  memmove(res+1,res,sizeof(struct synth_res)*(synth->resc-p));
  synth->resc++;
  memset(res,0,sizeof(struct synth_res));
  res->tid=tid;
  res->rid=rid;
  return res;
}

struct synth_res *synth_res_get(const struct synth *synth,int tid,int rid) {
  int p=synth_resv_search(synth,tid,rid);
  if (p<0) return 0;
  return synth->resv+p;
}

static int synth_resv_install(struct synth *synth,int tid,int rid,const void *src,int srcc) {
  if (!src||(srcc<1)) return 0;
  struct synth_res *res=0;
  int p=synth_resv_search(synth,tid,rid);
  if (p<0) {
    p=-p-1;
    res=synth_resv_insert(synth,p,tid,rid);
    if (!res) return -1;
  } else {
    res=synth->resv+p;
  }
  res->src=src;
  res->srcc=srcc;
  if (res->pcm) {
    synth_pcm_del(res->pcm);
    res->pcm=0;
  }
  return 0;
}

/* Install resources, public.
 */

int synth_install_sound(struct synth *synth,int rid,const void *src,int srcc) {
  return synth_resv_install(synth,EGG_TID_sound,rid,src,srcc);
}

int synth_install_song(struct synth *synth,int rid,const void *src,int srcc) {
  return synth_resv_install(synth,EGG_TID_song,rid,src,srcc);
}

/* Update, public entry points.
 */

void synth_updatef(float *v,int c,struct synth *synth) {
  synth_update_before(synth,c/synth->chanc);
  synth_update_inner(v,c,synth);
  synth_update_after(synth);
}

void synth_updatei(int16_t *v,int c,struct synth *synth) {
  synth_update_before(synth,c/synth->chanc);
  if (synth->qbufa<1) {
    synth->qbufa=1024*synth->chanc;
    if (!(synth->qbuf=malloc(synth->qbufa*sizeof(float)))) {
      synth->qbufa=0;
      memset(v,0,c<<1);
      return;
    }
  }
  while (c>0) {
    int updc=c;
    if (updc>synth->qbufa) updc=synth->qbufa;
    synth_update_inner(synth->qbuf,updc,synth);
    c-=updc;
    const float *src=synth->qbuf;
    for (;updc-->0;v++,src++) {
      int q=(int)((*src)*32767.0);
      if (q<-32678) *v=-32768;
      else if (q>32767) *v=32767;
      else *v=q;
    }
  }
  synth_update_after(synth);
}

/* Push event onto bus.
 */
 
void synth_event(struct synth *synth,uint8_t chid,uint8_t opcode,uint8_t a,uint8_t b,int durms) {
  int i=synth->busc;
  while (i-->0) {
    struct synth_node *bus=synth->busv[i];
    if (synth_node_bus_is_terminating(bus)) continue;
    synth_node_bus_midi_event(bus,chid,opcode,a,b,durms);
    return;
  }
  //TODO What's supposed to happen when there's no song to send it to?
  // I think it would be more appropriate to have a separate "user bus", always available. That's a spec change.
}

/* Spawn printer.
 */
 
static struct synth_printer *synth_printer_spawn(struct synth *synth,const void *src,int srcc) {
  if (synth->printerc>=synth->printera) {
    int na=synth->printera+8;
    if (na>INT_MAX/sizeof(void*)) return 0;
    void *nv=realloc(synth->printerv,sizeof(void*)*na);
    if (!nv) return 0;
    synth->printerv=nv;
    synth->printera=na;
  }
  struct synth_printer *printer=synth_printer_new(synth,src,srcc);
  if (!printer) return 0;
  synth->printerv[synth->printerc++]=printer;
  synth_printer_update(printer,synth->new_printer_framec);
  return printer;
}

/* Acquire PCM from serial.
 * This might happen synchronously, for PCM resources, or it might install a printer.
 */
 
static struct synth_pcm *synth_acquire_pcm(struct synth *synth,const void *src,int srcc,int rid) {

  // From raw PCM resource.
  if ((srcc>=8)&&!memcmp(src,"\0PCM",4)) {
    return synth_pcm_decode(synth->rate,src,srcc);
  }
  
  // From EGS resource.
  if ((srcc>=4)&&!memcmp(src,"\0EGS",4)) {
    struct synth_printer *printer=synth_printer_spawn(synth,src,srcc);
    if (!printer) return 0;
    if (synth_pcm_ref(printer->pcm)<0) return 0;
    return printer->pcm;
  }
  
  return 0;
}

/* Acquire PCM, for other synth parties (synth_node_channel).
 */
 
struct synth_pcm *synth_get_pcm(struct synth *synth,int rid) {
  struct synth_res *res=synth_res_get(synth,EGG_TID_sound,rid);
  if (!res) return 0;
  if (!res->pcm) {
    if (!(res->pcm=synth_acquire_pcm(synth,res->src,res->srcc,rid))) {
      res->pcm=synth_pcm_new(1);
    }
  }
  return res->pcm;
}

/* Play sound.
 */

void synth_play_sound(struct synth *synth,int rid) {
  struct synth_res *res=synth_res_get(synth,EGG_TID_sound,rid);
  if (!res) return;
  if (!res->pcm) {
    if (!(res->pcm=synth_acquire_pcm(synth,res->src,res->srcc,rid))) {
      // Create a dummy PCM so we don't try to re-acquire every time.
      if (!(res->pcm=synth_pcm_new(1))) return;
    }
  }
  if (res->pcm->c<=1) return;
  
  float trim=0.5f;//TODO
  float pan=0.0f;
  
  if (synth->busc>=synth->busa) {
    int na=synth->busa+4;
    if (na>INT_MAX/sizeof(void*)) return;
    void *nv=realloc(synth->busv,sizeof(void*)*na);
    if (!nv) return;
    synth->busv=nv;
    synth->busa=na;
  }
  struct synth_node *bus=synth_node_new(&synth_node_type_pcm,0,synth);
  if (!bus) return;
  if (
    (synth_node_pcm_configure(bus,res->pcm,trim,pan,rid)<0)||
    (synth_node_ready(bus)<0)
  ) {
    synth_node_del(bus);
    return;
  }
  synth->busv[synth->busc++]=bus;
}

/* End current song gracefully.
 */

void synth_song_end(struct synth *synth) {
  int i=synth->busc;
  while (i-->0) {
    struct synth_node *bus=synth->busv[i];
    synth_node_bus_terminate(bus);
  }
}

/* Drop busses cold if they do not have an associated rid (ie their data source is transient).
 */
 
void synth_song_stop_ridless(struct synth *synth) {
  int i=synth->busc;
  while (i-->0) {
    struct synth_node *bus=synth->busv[i];
    int rid=synth_node_bus_get_rid(bus);
    if ((rid>0)&&(rid<=0xffff)) continue;
    synth->busc--;
    memmove(synth->busv+i,synth->busv+i+1,sizeof(void*)*(synth->busc-i));
    synth_node_del(bus);
  }
}

/* Begin song, with finished serial data.
 */
 
void synth_song_begin(struct synth *synth,const void *src,int srcc,int repeat,int rid) {
  if (!src||(srcc<1)) return;
  if (synth->busc>=synth->busa) {
    int na=synth->busa+4;
    if (na>INT_MAX/sizeof(void*)) return;
    void *nv=realloc(synth->busv,sizeof(void*)*na);
    if (!nv) return;
    synth->busv=nv;
    synth->busa=na;
  }
  struct synth_node *bus=synth_node_new(&synth_node_type_bus,0,synth);
  if (!bus) return;
  if (
    (synth_node_bus_configure(bus,src,srcc,repeat,rid)<0)||
    (synth_node_ready(bus)<0)
  ) {
    synth_node_del(bus);
    return;
  }
  synth->busv[synth->busc++]=bus;
}

/* Play song, public entry points..
 */
 
void synth_play_song(struct synth *synth,int rid,int force,int repeat) {

  /* Without (force), if this song is already playing...
   * ...in the foreground, leave it be.
   * ...in the background, arrange to fade it back in.
   */
  if (rid&&!force) {
    struct synth_node *resumebus=0;
    int i=synth->busc;
    while (i-->0) {
      struct synth_node *bus=synth->busv[i];
      if (synth_node_bus_get_rid(bus)!=rid) continue;
      if (synth_node_bus_is_terminating(bus)) {
        synth_node_bus_unterminate(bus);
        resumebus=bus;
        break;
      } else {
        // It's already playing in the foreground.
        synth_node_bus_set_repeat(bus,repeat);
        return;
      }
    }
    if (resumebus) {
      for (i=synth->busc;i-->0;) {
        struct synth_node *bus=synth->busv[i];
        if (bus==resumebus) continue;
        synth_node_bus_terminate(bus);
      }
      return;
    }
  }
  
  synth_song_end(synth);
  if (!rid) return;

  struct synth_res *res=synth_res_get(synth,EGG_TID_song,rid);
  if (!res) return;
  synth_song_begin(synth,res->src,res->srcc,repeat,rid);
}

int synth_play_song_handoff(struct synth *synth,void *src,int srcc,int repeat) {
  if (!src||(srcc<1)) { src=0; srcc=0; }
  synth_song_stop_ridless(synth);
  if (synth->song_storage) free(synth->song_storage);
  synth->song_storage=src;
  synth_song_begin(synth,src,srcc,repeat,0x10000);
  return 0;
}

int synth_play_song_borrow(struct synth *synth,const void *src,int srcc,int repeat) {
  if (!src||(srcc<1)) { src=0; srcc=0; }
  // I guess it doesn't really matter, but drop the handoff storage to keep things neat.
  // Stopping other songs cold is part of our contract.
  synth_song_stop_ridless(synth);
  if (synth->song_storage) free(synth->song_storage);
  synth->song_storage=0;
  synth_song_begin(synth,src,srcc,repeat,0x10000);
  return 0;
}

/* Song status.
 */

int synth_get_song(const struct synth *synth) {
  int i=synth->busc;
  while (i-->0) {
    struct synth_node *bus=synth->busv[i];
    if (synth_node_bus_is_terminating(bus)) continue;
    int rid=synth_node_bus_get_rid(bus);
    if (rid>0) return rid;
  }
  return 0;
}

double synth_get_playhead(const struct synth *synth) {
  int i=synth->busc;
  while (i-->0) {
    struct synth_node *bus=synth->busv[i];
    if (synth_node_bus_is_terminating(bus)) continue;
    double p=synth_node_bus_get_playhead(bus);
    if (p<=0.0) return 0.001;
    return p;
  }
  return 0.0;
}

void synth_set_playhead(struct synth *synth,double s) {
  int i=synth->busc;
  while (i-->0) {
    struct synth_node *bus=synth->busv[i];
    if (synth_node_bus_is_terminating(bus)) continue;
    synth_node_bus_set_playhead(bus,s);
    return;
  }
}

int synth_count_busses(const struct synth *synth) {
  return synth->busc;
}
