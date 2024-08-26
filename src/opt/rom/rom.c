#include "rom.h"
#include "egg/egg.h"

/* Init.
 */
 
int rom_reader_init(struct rom_reader *reader,const void *src,int srcc) {
  if (!reader||!src||(srcc<4)) return -1;
  if (((unsigned char*)src)[0]!=0) return -1;
  if (((char*)src)[1]!='E') return -1;
  if (((char*)src)[2]!='G') return -1;
  if (((char*)src)[3]!='G') return -1;
  reader->src=src;
  reader->srcc=srcc;
  reader->srcp=4;
  reader->tid=1;
  reader->rid=1;
  reader->status=0;
  return 0;
}

/* Next resource.
 */

struct rom_res *rom_reader_next(struct rom_reader *reader) {
  if (reader->status) return 0;
  #define FINISH { reader->status=1; return 0; }
  #define FAIL { reader->status=-1; return 0; }
  for (;;) {
    if (reader->srcp>=reader->srcc) FINISH
    unsigned char lead=reader->src[reader->srcp++];
    if (!lead) FINISH
    switch (lead&0xc0) {
  
      case 0x00: {
          reader->tid+=lead;
          if (reader->tid>0xff) FAIL
          reader->rid=1;
        } break;
        
      case 0x40: {
          if (reader->srcp>reader->srcc-1) FAIL
          int d=(lead&0x3f)<<8;
          d|=reader->src[reader->srcp++];
          if (!d) FAIL
          reader->rid+=d;
          if (reader->rid>0xffff) FAIL
        } break;
        
      case 0x80: {
          if (reader->srcp>reader->srcc-1) FAIL
          int len=(lead&0x3f)<<8;
          len|=reader->src[reader->srcp++];
          len+=1;
          if (reader->srcp>reader->srcc-len) FAIL
          if (reader->rid>0xffff) FAIL
          reader->storage.tid=reader->tid;
          reader->storage.rid=reader->rid;
          reader->storage.v=reader->src+reader->srcp;
          reader->storage.c=len;
          reader->srcp+=len;
          reader->rid++;
          return &reader->storage;
        }
        
      case 0xc0: {
          if (reader->srcp>reader->srcc-2) FAIL
          int len=(lead&0x3f)<<16;
          len|=reader->src[reader->srcp++]<<8;
          len|=reader->src[reader->srcp++];
          len+=16385;
          if (reader->srcp>reader->srcc-len) FAIL
          if (reader->rid>0xffff) FAIL
          reader->storage.tid=reader->tid;
          reader->storage.rid=reader->rid;
          reader->storage.v=reader->src+reader->srcp;
          reader->storage.c=len;
          reader->srcp+=len;
          reader->rid++;
          return &reader->storage;
        }
    }
  }
  #undef FINISH
  #undef FAIL
}

/* Read strings resource.
 */
 
int rom_read_strings(
  const void *src,int srcc,
  int (*cb)(int p,const char *v,int c,void *userdata),
  void *userdata
) {
  if (!src||(srcc<4)) return -1;
  const unsigned char *SRC=src;
  if ((SRC[0]!=0x00)||(SRC[1]!='E')||(SRC[2]!='S')||(SRC[3]!=0xff)) return -1;
  int srcp=4,index=0,err;
  while (srcp<srcc) {
    int len=SRC[srcp++];
    if (len&0x80) {
      if (srcp>=srcc) break;
      len=((len&0x7f)<<8)|SRC[srcp++];
    }
    if (srcp>srcc-len) break;
    if (len) {
      if (err=cb(index,(char*)SRC+srcp,len,userdata)) return err;
    }
    srcp+=len;
    index++;
  }
  return 0;
}

/* String by index.
 */
 
struct rom_string_by_index_ctx {
  void *dstpp;
  int p;
};

static int rom_string_by_index_cb(int p,const char *v,int c,void *userdata) {
  struct rom_string_by_index_ctx *ctx=userdata;
  if (p==ctx->p) {
    *(const void**)ctx->dstpp=v;
    return c;
  }
  if (p>ctx->p) return -1;
  return 0;
}
 
