#include "image.h"
#include "opt/serial/serial.h"
#include <zlib.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <stdint.h>

/* The Paeth predictor.
 */
 
static uint8_t paeth(uint8_t a,uint8_t b,uint8_t c) {
  int p=a+b-c;
  int pa=p-a; if (pa<0) pa=-pa;
  int pb=p-b; if (pb<0) pb=-pb;
  int pc=p-c; if (pc<0) pc=-pc;
  if ((pa<=pb)&&(pa<=pc)) return a;
  if (pb<=pc) return b;
  return c;
}

/* Decode context.
 */
 
struct png_decoder {
  const uint8_t *src;
  int srcc;
  struct image *image; // Yoink and clear if you want to keep it. Presence means IHDR was processed.
  int y; // Row that we're waiting for. (>=image->h) means we're finished.
  int xstride; // Bytes column-to-column for filter purposes, minimum 1.
  uint8_t *rowbuf; // 1+image->stride
  z_stream z;
  int zinit;
};

static void png_decoder_cleanup(struct png_decoder *decoder) {
  image_del(decoder->image);
  if (decoder->rowbuf) free(decoder->rowbuf);
  if (decoder->zinit) inflateEnd(&decoder->z);
}

/* Receive IHDR.
 */

static int decode_IHDR(struct png_decoder *decoder,const uint8_t *src,int srcc) {
  if (decoder->image) return -1; // Already got one. Two is an error.
  if (srcc<13) return -1;
  int w=(src[0]<<24)|(src[1]<<16)|(src[2]<<8)|src[3];
  int h=(src[4]<<24)|(src[5]<<16)|(src[6]<<8)|src[7];
  if ((w<1)||(w>0x7fff)) return -1; // Enforce struct image's stricter size limits early.
  if ((h<1)||(h>0x7fff)) return -1;
  uint8_t depth=src[8];
  uint8_t colortype=src[9];
  uint8_t compression=src[10];
  uint8_t filter=src[11];
  uint8_t interlace=src[12];
  if (compression||filter||interlace) {
    // (interlace==1) is legal per spec, but I'm not going to implement it. And the other two can only be zero.
    return -2;
  }
  int chanc;
  switch (colortype) {
    case 0: chanc=1; break;
    case 2: chanc=3; break;
    case 3: chanc=1; break;
    case 4: chanc=2; break;
    case 6: chanc=4; break;
    default: return -1;
  }
  int pixelsize=chanc*depth;
  if (pixelsize<8) {
    if (pixelsize<1) return -1;
    if (8%pixelsize) return -1;
  } else {
    if (pixelsize>64) return -1;
    if (pixelsize&7) return -1;
  }
  decoder->xstride=pixelsize>>3;
  if (decoder->xstride<1) decoder->xstride=1;
  if (!(decoder->image=image_new_alloc(pixelsize,w,h))) return -1;
  if (!(decoder->rowbuf=malloc(1+decoder->image->stride))) return -1;
  if (inflateInit(&decoder->z)<0) return -1;
  decoder->zinit=1;
  decoder->z.next_out=(Bytef*)decoder->rowbuf;
  decoder->z.avail_out=1+decoder->image->stride;
  return 0;
}

/* Unfilter, by algorithm.
 * (prv) may be null.
 */
 
static void png_unfilter_SUB(uint8_t *dst,const uint8_t *src,const uint8_t *prv,int stride,int xstride) {
  memcpy(dst,src,xstride);
  const uint8_t *left=dst;
  dst+=xstride;
  src+=xstride;
  int i=stride-xstride;
  for (;i-->0;dst++,src++,left++) *dst=(*src)+(*left);
}

static void png_unfilter_UP(uint8_t *dst,const uint8_t *src,const uint8_t *prv,int stride,int xstride) {
  if (!prv) {
    memcpy(dst,src,stride);
  } else {
    for (;stride-->0;dst++,src++,prv++) *dst=(*src)+(*prv);
  }
}

static void png_unfilter_AVG(uint8_t *dst,const uint8_t *src,const uint8_t *prv,int stride,int xstride) {
  if (!prv) {
    // what an odd choice of filter...
    memcpy(dst,src,xstride);
    const uint8_t *left=dst;
    dst+=xstride;
    src+=xstride;
    int i=stride-xstride;
    for (;i-->0;dst++,src++,left++) *dst=(*src)+((*left)>>1);
  } else {
    const uint8_t *left=dst;
    int i=xstride;
    for (;i-->0;dst++,src++,prv++) *dst=(*src)+((*prv)>>1);
    for (i=stride-xstride;i-->0;dst++,src++,prv++,left++) *dst=(*src)+(((*left)+(*prv))>>1);
  }
}

