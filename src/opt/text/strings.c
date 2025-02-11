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

/* Represent signed decimal integer.
 */
 
int strings_decsint_repr(char *dst,int dsta,int src) {
  if (src<0) {
    int limit=-10,dstc=2,i;
    while (src<=limit) { dstc++; if (limit<INT_MIN/10) break; limit*=10; }
    if (dstc>dsta) return dstc;
    for (i=dstc;i-->0;src/=10) dst[i]='0'-src%10;
    dst[0]='-';
    return dstc;
  } else {
    int limit=10,dstc=1,i;
    while (src>=limit) { dstc++; if (limit>INT_MAX/10) break; limit*=10; }
    if (dstc>dsta) return dstc;
    for (i=dstc;i-->0;src/=10) dst[i]='0'+src%10;
    return dstc;
  }
}

/* Apply one string insertion.
 */
 
static int strings_insert(char *dst,int dsta,const struct strings_insertion *ins) {
  switch (ins->mode) {
  
    case 'i': return strings_decsint_repr(dst,dsta,ins->i);
      
    case 's': {
        if (!ins->s.v) return 0;
        if (ins->s.c<=0) return 0;
        if (ins->s.c<=dsta) memcpy(dst,ins->s.v,ins->s.c);
        return ins->s.c;
      }
      
    case 'r': {
        const char *src;
        int srcc=strings_get(&src,ins->r.rid,ins->r.ix);
        if (srcc<=0) return 0;
        if (srcc<=dsta) memcpy(dst,src,srcc);
        return srcc;
      }
      
  }
  return 0;
}

/* Apply string format.
 */
 
int strings_format(char *dst,int dsta,int rid,int index,const struct strings_insertion *insv,int insc) {
  const char *src;
  int srcc=strings_get(&src,rid,index);
  int dstc=0,srcp=0;
  while (srcp<srcc) {
    if (src[srcp]=='%') {
      srcp++;
      if ((srcp<srcc)&&(src[srcp]>='0')&&(src[srcp]<='9')) {
        int insp=src[srcp++]-'0';
        if (insp<insc) {
          dstc+=strings_insert(dst+dstc,dsta-dstc,insv+insp);
        }
      } else {
        if ((srcp<srcc)&&(src[srcp]=='%')) srcp++;
        if (dstc<dsta) dst[dstc]='%';
        dstc++;
      }
    } else {
      if (dstc<dsta) dst[dstc]=src[srcp];
      dstc++;
      srcp++;
    }
  }
  if (dstc<dsta) dst[dstc]=0;
  return dstc;
}

/* Language by index.
 */
 
int strings_lang_by_index(int p) {
  if (p<0) return -1;
  int pvlang=-1;
  const struct text_res *res=strings.resv;
  int i=strings.resc;
  for (;i-->0;res++) {
    int tid=(res->id>>16)&0xffff;
    if (tid!=EGG_TID_strings) continue;
    int rid=res->id&0xffff;
    int lang=rid>>6;
    if (lang!=pvlang) {
      if (!p--) return lang;
      pvlang=lang;
    }
  }
  return -1;
}
