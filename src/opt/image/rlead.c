#include "image.h"
#include "opt/serial/serial.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>

struct rlead_pixel_writer {
  uint8_t *dstrow;
  uint8_t *dstp;
  uint8_t dstmask;
  int remx; // pixels remaining in row
  int remy; // rows remaining in image, including the one in progress
  int w;
  int stride;
};

static void rlead_pixel_writer_init(struct rlead_pixel_writer *writer,uint8_t *dst,int w,int h,int stride) {
  writer->dstrow=dst;
  writer->dstp=dst;
  writer->dstmask=0x80;
  writer->remx=w;
  writer->remy=h;
  writer->w=w;
  writer->stride=stride;
}

static inline void rlead_pixel_write(struct rlead_pixel_writer *writer,int v) {
  if (writer->remx<1) {
    writer->remy--;
    if (writer->remy<1) { writer->remx=writer->remy=0; return; }
    writer->remx=writer->w;
    writer->dstrow+=writer->stride;
    writer->dstp=writer->dstrow;
    writer->dstmask=0x80;
  }
  if (v) (*writer->dstp)|=writer->dstmask;
  // Don't bother clearing bits on zeros -- the output buffer starts zeroed.
  writer->remx--;
  if (writer->dstmask==0x01) {
    writer->dstmask=0x80;
    writer->dstp++;
  } else {
    writer->dstmask>>=1;
  }
}

struct rlead_word_reader {
  const uint8_t *src;
  int srcc; // Bytes remaining in (src), including the current one.
  uint8_t srcmask;
};

static void rlead_word_reader_init(struct rlead_word_reader *reader,const uint8_t *src,int srcc) {
  reader->src=src;
  reader->srcc=srcc;
  reader->srcmask=0x80;
}

static inline int rlead_word_read(struct rlead_word_reader *reader,int len) {
  if (len<1) return 0;
  if (len>30) return 0;
  if (reader->srcc<1) return 0;
  uint32_t word=0;
  while (len-->0) {
    word<<=1;
    if ((*reader->src)&reader->srcmask) word|=1;
    if (reader->srcmask==0x01) {
      reader->srcmask=0x80;
      reader->src++;
      reader->srcc--;
      if (reader->srcc<1) {
        word<<=len;
        break;
      }
    } else {
      reader->srcmask>>=1;
    }
  }
  return word;
}

static void rlead_unfilter(uint8_t *rp,int stride,int h) {
  // Read forward.
  uint8_t *wp=rp+stride;
  int bc=stride*(h-1);
  for (;bc-->0;wp++,rp++) (*wp)^=(*rp);
}

struct image *rlead_decode(const void *_src,int srcc) {
  const uint8_t *src=_src;
  if (srcc<9) return 0;
  if (memcmp(src,"\x00rld",4)) return 0;
  int w=(src[4]<<8)|src[5];
  int h=(src[6]<<8)|src[7];
  struct image *image=image_new_alloc(1,w,h);
  if (!image) return 0;
  uint8_t flags=src[8];
  if (flags&4) image->hint|=IMAGE_HINT_ALPHA;
  else image->hint|=IMAGE_HINT_LUMA;
  int srcp=9;
  struct rlead_pixel_writer writer;
  rlead_pixel_writer_init(&writer,image->v,w,h,image->stride);
  struct rlead_word_reader reader;
  rlead_word_reader_init(&reader,src+srcp,srcc-srcp);
  int color=(flags&0x02)?1:0;
  
  while (reader.srcc) {
    int word=rlead_word_read(&reader,3);
    int i=word+1;
    while (i-->0) rlead_pixel_write(&writer,color);
    if (word==7) {
      int wordsize=4;
      for (;;) {
        word=rlead_word_read(&reader,wordsize);
        for (i=word;i-->0;) rlead_pixel_write(&writer,color);
        if (word!=(1<<wordsize)-1) break;
        if (wordsize++>=30) {
          image_del(image);
          return 0;
        }
      }
    }
    color^=1;
  }
  
  if (flags&0x01) rlead_unfilter(image->v,image->stride,h);
  return image;
}

/* Decode header.
 */
 
int rlead_decode_header(struct image *image,const void *_src,int srcc) {
  const uint8_t *src=_src;
  if (srcc<9) return 0;
  if (memcmp(src,"\x00rld",4)) return 0;
  int w=(src[4]<<8)|src[5];
  int h=(src[6]<<8)|src[7];
  if ((w<1)||(w>0x7fff)||(h<1)||(h>0x7fff)) return -1;
  int stride=(w+7)>>3;
  if (stride>INT_MAX/h) return -1;
  image->w=w;
  image->h=h;
  image->stride=stride;
  image->pixelsize=1;
  return image->stride*image->h;
}

/* Encoder context.
 */
 
struct rlead_encoder {
  struct sr_encoder *dst; // WEAK
  uint8_t *filtered;
  int w,h,stride,pxc;
  uint8_t dstbuf,dstmask;
  int using_filter;
  const struct image *image;
};

static void rlead_encoder_cleanup(struct rlead_encoder *encoder) {
  if (encoder->filtered) free(encoder->filtered);
}

/* Emit one value bitwise.
 */
 
