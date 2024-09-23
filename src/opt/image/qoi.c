#include "image.h"
#include "opt/serial/serial.h"
#include "opt/stdlib/egg-stdlib.h"

#ifndef IMAGE_ENABLE_ENCODERS
  #define IMAGE_ENABLE_ENCODERS 1
#endif

/* Decode, header only.
 */
 
int qoi_decode_header(struct image *image,const void *_src,int srcc) {
  const uint8_t *src=_src;
  if ((srcc<12)||memcmp(src,"qoif",4)) return -1;
  int w=(src[4]<<24)|(src[5]<<16)|(src[6]<<8)|src[7];
  int h=(src[8]<<24)|(src[9]<<16)|(src[10]<<8)|src[11];
  if ((w<1)||(w>0x7fff)||(h<1)||(h>0x7fff)) return -1;
  int stride=w<<2;
  if (stride>INT_MAX/h) return -1;
  image->w=w;
  image->h=h;
  image->stride=stride;
  image->pixelsize=32;
  return stride*h;
}

/* Decode.
 */

struct image *qoi_decode(const void *_src,int srcc) {
  const uint8_t *src=_src;
  if (srcc<14) return 0;
  if (memcmp(src,"qoif",4)) return 0;
  int w=(src[4]<<24)|(src[5]<<16)|(src[6]<<8)|src[7];
  int h=(src[8]<<24)|(src[9]<<16)|(src[10]<<8)|src[11];
  struct image *image=image_new_alloc(32,w,h);
  if (!image) return 0;
  #define FAIL { image_del(image); return 0; }
  int srcp=14;
  uint8_t *dst=image->v;
  int dstp=0,dstc=image_get_pixels_length(image);
  uint8_t cache[64*4]={0};
  uint8_t prv[4]={0,0,0,0xff};
  while ((srcp<srcc)&&(dstp<dstc)) {
    uint8_t lead=src[srcp++];
    
    if (lead==0xfe) { // 11111110 rrrrrrrr gggggggg bbbbbbbb : QOI_OP_RGB. Alpha from previous pixel.
      if (srcp>srcc-3) FAIL
      uint8_t r=src[srcp++];
      uint8_t g=src[srcp++];
      uint8_t b=src[srcp++];
      dst[dstp++]=r;
      dst[dstp++]=g;
      dst[dstp++]=b;
      dst[dstp++]=prv[3];
      prv[0]=r;
      prv[1]=g;
      prv[2]=b;
      int cachep=((r*3+g*5+b*7+prv[3]*11)&0x3f)<<2;
      memcpy(cache+cachep,prv,4);
      continue;
    }
    
    if (lead==0xff) { // 11111111 rrrrrrrr gggggggg bbbbbbbb aaaaaaaa : QOI_OP_RGBA
      if (srcp>srcc-4) FAIL
      uint8_t r=src[srcp++];
      uint8_t g=src[srcp++];
      uint8_t b=src[srcp++];
      uint8_t a=src[srcp++];
      dst[dstp++]=r;
      dst[dstp++]=g;
      dst[dstp++]=b;
      dst[dstp++]=a;
      prv[0]=r;
      prv[1]=g;
      prv[2]=b;
      prv[3]=a;
      int cachep=((r*3+g*5+b*7+a*11)&0x3f)<<2;
      memcpy(cache+cachep,prv,4);
      continue;
    }
    
    switch (lead&0xc0) {
      case 0x00: { // 00iiiiii : QOI_OP_INDEX. Emit cache[i].
          int cachep=lead<<2;
          memcpy(dst+dstp,cache+cachep,4);
          memcpy(prv,cache+cachep,4);
          dstp+=4;
        } break;
        
      case 0x40: { // 01rrggbb : QOI_OP_DIFF. (r,g,b) are (-2..1), difference from previous pixel.
          int dr=((lead>>4)&3)-2;
          int dg=((lead>>2)&3)-2;
          int db=(lead&3)-2;
          uint8_t r=prv[0]+dr;
          uint8_t g=prv[1]+dg;
          uint8_t b=prv[2]+db;
          dst[dstp++]=r;
          dst[dstp++]=g;
          dst[dstp++]=b;
          dst[dstp++]=prv[3];
          prv[0]=r;
          prv[1]=g;
          prv[2]=b;
          int cachep=((r*3+g*5+b*7+prv[3]*11)&0x3f)<<2;
          memcpy(cache+cachep,prv,4);
        } break;
        
      case 0x80: { // 10gggggg rrrrbbbb : QOI_OP_LUMA. (g) in (-32..31), (r,b) in (-8..7)+(g), difference from previous pixel.
          if (srcp>srcc-1) FAIL
          int dg=(lead&0x3f)-32;
          uint8_t next=src[srcp++];
          int dr=(next>>4)-8+dg;
          int db=(next&0xf)-8+dg;
          uint8_t r=prv[0]+dr;
          uint8_t g=prv[1]+dg;
          uint8_t b=prv[2]+db;
          dst[dstp++]=r;
          dst[dstp++]=g;
          dst[dstp++]=b;
          dst[dstp++]=prv[3];
          prv[0]=r;
          prv[1]=g;
          prv[2]=b;
          int cachep=((r*3+g*5+b*7+prv[3]*11)&0x3f)<<2;
          memcpy(cache+cachep,prv,4);
        } break;
        
      case 0xc0: { // 11llllll : QOI_OP_RUN. Emit previous pixel (l+1) times, 1..62. 63 and 64 are illegal.
          int len=(lead&0x3f)+1;
          while ((len-->0)&&(dstp<dstc)) {
            memcpy(dst+dstp,prv,4);
            dstp+=4;
          }
        } break;
        
    }
  }
  // Spec implies that incomplete data is an error. We won't enforce that.
  return image;
}

