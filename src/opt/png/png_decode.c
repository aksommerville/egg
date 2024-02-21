#include "png.h"
#include <zlib.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>

/* Convenience, decode in one shot.
 */
 
struct png_image *png_decode(const void *src,int srcc) {
  if (!src||(srcc<0)) return 0;
  struct png_decoder *decoder=png_decoder_new();
  if (!decoder) return 0;
  if (png_decode_more(decoder,src,srcc)<0) {
    png_decoder_del(decoder);
    return 0;
  }
  struct png_image *image=png_decode_finish(decoder);
  png_decoder_del(decoder);
  return image;
}

/* Decoder definition.
 */
 
#define PNG_DECODER_EXPECT_SIGNATURE 0 /* Waiting for 8-byte file signature. */
#define PNG_DECODER_EXPECT_HEADER    1 /* Waiting for 8-byte chunk header. */
#define PNG_DECODER_EXPECT_CHUNK     2 /* Not IDAT, but all other chunks, we queue up the entire body before processing. */
#define PNG_DECODER_EXPECT_CRC       3 /* Waiting for CRC, in order to ignore it. */
#define PNG_DECODER_EXPECT_IDAT      4 /* We don't wait for the whole chunk to arrive, before delivering to zlib. */
#define PNG_DECODER_EXPECT_FINISHED  5 /* Got IEND, ignore all further input. */
#define PNG_DECODER_EXPECT_FATAL     6 /* Can't continue. */

struct png_decoder {
  z_stream z;
  int zinit;
  struct png_image *image; // Exists after reading IHDR, until the user finishes decode.
  
  // Generic input reception.
  // My design is that if the caller supplies the entire file in the first update, we will never allocate (expectv).
  int expect_type;
  uint8_t *expectv;
  int expectp; // 0..expectc
  int expectc; // expected length; we don't necessarily have it yet
  int expecta; // ***may be less than expectc*** but always >= expectp.
  int idat_pending; // How much of the IDAT payload remaining? Could use expectp/expectc for this, but that feels risky.
  uint32_t chunkid; // During CHUNK mode.
  
  // Extra state for receiving pixels.
  uint8_t *rowbuf;
  int rowbufc;
  int y;
  int xstride; // bytes column to column, for filter purposes
};

/* Delete.
 */

void png_decoder_del(struct png_decoder *decoder) {
  if (!decoder) return;
  
  if (decoder->zinit) inflateEnd(&decoder->z);
  if (decoder->expectv) free(decoder->expectv);
  if (decoder->image) png_image_del(decoder->image);
  if (decoder->rowbuf) free(decoder->rowbuf);
  
  free(decoder);
}

/* Ensure that we have enough (expectv) to fill it up to (expectc).
 * We DO NOT do this on setting (expectc), because the data might already be ready in the input buffer.
 */
 
static int png_decoder_require_expectv(struct png_decoder *decoder) {
  if (decoder->expectc<1) return -1;
  if (decoder->expectc<=decoder->expecta) return 0;
  int na=decoder->expectc;
  if (na<INT_MAX-1024) na=(na+1024)&~1023;
  void *nv=realloc(decoder->expectv,na);
  if (!nv) return -1;
  decoder->expectv=nv;
  decoder->expecta=na;
  return 0;
}

/* New.
 */
 
struct png_decoder *png_decoder_new() {
  struct png_decoder *decoder=calloc(1,sizeof(struct png_decoder));
  if (!decoder) return 0;
  
  decoder->expect_type=PNG_DECODER_EXPECT_SIGNATURE;
  decoder->expectc=8;
  
  return decoder;
}

/* Apply whatever's in (rowbuf) as the next row of the final image.
 */
 
static inline uint8_t png_paeth(uint8_t a,uint8_t b,uint8_t c) {
  int p=a+b-c;
  int pa=a-p; if (pa<0) pa=-pa;
  int pb=b-p; if (pb<0) pb=-pb;
  int pc=c-p; if (pc<0) pc=-pc;
  if ((pa<=pb)&&(pa<=pc)) return a;
  if (pb<=pc) return b;
  return c;
}
 