static void png_unfilter_PAETH(uint8_t *dst,const uint8_t *src,const uint8_t *prv,int stride,int xstride) {
  if (!prv) {
    png_unfilter_SUB(dst,src,prv,stride,xstride);
  } else {
    const uint8_t *left=dst;
    int i=xstride;
    for (;i-->0;dst++,src++,prv++) *dst=(*src)+(*prv);
    for (i=stride-xstride;i-->0;dst++,src++,prv++,left++) *dst=(*src)+paeth(*left,*prv,prv[-xstride]);
  }
}

/* Receive filtered pixels from (decoder->rowbuf).
 */
 
static int png_decoder_finish_row(struct png_decoder *decoder) {
  if (decoder->y<decoder->image->h) {
    uint8_t *dst=decoder->image->v;
    dst+=decoder->image->stride*decoder->y;
    const uint8_t *prv=0;
    if (decoder->y) prv=dst-decoder->image->stride;
    switch (decoder->rowbuf[0]) {
      case 0: memcpy(dst,decoder->rowbuf+1,decoder->image->stride); break;
      case 1: png_unfilter_SUB(dst,decoder->rowbuf+1,prv,decoder->image->stride,decoder->xstride); break;
      case 2: png_unfilter_UP(dst,decoder->rowbuf+1,prv,decoder->image->stride,decoder->xstride); break;
      case 3: png_unfilter_AVG(dst,decoder->rowbuf+1,prv,decoder->image->stride,decoder->xstride); break;
      case 4: png_unfilter_PAETH(dst,decoder->rowbuf+1,prv,decoder->image->stride,decoder->xstride); break;
      default: return -1;
    }
    decoder->y++;
  }
  return 0;
}

/* Receive IDAT.
 */
 
static int decode_IDAT(struct png_decoder *decoder,const uint8_t *src,int srcc) {
  if (!decoder->zinit) return -1; // IDAT before IHDR, perhaps?
  decoder->z.next_in=(Bytef*)src;
  decoder->z.avail_in=srcc;
  while (decoder->z.avail_in) {
  
    if (!decoder->z.avail_out) {
      if (png_decoder_finish_row(decoder)<0) return -1;
      decoder->z.next_out=(Bytef*)decoder->rowbuf;
      decoder->z.avail_out=1+decoder->image->stride;
    }
    
    int err=inflate(&decoder->z,Z_NO_FLUSH);
    if (err<0) return -1;
  }
  return 0;
}

/* Finalize decode. Verify we got all the things.
 */
 
static int decode_finish(struct png_decoder *decoder) {
  if (!decoder->image) return -1;
  
  decoder->z.next_in=0;
  decoder->z.avail_in=0;
  for (;;) {
    if (!decoder->z.avail_out) {
      if (png_decoder_finish_row(decoder)<0) return -1;
      decoder->z.next_out=(Bytef*)decoder->rowbuf;
      decoder->z.avail_out=1+decoder->image->stride;
    }
    int err=inflate(&decoder->z,Z_FINISH);
    if (err<0) return -1;
    if (err==Z_STREAM_END) {
      if (!decoder->z.avail_out) {
        png_decoder_finish_row(decoder);
      }
      break;
    }
  }
  
  if (decoder->y<decoder->image->h) return -1;
  return 0;
}

/* Decode in context.
 * Caller validates signature, we do all the rest.
 */
 
static int image_decode_png_inner(struct png_decoder *decoder) {
  int srcp=8;
  int stopp=decoder->srcc-8;
  while (srcp<=stopp) {
    int chunklen=decoder->src[srcp++]<<24;
    chunklen|=decoder->src[srcp++]<<16;
    chunklen|=decoder->src[srcp++]<<8;
    chunklen|=decoder->src[srcp++];
    const uint8_t *chunkid=decoder->src+srcp;
    srcp+=4;
    const uint8_t *payload=decoder->src+srcp;
    srcp+=chunklen;
    srcp+=4; // Don't bother validating CRC.
    if (srcp>decoder->srcc) return -1;
    int err=0;
    if (!memcmp(chunkid,"IHDR",4)) err=decode_IHDR(decoder,payload,chunklen);
    else if (!memcmp(chunkid,"IDAT",4)) err=decode_IDAT(decoder,payload,chunklen);
    if (err<0) return err;
  }
  return decode_finish(decoder);
}