#if IMAGE_ENABLE_ENCODERS

/* Encode.
 */

int qoi_encode(struct sr_encoder *dst,struct image *src) {
  if (image_force_rgba(src)<0) return -1;

  // Header.
  if (sr_encode_raw(dst,"qoif",4)<0) return -1;
  if (sr_encode_intbe(dst,src->w,4)<0) return -1;
  if (sr_encode_intbe(dst,src->h,4)<0) return -1;
  if (sr_encode_u8(dst,4)<0) return -1; // RGBA
  if (sr_encode_u8(dst,1)<0) return -1; // linear, as opposed to sRGB
  
  // Payload.
  const uint8_t *srcp=(uint8_t*)src->v;
  int srcc=src->w*src->h; // remaining input pixel count
  uint8_t prev[]={0,0,0,0xff};
  uint8_t buf[256]={0};
  int runlen=0;
  while (srcc-->0) {
  
    // Pop the next pixel.
    uint8_t r=*srcp++;
    uint8_t g=*srcp++;
    uint8_t b=*srcp++;
    uint8_t a=*srcp++;
    
    // Continue or terminate run.
    if ((r==prev[0])&&(g==prev[1])&&(b==prev[2])&&(a==prev[3])) {
      runlen++;
      if (runlen>=62) {
        if (sr_encode_u8(dst,0xfd)<0) return -1;
        runlen=0;
      }
      continue;
    }
    if (runlen) {
      if (sr_encode_u8(dst,0xc0|(runlen-1))<0) return -1;
      runlen=0;
    }
    
    // QOI_OP_INDEX if it matches the buffer, otherwise update buffer.
    int bufp=((r*3+g*5+b*7+a*11)&0x3f)<<2;
    if ((r==buf[bufp])&&(g==buf[bufp+1])&&(b==buf[bufp+2])&&(a==buf[bufp+3])) {
      if (sr_encode_u8(dst,bufp>>2)<0) return -1;
      prev[0]=r;
      prev[1]=g;
      prev[2]=b;
      prev[3]=a;
      continue;
    }
    buf[bufp++]=r;
    buf[bufp++]=g;
    buf[bufp++]=b;
    buf[bufp++]=a;
    
    // Check difference per channel from previous, and update previous.
    int dr=r-prev[0];
    int dg=g-prev[1];
    int db=b-prev[2];
    int da=a-prev[3];
    prev[0]=r;
    prev[1]=g;
    prev[2]=b;
    prev[3]=a;
    
    // If alpha didn't change, check for QOI_OP_DIFF and QOI_OP_LUMA.
    if (!da) {
      if ((dr>=-2)&&(dr<=1)&&(dg>=-2)&&(dg<=1)&&(db>=-2)&&(db<=1)) {
        if (sr_encode_u8(dst,0x40|((dr+2)<<4)|((dg+2)<<2)|(db+2))<0) return -1;
        continue;
      }
      if ((dg>=-32)&&(dg<=31)) {
        dr-=dg;
        db-=dg;
        if ((dr>=-8)&&(dr<=7)&&(db>=-8)&&(db<=7)) {
          if (sr_encode_u8(dst,0x80|(dg+32))<0) return -1;
          if (sr_encode_u8(dst,((dr+8)<<4)|(db+8))<0) return -1;
          continue;
        }
      }
      // QOI_OP_RGB
      if (sr_encode_u8(dst,0xfe)<0) return -1;
      if (sr_encode_u8(dst,r)<0) return -1;
      if (sr_encode_u8(dst,g)<0) return -1;
      if (sr_encode_u8(dst,b)<0) return -1;
      continue;
    }
    
    // Finally, QOI_OP_RGBA
    if (sr_encode_u8(dst,0xff)<0) return -1;
    if (sr_encode_u8(dst,r)<0) return -1;
    if (sr_encode_u8(dst,g)<0) return -1;
    if (sr_encode_u8(dst,b)<0) return -1;
    if (sr_encode_u8(dst,a)<0) return -1;
  
  }
  if (runlen) {
    if (sr_encode_u8(dst,0xc0|(runlen-1))<0) return -1;
  }
  
  // Trailer.
  if (sr_encode_raw(dst,"\0\0\0\0\0\0\0\1",8)<0) return -1;
  
  return 0;
}

#endif
