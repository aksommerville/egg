#include "png.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>
#include <zlib.h>

/* Encoder context.
 */
 
struct png_encoder {
  const struct png_image *image;
  uint8_t *dst;
  int dstc,dsta;
  uint8_t *rowbuf;
  int rowbufc;
  z_stream z;
  int zinit;
  int pixelsize; // bits
  int rowlen; // output stride in bytes (image->stride is allowed to overshoot)
};

static void png_encoder_cleanup(struct png_encoder *encoder) {
  if (encoder->dst) free(encoder->dst);
  if (encoder->rowbuf) free(encoder->rowbuf);
  if (encoder->zinit) deflateEnd(&encoder->z);
}

/* Add to output.
 */
 
static int png_encoder_require(struct png_encoder *encoder,int addc) {
  if (addc<1) return 0;
  if (encoder->dstc>INT_MAX-addc) return -1;
  int na=encoder->dstc+addc;
  if (na<=encoder->dsta) return 0;
  if (na<INT_MAX-1024) na=(na+1024)&~1023;
  void *nv=realloc(encoder->dst,na);
  if (!nv) return -1;
  encoder->dst=nv;
  encoder->dsta=na;
  return 0;
}

static int png_encoder_append(struct png_encoder *encoder,const void *src,int srcc) {
  if (!src||(srcc<0)) return -1;
  if (png_encoder_require(encoder,srcc)<0) return -1;
  memcpy(encoder->dst+encoder->dstc,src,srcc);
  encoder->dstc+=srcc;
  return 0;
}

/* Calculate and encode the CRC for a chunk at the end of output.
 * Provide the chunk's payload length; there must be a length and type before that.
 */
 
static int png_encode_crc(struct png_encoder *encoder,int paylen) {
  if ((paylen<0)||(paylen>encoder->dstc-4)) return -1;
  uint32_t crc=crc32(crc32(0,0,0),encoder->dst+encoder->dstc-paylen-4,paylen+4);
  uint8_t tmp[4]={crc>>24,crc>>16,crc>>8,crc};
  return png_encoder_append(encoder,tmp,sizeof(tmp));
}

/* IHDR.
 */
 
static int png_encode_IHDR(struct png_encoder *encoder) {
  uint8_t tmp[13]={
    encoder->image->w>>24,
    encoder->image->w>>16,
    encoder->image->w>>8,
    encoder->image->w,
    encoder->image->h>>24,
    encoder->image->h>>16,
    encoder->image->h>>8,
    encoder->image->h,
    encoder->image->depth,
    encoder->image->colortype,
    0,0,0,
  };
  uint8_t pfx[8]={
    sizeof(tmp)>>24,
    sizeof(tmp)>>16,
    sizeof(tmp)>>8,
    sizeof(tmp),
    'I','H','D','R',
  };
  if (png_encoder_append(encoder,pfx,sizeof(pfx))<0) return -1;
  if (png_encoder_append(encoder,tmp,sizeof(tmp))<0) return -1;
  if (png_encode_crc(encoder,sizeof(tmp))<0) return -1;
  return 0;
}

/* Encode all extra chunks.
 */
 
static int png_encode_chunks(struct png_encoder *encoder) {
  const struct png_chunk *chunk=encoder->image->chunkv;
  int i=encoder->image->chunkc;
  for (;i-->0;chunk++) {
    uint8_t pfx[8]={
      chunk->c>>24,
      chunk->c>>16,
      chunk->c>>8,
      chunk->c,
      chunk->chunkid>>24,
      chunk->chunkid>>16,
      chunk->chunkid>>8,
      chunk->chunkid,
    };
    if (png_encoder_append(encoder,pfx,sizeof(pfx))<0) return -1;
    if (png_encoder_append(encoder,chunk->v,chunk->c)<0) return -1;
    if (png_encode_crc(encoder,chunk->c)<0) return -1;
  }
  return 0;
}

/* Apply SUB filter.
 */
 
static void png_filter_row(uint8_t *dst,const uint8_t *src,int xstride,int c) {
  memcpy(dst,src,xstride);
  dst+=xstride;
  src+=xstride;
  c-=xstride;
  for (;c-->0;dst++,src++) {
    *dst=src[0]-src[-xstride];
  }
}

/* Encode IDAT.
 */
 
