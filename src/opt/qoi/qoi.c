#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include "opt/serial/serial.h"
#include "qoi.h"
#include <stdio.h>

#define QOI_SIZE_LIMIT 4096

/* Delete image.
 */
 
void qoi_image_del(struct qoi_image *image) {
  if (!image) return;
  if (image->v) free(image->v);
  free(image);
}

/* Decode into existing image.
 */
 
static int qoi_decode_inner(struct qoi_image *image,const uint8_t *src,int srcc) {
  uint8_t *dst=(uint8_t*)image->v;
  int srcp=14,dstp=0;
  int pxc=image->w*image->h;
  int dstend=pxc*4-4;
  uint8_t prev[4]={0,0,0,0xff};
  uint8_t buf[256]={0};
  
  #define PREVTOBUF { \
    int bufp=((prev[0]*3+prev[1]*5+prev[2]*7+prev[3]*11)&0x3f)<<2; \
    buf[bufp++]=prev[0]; \
    buf[bufp++]=prev[1]; \
    buf[bufp++]=prev[2]; \
    buf[bufp]=prev[3]; \
  }
  
  while (srcp<srcc) {
    if (dstp>dstend) break;
    uint8_t lead=src[srcp++];
    
    if (lead==0xfe) { // QOI_OP_RGB
      memcpy(prev,src+srcp,3);
      srcp+=3;
      memcpy(dst+dstp,prev,4);
      dstp+=4;
      PREVTOBUF
      continue;
    }
    
    if (lead==0xff) { // QOI_OP_RGBA
      memcpy(prev,src+srcp,4);
      srcp+=4;
      memcpy(dst+dstp,prev,4);
      dstp+=4;
      PREVTOBUF
      continue;
    }
      
    switch (lead&0xc0) {
      case 0x00: { // QOI_OP_INDEX
          int bufp=(lead&0x3f)<<2;
          memcpy(prev,buf+bufp,4);
          memcpy(dst+dstp,prev,4);
          dstp+=4;
        } break;
        
      case 0x40: { // QOI_OP_DIFF
          int dr=((lead>>4)&3)-2;
          int dg=((lead>>2)&3)-2;
          int db=(lead&3)-2;
          prev[0]+=dr;
          prev[1]+=dg;
          prev[2]+=db;
          memcpy(dst+dstp,prev,4);
          dstp+=4;
          PREVTOBUF
        } break;
        
      case 0x80: { // QOI_OP_LUMA
          int dg=(lead&0x3f)-32;
          int dr=dg+(src[srcp]>>4)-8;
          int db=dg+(src[srcp]&0x0f)-8;
          srcp++;
          prev[0]+=dr;
          prev[1]+=dg;
          prev[2]+=db;
          memcpy(dst+dstp,prev,4);
          dstp+=4;
          PREVTOBUF
        } break;
        
      case 0xc0: { // QOI_OP_RUN
          int c=(lead&0x3f)+1;
          while (c-->0) {
            if (dstp>dstend) break;
            memcpy(dst+dstp,prev,4);
            dstp+=4;
          }
        } break;
    }
  }
  #undef PREVTOBUF
  return 0;
}

/* Decode.
 */

struct qoi_image *qoi_decode(const void *src,int srcc) {
  
  const uint8_t *SRC=src;
  if (srcc<22) return 0; // 14 header + 8 EOF
  if (memcmp(src,"qoif",4)) return 0;
  int w=(SRC[4]<<24)|(SRC[5]<<16)|(SRC[6]<<8)|SRC[7];
  int h=(SRC[8]<<24)|(SRC[9]<<16)|(SRC[10]<<8)|SRC[11];
  // [12]=channels, [13]=colorspace, don't care.
  if ((w<1)||(w>QOI_SIZE_LIMIT)) return 0;
  if ((h<1)||(h>QOI_SIZE_LIMIT)) return 0;
  
  struct qoi_image *dst=calloc(1,sizeof(struct qoi_image));
  if (!dst) return 0;
  if (!(dst->v=malloc(w*h*4))) {
    free(dst);
    return 0;
  }
  dst->w=w;
  dst->h=h;
  int err=qoi_decode_inner(dst,SRC,srcc);
  if (err<0) {
    qoi_image_del(dst);
    return 0;
  }
  
  return dst;
}

/* Encode.
 */

int qoi_encode(struct sr_encoder *dst,const struct qoi_image *src) {

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