static int rlead_emit_bits(struct rlead_encoder *encoder,int v,int startmask) {
  for (;startmask;startmask>>=1) {
    if (v&startmask) encoder->dstbuf|=encoder->dstmask;
    if (encoder->dstmask==1) {
      if (sr_encode_u8(encoder->dst,encoder->dstbuf)<0) return -1;
      encoder->dstbuf=0;
      encoder->dstmask=0x80;
    } else {
      encoder->dstmask>>=1;
    }
  }
  return 0;
}

/* Emit one run length.
 */
 
static int rlead_emit_run(struct rlead_encoder *encoder,int runlen) {
   
  if (runlen<1) return -1;
  runlen--;
   
  int range=8;
  for (;;) {
    int limit=range-1;
    if (runlen<limit) { // end of sequence, after we emit this. sic '<' not '<='
      if (rlead_emit_bits(encoder,runlen,range>>1)<0) return -1;
      break;
    }
    if (rlead_emit_bits(encoder,limit,range>>1)<0) return -1;
    runlen-=limit;
    if (range>=0x40000000) return -1;
    range<<=1;
  }
  
  return 0;
}

/* Encode to rlead, having acquired the filtered image.
 */
 
static int rlead_encode_inner(struct rlead_encoder *encoder) {

  uint8_t flags=0;
  if (encoder->filtered[0]&0x80) flags|=0x01; // initial color
  if (encoder->using_filter) flags|=0x02;
  if (encoder->image->hint&IMAGE_HINT_ALPHA) flags|=0x04;

  // 9-byte header: "\0rld",u16 w,u16 h,u8 flags(1=color,2=filter)
  if (sr_encode_raw(encoder->dst,"\0rld",4)<0) return -1;
  if (sr_encode_intbe(encoder->dst,encoder->w,2)<0) return -1;
  if (sr_encode_intbe(encoder->dst,encoder->h,2)<0) return -1;
  if (sr_encode_u8(encoder->dst,flags)<0) return -1;
  
  // Read pixels.
  encoder->dstbuf=0;
  encoder->dstmask=0x80;
  int runcolor=(encoder->filtered[0]&0x80)?1:0;
  int runlen=0;
  const uint8_t *srcrow=encoder->filtered;
  int yi=encoder->h;
  for (;yi-->0;srcrow+=encoder->stride) {
    const uint8_t *srcp=srcrow;
    uint8_t srcmask=0x80;
    int xi=encoder->w;
    for (;xi-->0;) {
      int pixel=((*srcp)&srcmask)?1:0;
      if (srcmask==1) { srcmask=0x80; srcp++; }
      else srcmask>>=1;
      if (pixel!=runcolor) {
        if (rlead_emit_run(encoder,runlen)<0) return -1;
        runcolor=pixel;
        runlen=1;
      } else {
        runlen++;
      }
    }
  }
  if (runlen) {
    if (rlead_emit_run(encoder,runlen)<0) return -1;
  }
  if (encoder->dstmask!=0x80) {
    if (sr_encode_u8(encoder->dst,encoder->dstbuf)<0) return -1;
  }
  
  return 0;
}

/* Count the 0 and 255 bytes in a buffer.
 * ie a quick-ish way of estimating how run-length encodable this will be.
 */
 
static int rlead_count_flushes(const uint8_t *v,int c) {
  int flushc=0;
  for (;c-->0;v++) if ((*v==0x00)||(*v==0xff)) flushc++;
  return flushc;
}

/* Copy and filter image into encoder.
 */
 
static int rlead_encoder_acquire_image(struct rlead_encoder *encoder,const uint8_t *src,int w,int h,int stride) {
  if ((w<1)||(w>0x7fff)) return -1;
  if ((h<1)||(h>0x7fff)) return -1;
  if (stride<(w+7)>>3) return -1;
  encoder->stride=stride;
  encoder->pxc=encoder->stride*h;
  if (!(encoder->filtered=malloc(encoder->pxc))) return -1;
  memcpy(encoder->filtered,src,encoder->pxc);
  encoder->w=w;
  encoder->h=h;
  
  int flushc_before=rlead_count_flushes(encoder->filtered,encoder->pxc);
  uint8_t *dstp=encoder->filtered+encoder->pxc;
  uint8_t *srcp=dstp-encoder->stride;
  int i=encoder->stride*(h-1);
  while (i-->0) {
    dstp--;
    srcp--;
    (*dstp)^=(*srcp);
  }
  int flushc_after=rlead_count_flushes(encoder->filtered,encoder->pxc);
  
  if (flushc_before>flushc_after) {
    srcp=encoder->filtered;
    dstp=srcp+encoder->stride;
    i=encoder->stride*(h-1);
    while (i-->0) {
      (*dstp)^=(*srcp);
      dstp++;
      srcp++;
    }
  } else {
    encoder->using_filter=1;
  }
  
  return 0;
}

/* Encode.
 */
 
int rlead_encode(struct sr_encoder *dst,struct image *image) {
  if (!dst||!image) return -1;
  if (image_reformat_in_place(image,1,0,0,0,0)<0) return -1;
  struct rlead_encoder encoder={.dst=dst,.image=image};
  if (
    (rlead_encoder_acquire_image(&encoder,image->v,image->w,image->h,image->stride)<0)||
    (rlead_encode_inner(&encoder)<0)
  ) {
    rlead_encoder_cleanup(&encoder);
    return -1;
  }
  rlead_encoder_cleanup(&encoder);
  return 0;
}