static int png_decode_row(struct png_decoder *decoder) {
  if (decoder->y>=decoder->image->h) return 0; // Extra data, ignore it, whatever.
  uint8_t *dst=((uint8_t*)decoder->image->pixels)+decoder->y*decoder->image->stride;
  const uint8_t *pv=0;
  if (decoder->y) pv=dst-decoder->image->stride;
  const uint8_t *src=decoder->rowbuf+1;
  int len=decoder->image->stride;
  switch (decoder->rowbuf[0]) {
    case 0: {
        memcpy(dst,src,len);
      } break;
    case 1: {
        memcpy(dst,src,decoder->xstride);
        int i=decoder->xstride; for (;i<len;i++) {
          dst[i]=src[i]+dst[i-decoder->xstride];
        }
      } break;
    case 2: if (pv) {
        int i=len; for (;i-->0;dst++,src++,pv++) {
          *dst=(*src)+(*pv);
        }
      } else {
        memcpy(dst,src,len);
      } break;
    case 3: if (pv) {
        int i=0;
        for (;i<decoder->xstride;i++) dst[i]=src[i]+(pv[i]>>1);
        for (;i<len;i++) dst[i]=src[i]+((pv[i]+dst[i-decoder->xstride])>>1);
      } else {
        int i=0;
        for (;i<decoder->xstride;i++) dst[i]=src[i];
        for (;i<len;i++) dst[i]=src[i]+(dst[i-decoder->xstride]>>1);
      } break;
    case 4: if (pv) {
        int i=0;
        for (;i<decoder->xstride;i++) dst[i]=src[i]+pv[i];
        for (;i<len;i++) dst[i]=src[i]+png_paeth(dst[i-decoder->xstride],pv[i],pv[i-decoder->xstride]);
      } else {
        int i=0;
        for (;i<decoder->xstride;i++) dst[i]=src[i];
        for (;i<len;i++) dst[i]=src[i]+dst[i-decoder->xstride];
      } break;
    default: return -1;
  }
  decoder->y++;
  return 0;
}

/* Receive some portion of the IDAT payload, pass it through zlib, process zlib output.
 */
 
static int png_decode_IDAT(struct png_decoder *decoder,const void *src,int srcc) {
  if (!decoder->image) return -1; // eg IDAT before IHDR, that would be a problem.
  if (!decoder->rowbuf) return -1;
  decoder->z.next_in=(Bytef*)src;
  decoder->z.avail_in=srcc;
  while (decoder->z.avail_in) {
    if (!decoder->z.avail_out) {
      if (png_decode_row(decoder)<0) return -1;
      decoder->z.next_out=(Bytef*)decoder->rowbuf;
      decoder->z.avail_out=decoder->rowbufc;
    }
    int err=inflate(&decoder->z,Z_NO_FLUSH);
    if (err<0) return -1;
  }
  return 0;
}

static int png_flush_zlib(struct png_decoder *decoder) {
  int finished=0;
  while (1) {
    if (!decoder->z.avail_out) {
      if (png_decode_row(decoder)<0) return -1;
      decoder->z.next_out=(Bytef*)decoder->rowbuf;
      decoder->z.avail_out=decoder->rowbufc;
    }
    if (finished) return 0;
    int err=inflate(&decoder->z,Z_FINISH);
    if (err<0) return -1;
    if (err==Z_STREAM_END) finished=1;
  }
}

/* Receive IHDR.
 */
 
static int png_decode_IHDR(struct png_decoder *decoder,const uint8_t *src,int srcc) {
  if (decoder->image) return -1; // already got one!
  if (decoder->zinit) return -1;
  if (srcc<13) return -1;
  
  int w=(src[0]<<24)|(src[1]<<16)|(src[2]<<8)|src[3];
  int h=(src[4]<<24)|(src[5]<<16)|(src[6]<<8)|src[7];
  int depth=src[8];
  int colortype=src[9];
  if (src[10]||src[11]||src[12]) {
    // Interlaced, or some new compression or filter mode. Reject.
    return -1;
  }
  if ((w<1)||(h<1)) return -1; // there will also be a maximum enforced at png_image_new()
  
  if (!(decoder->image=png_image_new(w,h,depth,colortype))) return -1;
  
  decoder->rowbufc=1+decoder->image->stride;
  if (!(decoder->rowbuf=malloc(decoder->rowbufc))) return -1;
  decoder->xstride=(depth*png_channel_count_for_colortype(colortype)+7)>>3;
  decoder->y=0;
  
  if (inflateInit(&decoder->z)<0) return -1;
  decoder->zinit=1;
  decoder->z.next_out=(Bytef*)decoder->rowbuf;
  decoder->z.avail_out=decoder->rowbufc;
  
  return 0;
}

/* Receive some extra chunk, dump it in the image's chunk list.
 */
 
static int png_decode_extra(struct png_decoder *decoder,uint32_t chunkid,const void *src,int srcc) {
  if (!decoder->image) return -1; // null means we didn't get IHDR yet, and IHDR must be first.
  if (png_image_add_chunk(decoder->image,chunkid,src,srcc)<0) return -1;
  return 0;
}

/* Receive what was expected, process it, and set the next expectation.
 * All expectation types except IDAT, FINISHED, and FATAL land here.
 */
 