int rom_string_by_index(void *dstpp,const void *src,int srcc,int p) {
  if (p<0) return -1;
  struct rom_string_by_index_ctx ctx={.dstpp=dstpp,.p=p};
  return rom_read_strings(src,srcc,rom_string_by_index_cb,&ctx);
}

/* Read metadata resource.
 */
 
int rom_read_metadata(
  const void *src,int srcc,
  int (*cb)(const char *k,int kc,const char *v,int vc,void *userdata),
  void *userdata
) {
  if (!src||(srcc<4)) return -1;
  const unsigned char *SRC=src;
  if ((SRC[0]!=0x00)||(SRC[1]!='E')||(SRC[2]!='M')||(SRC[3]!=0xff)) return -1;
  int srcp=4,err;
  while (srcp<srcc) {
    int kc=SRC[srcp++];
    if (!kc||(srcp>=srcc)) break;
    int vc=SRC[srcp++];
    if (srcp>srcc-vc-kc) break;
    const char *k=(char*)SRC+srcp; srcp+=kc;
    const char *v=(char*)SRC+srcp; srcp+=vc;
    if (err=cb(k,kc,v,vc,userdata)) return err;
  }
  return 0;
}

/* Resolve metadata field against entire ROM.
 */
 
struct rom_meta_ctx {
  const char *k; // Without the '$' suffix.
  int kc;
  const char *vn,*vl; // n=neutral, l=languaged
  int vnc,vlc;
};

static int rom_memcmp(const char *a,const char *b,int c) {
  for (;c-->0;a++,b++) {
    if (*a==*b) continue;
    return 1;
  }
  return 0;
}

static int rom_lookup_metadata_mcb(const char *k,int kc,const char *v,int vc,void *userdata) {
  struct rom_meta_ctx *ctx=userdata;
  if ((kc==ctx->kc+1)&&!rom_memcmp(k,ctx->k,ctx->kc)&&(k[ctx->kc]=='$')) {
    ctx->vl=v;
    ctx->vlc=vc;
    return (ctx->vnc?1:0);
  }
  if ((kc==ctx->kc)&&!rom_memcmp(k,ctx->k,kc)) {
    ctx->vn=v;
    ctx->vnc=vc;
    return (ctx->vlc?1:0);
  }
  return 0;
}

int rom_lookup_metadata(void *dstpp,const void *src,int srcc,const char *k,int kc,int lang) {
  if (!k) return -1;
  if (kc<0) { kc=0; while (k[kc]) kc++; }

  // Find metadata:1 and strings:lang:1.
  const unsigned char *mv=0,*sv=0;
  int mc=0,sc=0;
  int srid=-1;
  if (lang) srid=(lang<<6)|1;
  struct rom_reader reader;
  if (rom_reader_init(&reader,src,srcc)<0) return -1;
  struct rom_res *res;
  while (res=rom_reader_next(&reader)) {
    if ((res->tid==EGG_TID_metadata)&&(res->rid==1)) {
      mv=res->v;
      mc=res->c;
    } else if ((res->tid==EGG_TID_strings)&&(res->rid==srid)) {
      sv=res->v;
      sc=res->c;
      break;
    } else if (res->tid>EGG_TID_strings) {
      break;
    }
  }
  
  // Find "k" and "k$" in metadata:1.
  struct rom_meta_ctx mctx={
    .k=k,
    .kc=kc,
  };
  rom_read_metadata(mv,mc,rom_lookup_metadata_mcb,&mctx);
  
  // If there is a dollar version, search in strings:lang:1.
  if (mctx.vlc&&sc) {
    int index=0,p=0;
    for (;p<mctx.vlc;p++) {
      char ch=mctx.vl[p];
      if ((ch<'0')||(ch>'9')) { index=-1; break; }
      index*=10;
      index+=ch-'0';
      if (index>32767) { index=-1; break; }
    }
    if (index>=0) {
      int dstc=rom_string_by_index(dstpp,sv,sc,index);
      if (dstc>0) return dstc;
    }
  }
  
  // Return the neutral version, which might be empty.
  *(const void**)dstpp=mctx.vn;
  return mctx.vnc;
}
