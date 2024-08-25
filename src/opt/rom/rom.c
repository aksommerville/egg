#include "rom.h"

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
          len|=reader->src[reader->srcp++];
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