static int png_decode_bit(struct png_decoder *decoder,const void *src,int srcc) {
  switch (decoder->expect_type) {
    case PNG_DECODER_EXPECT_SIGNATURE: {
        if (srcc!=8) return -1;
        if (memcmp(src,"\x89PNG\r\n\x1a\n",8)) return -1;
        decoder->expectc=8;
        decoder->expect_type=PNG_DECODER_EXPECT_HEADER;
      } return 0;
    case PNG_DECODER_EXPECT_HEADER: {
        if (srcc!=8) return -1;
        const uint8_t *SRC=src;
        int len=(SRC[0]<<24)|(SRC[1]<<16)|(SRC[2]<<8)|SRC[3];
        if (len<0) return -1;
        decoder->chunkid=(SRC[4]<<24)|(SRC[5]<<16)|(SRC[6]<<8)|SRC[7];
        if (!len) {
          goto _end_of_chunk_;
        } else if (decoder->chunkid==PNG_CHUNKID_IDAT) {
          decoder->idat_pending=len;
          decoder->expect_type=PNG_DECODER_EXPECT_IDAT;
        } else {
          decoder->expectc=len;
          decoder->expect_type=PNG_DECODER_EXPECT_CHUNK;
        }
      } return 0;
    case PNG_DECODER_EXPECT_CHUNK: _end_of_chunk_: {
        decoder->expectc=4;
        decoder->expect_type=PNG_DECODER_EXPECT_CRC;
        switch (decoder->chunkid) {
          case PNG_CHUNKID_IHDR: return png_decode_IHDR(decoder,src,srcc);
          case PNG_CHUNKID_IEND: {
              decoder->expectc=0;
              decoder->expect_type=PNG_DECODER_EXPECT_FINISHED;
              if (png_flush_zlib(decoder)<0) return -1;
            } return 0;
          default: return png_decode_extra(decoder,decoder->chunkid,src,srcc);
        }
      }
    case PNG_DECODER_EXPECT_CRC: {
        decoder->expectc=8;
        decoder->expect_type=PNG_DECODER_EXPECT_HEADER;
      } return 0;
  }
  return -1;
}

/* Consume partial input.
 * Return length consumed, must be at least one.
 */
 
static int png_decode_inner(struct png_decoder *decoder,const void *src,int srcc) {
  if (srcc<1) return -1;
  
  // FINISHED and FATAL modes, we can answer without looking.
  if (decoder->expect_type==PNG_DECODER_EXPECT_FINISHED) return srcc;
  if (decoder->expect_type==PNG_DECODER_EXPECT_FATAL) return -1;
  
  // IDAT mode, we skip the buffer and send it right on to zlib.
  if (decoder->expect_type==PNG_DECODER_EXPECT_IDAT) {
    int cpc=decoder->idat_pending;
    if (cpc>srcc) cpc=srcc;
    if (png_decode_IDAT(decoder,src,cpc)<0) return -1;
    decoder->idat_pending-=cpc;
    if (!decoder->idat_pending) {
      decoder->expect_type=PNG_DECODER_EXPECT_CRC;
      decoder->expectc=4;
      decoder->expectp=0;
    }
    return cpc;
  }
  
  // If we have the entire expected chunk in (src), process it right there.
  if ((srcc>=decoder->expectc)&&!decoder->expectp) {
    int cpc=decoder->expectc;
    decoder->expectc=0; // decode_bit must reset these; force an error if it doesn't
    decoder->expectp=0;
    if (png_decode_bit(decoder,src,cpc)<0) return -1;
    return cpc;
  }
  
  // All other modes, we gather some amount of data into the buffer if needed.
  if (png_decoder_require_expectv(decoder)<0) return -1;
  if (decoder->expectp>=decoder->expectc) return -1;
  int cpc=decoder->expectc-decoder->expectp;
  if (cpc>srcc) cpc=srcc;
  memcpy(decoder->expectv+decoder->expectp,src,cpc);
  decoder->expectp+=cpc;
  if (decoder->expectp>=decoder->expectc) {
    int bitlen=decoder->expectc;
    decoder->expectc=0;
    decoder->expectp=0;
    if (png_decode_bit(decoder,decoder->expectv,bitlen)<0) return -1;
  }
  return cpc;
}

/* Consume input, public entry point..
 */

int png_decode_more(struct png_decoder *decoder,const void *src,int srcc) {
  if (!decoder) return -1;
  if (!src||(srcc<1)) return 0;
  while (srcc>0) {
    int err=png_decode_inner(decoder,src,srcc);
    if (err<=0) {
      decoder->expect_type=PNG_DECODER_EXPECT_FATAL;
      return -1;
    }
    src=(char*)src+err;
    srcc-=err;
  }
  return 0;
}

/* Finish processing and produce image.
 */
 
struct png_image *png_decode_finish(struct png_decoder *decoder) {
  if (!decoder) return 0;
  if (decoder->expect_type==PNG_DECODER_EXPECT_FATAL) return 0;
  // Could check for FINISHED mode, or look deeper at the pixel receiving state, but whatever.
  // If (image) exists, it is at least technically valid, pixels are allocated and all.
  struct png_image *image=decoder->image;
  decoder->image=0;
  return image;
}