/* Zlib malloc wrappers, necessary because we compile with Z_SOLO.
 */

static voidpf zalloc(voidpf userdata,uInt items,uInt size) {
  return calloc(items,size);
}

static void zfree(voidpf userdata,voidpf address) {
  free(address);
}

/* Decode PNG, main entry point.
 */
  
struct image *png_decode(const void *_src,int srcc) {
  const uint8_t *src=_src;
  if ((srcc<8)||memcmp(src,"\x89PNG\r\n\x1a\n",8)) return 0;
  struct png_decoder decoder={
    .src=src,
    .srcc=srcc,
    .z={
      .zalloc=zalloc,
      .zfree=zfree,
    },
  };
  int err=image_decode_png_inner(&decoder);
  if (err<0) {
    png_decoder_cleanup(&decoder);
    return 0;
  }
  struct image *image=decoder.image;
  decoder.image=0;
  png_decoder_cleanup(&decoder);
  return image;
}

/* Encoder context.
 */
 
struct png_encoder {
  struct sr_encoder *dst; // WEAK
  struct image *image; // WEAK
  uint8_t *rowbuf;
  int rowbufc; // including filter byte, ie 1+stride
  int xstride; // bytes column to column for filter purposes (min 1)
  int stride; // May be less than input.
  z_stream *z; // Initialized if not null.
};

static void png_encoder_cleanup(struct png_encoder *ctx) {
  if (ctx->rowbuf) free(ctx->rowbuf);
  if (ctx->z) {
    deflateEnd(ctx->z);
    free(ctx->z);
  }
}

/* Add one chunk to encoder.
 */
 
static int png_encode_chunk(struct png_encoder *ctx,const char chunktype[4],const void *v,int c) {
  if (sr_encode_intbe(ctx->dst,c,4)<0) return -1;
  if (sr_encode_raw(ctx->dst,chunktype,4)<0) return -1;
  if (sr_encode_raw(ctx->dst,v,c)<0) return -1;
  uint32_t crc=crc32(crc32(0,0,0),(Bytef*)chunktype,4);
  if (c) crc=crc32(crc,v,c); // evidently crc32() doesn't like empty input
  if (sr_encode_intbe(ctx->dst,crc,4)<0) return -1;
  return 0;
}

/* Encode IHDR.
 */
 
static int png_encode_IHDR(struct png_encoder *ctx) {
  uint8_t depth,colortype;
  switch (ctx->image->pixelsize) {
    case 1: case 2: case 4: case 8: depth=ctx->image->pixelsize; colortype=0; break;
    case 24: depth=8; colortype=2; break;
    case 32: depth=8; colortype=6; break;
    default: return -1;
  }
  uint8_t tmp[13]={
    ctx->image->w>>24,
    ctx->image->w>>16,
    ctx->image->w>>8,
    ctx->image->w,
    ctx->image->h>>24,
    ctx->image->h>>16,
    ctx->image->h>>8,
    ctx->image->h,
    depth,
    colortype,
    0,0,0, // Compression, Filter, Interlace
  };
  return png_encode_chunk(ctx,"IHDR",tmp,sizeof(tmp));
}

/* Filters.
 * NONE and SUB accept null (prv), the others require it.
 * (they do declare a prv parameter, just for consistency).
 * Each returns the count of zeroes after filtering.
 */
 
static inline int png_count_zeroes(const uint8_t *v,int c) {
  int zeroc=0;
  for (;c-->0;v++) if (!*v) zeroc++;
  return zeroc;
}
 
static int png_filter_row_NONE(uint8_t *dst,const uint8_t *src,const uint8_t *prv,int c,int xstride) {
  memcpy(dst,src,c);
  return png_count_zeroes(dst,c);
}
 
static int png_filter_row_SUB(uint8_t *dst,const uint8_t *src,const uint8_t *prv,int c,int xstride) {
  const uint8_t *dst0=dst;
  int c0=c;
  memcpy(dst,src,xstride);
  c-=xstride;
  dst+=xstride;
  const uint8_t *tail=src;
  src+=xstride;
  for (;c-->0;dst++,src++,tail++) {
    *dst=(*src)-(*tail);
  }
  return png_count_zeroes(dst0,c0);
}

