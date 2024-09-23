#include "text_internal.h"

struct strings strings={0};

/* Cleanup.
 */
 
void strings_cleanup() {
  if (strings.resv) free(strings.resv);
  memset(&strings,0,sizeof(strings));
}

/* Append to TOC.
 * TOC must be sorted by (tid,rid).
 * We don't validate that because we assume the TOC is only built up by iterating ROM, which is sorted the same way.
 */
 
static int strings_toc_append(int tid,int rid,const void *v,int c) {
  if (strings.resc>=strings.resa) {
    int na=strings.resa+64;
    if (na>INT_MAX/sizeof(struct text_res)) return -1;
    void *nv=realloc(strings.resv,sizeof(struct text_res)*na);
    if (!nv) return -1;
    strings.resv=nv;
    strings.resa=na;
  }
  struct text_res *res=strings.resv+strings.resc++;
  res->id=(tid<<16)|rid;
  res->v=v;
  res->c=c;
  return 0;
}

/* Set rom.
 */
 
void strings_set_rom(const void *src,int srcc) {
  if (!src||(srcc<0)) srcc=0;
  strings.rom=src;
  strings.romc=srcc;
  strings.resc=0;
  struct rom_reader reader;
  if (rom_reader_init(&reader,strings.rom,strings.romc)>=0) {
    struct rom_res *res;
    while (res=rom_reader_next(&reader)) {
      if ((res->tid==EGG_TID_strings)||(res->tid==EGG_TID_image)) {
        strings_toc_append(res->tid,res->rid,res->v,res->c);
      }
    }
  }
}

/* Recheck global language.
 */
 
void strings_check_language() {
  strings.lang=egg_get_language();
  // Turns out, we're not caching anything based on current language, so nothing more to do.
}

/* Get string.
 */
 
struct strings_get_ctx {
  void *dstpp;
  int dstc;
  int index;
};

static int strings_get_cb(int p,const char *v,int c,void *userdata) {
  struct strings_get_ctx *ctx=userdata;
  if (p>ctx->index) return -1;
  if (p<ctx->index) return 0;
  *(const void**)ctx->dstpp=v;
  ctx->dstc=c;
  return 1;
}
 
int strings_get(void *dstpp,int rid,int index) {
  if (index<0) return 0;
  if (rid<1) return 0;
  if (rid>0xffff) return 0;
  if (rid<64) {
    if (!strings.lang) strings.lang=egg_get_language();
    rid|=strings.lang<<6;
  }
  uint32_t id=(EGG_TID_strings<<16)|rid;
  int lo=0,hi=strings.resc;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
    uint32_t q=strings.resv[ck].id;
         if (id<q) hi=ck;
    else if (id>q) lo=ck;
    else {
      struct strings_get_ctx ctx={.dstpp=dstpp,.index=index};
      rom_read_strings(strings.resv[ck].v,strings.resv[ck].c,strings_get_cb,&ctx);
      return ctx.dstc;
    }
  }
  return 0;
}
