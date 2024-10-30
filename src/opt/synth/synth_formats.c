#include "synth_formats.h"
#include "opt/serial/serial.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <stdlib.h>

/* Prepare EGS reader.
 */
 
int synth_egs_reader_init(struct synth_egs_reader *reader,const void *src,int srcc) {
  if (!reader||!src) return -1;
  if ((srcc<4)||memcmp(src,"\0EGS",4)) { reader->stage='!'; return -1; }
  reader->v=src;
  reader->c=srcc;
  reader->p=4;
  reader->stage='c';
  return 0;
}

/* Next EGS channel.
 */

int synth_egs_reader_next_channel(struct synth_egs_channel *dst,struct synth_egs_reader *reader) {
  if (!dst||!reader) return -1;
  if (reader->stage=='!') return -1;
  if (reader->stage!='c') return 0;
  if (reader->p>=reader->c) { reader->stage=0; return 0; }
  uint8_t chid=reader->v[reader->p++];
  if (chid==0xff) { reader->stage='e'; return 0; }
  dst->chid=chid;
  if (reader->p>reader->c-5) { reader->stage='!'; return -1; }
  dst->master=reader->v[reader->p++];
  dst->mode=reader->v[reader->p++];
  dst->c=(reader->v[reader->p]<<16)|(reader->v[reader->p+1]<<8)|reader->v[reader->p+2];
  reader->p+=3;
  if (reader->p>reader->c-dst->c) { reader->stage='!'; return -1; }
  dst->v=reader->v+reader->p;
  reader->p+=dst->c;
  return 1;
}

/* All remaining EGS channels.
 */
 
int synth_egs_reader_all_channels(void *dstpp,struct synth_egs_reader *reader) {
  if (!dstpp||!reader) return -1;
  if (reader->stage=='!') return -1;
  if (reader->stage!='c') return 0;
  *(const void**)dstpp=reader->v+reader->p;
  int dstc=0;
  for (;;) {
    if (reader->p>=reader->c) {
      reader->stage=0;
      break;
    }
    if (reader->v[reader->p]==0xff) {
      reader->p++;
      reader->stage='e';
      break;
    }
    if (reader->p>reader->c-6) { reader->stage='!'; return -1; }
    int paylen=(reader->v[reader->p+3]<<16)|(reader->v[reader->p+1]<<8)|reader->v[reader->p+2];
    reader->p+=6;
    dstc+=6;
    if (reader->p>reader->c-paylen) { reader->stage='!'; return -1; }
    reader->p+=paylen;
    dstc+=paylen;
  }
  return dstc;
}

/* Next EGS event.
 */

int synth_egs_reader_next_event(struct synth_egs_event *dst,struct synth_egs_reader *reader) {
  if (!dst||!reader) return -1;
  if (reader->stage=='!') return -1;
  if (reader->stage!='e') return 0;
  if (reader->p>=reader->c) { reader->stage=0; return 0; }
  uint8_t lead=reader->v[reader->p++];
  if (!lead) { reader->stage=0; return 0; }
  dst->v=0;
  dst->c=0;
  dst->noteid=0xff;
  if (!(lead&0x80)) {
    dst->delay=lead&0x3f;
    if (lead&0x40) dst->delay=(dst->delay+1)*64;
    return 1;
  }
  dst->delay=0;
  switch (lead&0xf0) {
    case 0x80:
    case 0x90:
    case 0xa0: {
        if (reader->p>reader->c-2) { reader->stage='!'; return -1; }
        uint8_t a=reader->v[reader->p++];
        uint8_t b=reader->v[reader->p++];
        dst->chid=lead&0x0f;
        dst->noteid=a>>1;
        dst->velocity=((a<<6)&0x40)|(b>>2);
        switch (lead&0xf0) {
          case 0x80: dst->duration=(b&0x03)*16; break;
          case 0x90: dst->velocity&=0x78; dst->velocity|=dst->velocity>>4; dst->duration=((b&0x1f)+1)*64; break;
          case 0xa0: dst->velocity&=0x78; dst->velocity|=dst->velocity>>4; dst->duration=((b&0x1f)+1)*512; break;
        }
        if (!dst->velocity) dst->velocity=1;
      } break;
    // Reserved events, but lengths have been defined:
    #define RESERVED(evlen) { \
      if (reader->p-1>reader->c-evlen) { reader->stage='!'; return -1; } \
      dst->v=reader->v+reader->p-1; \
      dst->c=evlen; \
      reader->p+=evlen-1; \
    }
    case 0xb0: RESERVED(1) break;
    case 0xc0: RESERVED(2) break;
    case 0xd0: RESERVED(3) break;
    case 0xe0: RESERVED(3) break;
    case 0xf0: {
        if (reader->p>reader->c-2) { reader->stage='!'; return -1; }
        int paylen=reader->v[reader->p+2];
        if (reader->p+2+paylen>reader->c) { reader->stage='!'; return -1; }
        reader->v=reader->v+reader->p-1;
        reader->c=3+paylen;
        reader->p+=2+paylen;
      } break;
    #undef RESERVED
    default: reader->stage='!'; return -1;
  }
  return 1;
}