static int png_filter_row_UP(uint8_t *dst,const uint8_t *src,const uint8_t *prv,int c,int xstride) {
  const uint8_t *dst0=dst;
  int c0=c;
  for (;c-->0;dst++,src++,prv++) {
    *dst=(*src)-(*prv);
  }
  return png_count_zeroes(dst0,c0);
}

static int png_filter_row_AVG(uint8_t *dst,const uint8_t *src,const uint8_t *prv,int c,int xstride) {
  const uint8_t *dst0=dst;
  int c0=c;
  const uint8_t *tail=src;
  int i=0; for (;i<xstride;i++,dst++,src++,prv++) {
    *dst=(*src)-((*prv)>>1);
  }
  c-=xstride;
  for (;c-->0;dst++,src++,prv++,tail++) {
    *dst=(*src)-(((*tail)+(*prv))>>1);
  }
  return png_count_zeroes(dst0,c0);
}

static inline uint8_t png_paeth(uint8_t a,uint8_t b,uint8_t c) {
  int p=a+b-c;
  int pa=p-a; if (pa<0) pa=-pa;
  int pb=p-b; if (pb<0) pb=-pb;
  int pc=p-c; if (pc<0) pc=-pc;
  if ((pa<=pb)&&(pa<=pc)) return a;
  if (pb<=pc) return b;
  return c;
}

static int png_filter_row_PAETH(uint8_t *dst,const uint8_t *src,const uint8_t *prv,int c,int xstride) {
  const uint8_t *dst0=dst;
  int c0=c;
  const uint8_t *srctail=src,*prvtail=prv;
  int i=0; for (;i<xstride;i++,dst++,src++,prv++) {
    *dst=(*src)-(*prv);
  }
  c-=xstride;
  for (;c-->0;dst++,src++,prv++,srctail++,prvtail++) {
    *dst=(*src)-png_paeth(*srctail,*prv,*prvtail);
  }
  return png_count_zeroes(dst0,c0);
}

/* Apply filter to one row.
 * Return the chosen filter byte and overwrite (dst) completely.
 */
 
static uint8_t png_filter_row(uint8_t *dst,const uint8_t *src,const uint8_t *prv,int c,int xstride) {
  if (!prv) {
    // All filters are legal for the first row, but only NONE and SUB make sense.
    // (PAETH turns into SUB, UP turns into NONE, and AVG I'm not sure).
    int nonescore=png_filter_row_NONE(dst,src,0,c,xstride);
    int subscore=png_filter_row_SUB(dst,src,0,c,xstride);
    if (subscore>=nonescore) { // SUB wins ties, since it's the last one and already present in buffer.
      return 1;
    }
    memcpy(dst,src,c);
    return 0;
  }
  // Normal case: Try all five filters, and keep whichever produces the most zeroes.
  int nonescore=png_filter_row_NONE(dst,src,prv,c,xstride);
  int subscore=png_filter_row_SUB(dst,src,prv,c,xstride);
  int upscore=png_filter_row_UP(dst,src,prv,c,xstride);
  int avgscore=png_filter_row_AVG(dst,src,prv,c,xstride);
  int paethscore=png_filter_row_PAETH(dst,src,prv,c,xstride);
  // Ties break in order of convenience: PAETH, NONE, SUB, UP, AVG
  if ((paethscore>=avgscore)&&(paethscore>=upscore)&&(paethscore>=subscore)&&(paethscore>=nonescore)) {
    return 4;
  }
  if ((nonescore>=avgscore)&&(nonescore>=upscore)&&(nonescore>=subscore)) {
    memcpy(dst,src,c);
    return 0;
  }
  if ((subscore>=avgscore)&&(subscore>=upscore)) {
    png_filter_row_SUB(dst,src,prv,c,xstride);
    return 1;
  }
  if (upscore>=avgscore) {
    png_filter_row_UP(dst,src,prv,c,xstride);
    return 2;
  }
  png_filter_row_AVG(dst,src,prv,c,xstride);
  return 3;
}

/* Allocate more space in master output if needed, and point zlib to it.
 */
 