static int png_encode_IDAT(struct png_encoder *encoder) {

  // Create the row buffer and zlib context.
  if (encoder->rowbuf||encoder->zinit) return -1;
  encoder->rowbufc=1+encoder->rowlen;
  if (!(encoder->rowbuf=malloc(encoder->rowbufc))) return -1;
  if (deflateInit(&encoder->z,Z_BEST_COMPRESSION)<0) return -1;
  encoder->zinit=1;
  int xstride=(encoder->pixelsize+7)>>3;
  
  // Emit chunk header with a dummy length.
  int lendstp=encoder->dstc;
  uint8_t prefix[8]={0,0,0,0,'I','D','A','T'};
  if (png_encoder_append(encoder,prefix,sizeof(prefix))<0) return -1;
  int payloaddstp=encoder->dstc;
  
  // Filter and compress each row.
  // To keep things simple, I'm only using the SUB filter.
  // A better encoder would try all five filters for each row, and use some heuristic to pick one (eg most zeroes).
  int yi=encoder->image->h;
  const uint8_t *row=encoder->image->pixels;
  for (;yi-->0;row+=encoder->image->stride) {
    encoder->rowbuf[0]=1; // SUB
    png_filter_row(encoder->rowbuf+1,row,xstride,encoder->rowlen);
    encoder->z.next_in=(Bytef*)encoder->rowbuf;
    encoder->z.avail_in=encoder->rowbufc;
    while (encoder->z.avail_in) {
      if (png_encoder_require(encoder,1)<0) return -1;
      encoder->z.next_out=(Bytef*)(encoder->dst+encoder->dstc);
      encoder->z.avail_out=encoder->dsta-encoder->dstc;
      int ao0=encoder->z.avail_out;
      int err=deflate(&encoder->z,Z_NO_FLUSH);
      if (err<0) return -1;
      int addc=ao0-encoder->z.avail_out;
      encoder->dstc+=addc;
    }
  }
  
  // Finish compression, continue flushing output.
  while (1) {
    if (png_encoder_require(encoder,1)<0) return -1;
    encoder->z.next_out=(Bytef*)(encoder->dst+encoder->dstc);
    encoder->z.avail_out=encoder->dsta-encoder->dstc;
    int ao0=encoder->z.avail_out;
    int err=deflate(&encoder->z,Z_FINISH);
    if (err<0) return -1;
    int addc=ao0-encoder->z.avail_out;
    encoder->dstc+=addc;
    if (err==Z_STREAM_END) break;
  }
  
  // Fill in length and add CRC.
  int paylen=encoder->dstc-payloaddstp;
  if (encoder->dstc<lendstp+4) return -1;
  encoder->dst[lendstp+0]=paylen>>24;
  encoder->dst[lendstp+1]=paylen>>16;
  encoder->dst[lendstp+2]=paylen>>8;
  encoder->dst[lendstp+3]=paylen;
  if (png_encode_crc(encoder,paylen)<0) return -1;
  return 0;
}

/* Encode in context.
 */
 
static int png_encode_inner(struct png_encoder *encoder) {

  encoder->pixelsize=encoder->image->depth*png_channel_count_for_colortype(encoder->image->colortype);
  if (encoder->pixelsize<1) return -1;
  encoder->rowlen=(encoder->image->w*encoder->pixelsize+7)>>3;
  if (encoder->image->stride<encoder->rowlen) return -1;
  
  if (png_encoder_append(encoder,"\x89PNG\r\n\x1a\n",8)<0) return -1;
  if (png_encode_IHDR(encoder)<0) return -1;
  if (png_encode_chunks(encoder)<0) return -1;
  if (png_encode_IDAT(encoder)<0) return -1;
  if (png_encoder_append(encoder,"\0\0\0\0IEND\xae\x42\x60\x82",12)<0) return -1;
  return 0;
}

/* Encode image to PNG, main entry point.
 */
 
int png_encode(void *dstpp,const struct png_image *image) {
  if (!dstpp||!image) return -1;
  if ((image->w<1)||(image->h<1)) return -1;
  if (!image->pixels) return -1;
  struct png_encoder encoder={
    .image=image,
  };
  if (png_encode_inner(&encoder)<0) {
    png_encoder_cleanup(&encoder);
    return -1;
  }
  int dstc=encoder.dstc;
  *(void**)dstpp=encoder.dst;
  encoder.dst=0;
  png_encoder_cleanup(&encoder);
  return dstc;
}