static int png_encoder_require_zlib_output(struct png_encoder *ctx) {
  if (sr_encoder_require(ctx->dst,8192)<0) return -1;
  ctx->z->next_out=(Bytef*)ctx->dst->v+ctx->dst->c;
  ctx->z->avail_out=ctx->dst->a-ctx->dst->c;
  return 0;
}

/* Feed (rowbuf) into zlib, and advance output as needed.
 */
 
static int png_encode_row(struct png_encoder *ctx) {
  ctx->z->next_in=(Bytef*)ctx->rowbuf;
  ctx->z->avail_in=ctx->rowbufc;
  while (ctx->z->avail_in) {
    if (png_encoder_require_zlib_output(ctx)<0) return -1;
    int out0=ctx->z->total_out;
    int err=deflate(ctx->z,Z_NO_FLUSH);
    if (err<0) return -1;
    int outadd=ctx->z->total_out-out0;
    ctx->dst->c+=outadd;
  }
  return 0;
}

/* Flush zlib out.
 */
 
static int png_encode_end_of_image(struct png_encoder *ctx) {
  while (1) {
    if (png_encoder_require_zlib_output(ctx)<0) return -1;
    int out0=ctx->z->total_out;
    int err=deflate(ctx->z,Z_FINISH);
    if (err<0) return -1;
    int outadd=ctx->z->total_out-out0;
    ctx->dst->c+=outadd;
    if (err=Z_STREAM_END) break;
  }
  return 0;
}

/* Encode IDAT, the main event.
 * We're not going to use png_encode_chunk. We'll build up the chunk incrementally, then calculate CRC on our own.
 */
 
static int png_encode_IDAT(struct png_encoder *ctx) {
  int lenp=ctx->dst->c;
  if (sr_encode_raw(ctx->dst,"\0\0\0\0IDAT",8)<0) return -1;
  
  ctx->rowbufc=1+ctx->stride;
  if (!(ctx->rowbuf=malloc(ctx->rowbufc))) return -1;
  ctx->xstride=(ctx->image->pixelsize+7)>>3;
  
  if (!(ctx->z=calloc(1,sizeof(z_stream)))) return -1;
  ctx->z->zalloc=zalloc;
  ctx->z->zfree=zfree;
  if (deflateInit(ctx->z,Z_BEST_COMPRESSION)<0) {
    free(ctx->z);
    ctx->z=0;
    return -1;
  }
  
  const uint8_t *pvrow=0;
  const uint8_t *srcrow=ctx->image->v;
  int y=0;
  for (;y<ctx->image->h;y++,srcrow+=ctx->image->stride) {
    ctx->rowbuf[0]=png_filter_row(ctx->rowbuf+1,srcrow,pvrow,ctx->stride,ctx->xstride);
    if (png_encode_row(ctx)<0) return -1;
    pvrow=srcrow;
  }
  if (png_encode_end_of_image(ctx)<0) return -1;
  
  int len=ctx->dst->c-lenp-8;
  int crc=crc32(crc32(0,0,0),((Bytef*)ctx->dst->v)+lenp+4,ctx->dst->c-lenp-4);
  if (sr_encode_intbe(ctx->dst,crc,4)<0) return -1;
  uint8_t *lenv=(uint8_t*)ctx->dst->v+lenp;
  lenv[0]=len>>24;
  lenv[1]=len>>16;
  lenv[2]=len>>8;
  lenv[3]=len;
  return 0;
}

/* Encode in context.
 */
 
static int png_encode_inner(struct png_encoder *ctx) {
  // We could allow (1,2,4,8,24,32) for pixelsize. For now forcing everything to RGBA.
  if (image_force_rgba(ctx->image)<0) return -1;
  ctx->stride=ctx->image->stride;
  if (sr_encode_raw(ctx->dst,"\x89PNG\r\n\x1a\n",8)<0) return -1;
  if (png_encode_IHDR(ctx)<0) return -1;
  if (png_encode_IDAT(ctx)<0) return -1;
  if (png_encode_chunk(ctx,"IEND",0,0)<0) return -1;
  return 0;
}

/* Encode.
 */

int png_encode(struct sr_encoder *dst,struct image *image) {
  if (!dst||!image) return -1;
  struct png_encoder ctx={
    .dst=dst,
    .image=image,
  };
  int err=png_encode_inner(&ctx);
  png_encoder_cleanup(&ctx);
  return err;
}
