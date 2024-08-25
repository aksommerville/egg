#include "image.h"
#include "opt/serial/serial.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>

/* Delete image.
 */

void image_del(struct image *image) {
  if (!image) return;
  if (image->v) free(image->v);
  free(image);
}

/* Decode, any format.
 */

struct image *image_decode(const void *src,int srcc) {
  if (!src||(srcc<1)) return 0;
  switch (image_format_guess(src,srcc)) {
    #define _(tag) case IMAGE_FORMAT_##tag: return tag##_decode(src,srcc);
    IMAGE_FORMAT_FOR_EACH
    #undef _
  }
  return 0;
}

/* Encode.
 */
 
int image_encode(struct sr_encoder *dst,struct image *image,int format) {
  switch (format) {
    #define _(tag) case IMAGE_FORMAT_##tag: return tag##_encode(dst,image);
    IMAGE_FORMAT_FOR_EACH
    #undef _
  }
  return -1;
}

/* New image.
 */
 
struct image *image_new_alloc(int pixelsize,int w,int h) {
  if ((w<1)||(w>0x7fff)) return 0;
  if ((h<1)||(h>0x7fff)) return 0;
  int stride;
  switch (pixelsize) {
    case 1: stride=(w+7)>>3; break;
    case 2: stride=(w+3)>>2; break;
    case 4: stride=(w+1)>>1; break;
    case 8: stride=w; break;
    case 16: stride=w<<1; break;
    case 24: stride=w*3; break;
    case 32: stride=w<<2; break;
    default: return 0;
  }
  if (stride>INT_MAX/h) return 0;
  struct image *image=calloc(1,sizeof(struct image));
  if (!image) return 0;
  if (!(image->v=calloc(stride,h))) {
    free(image);
    return 0;
  }
  image->w=w;
  image->h=h;
  image->stride=stride;
  image->pixelsize=pixelsize;
  return image;
}

/* One-dimensional iterator.
 */
 
struct iter1d {
  uint8_t *p;
  uint8_t sub;
  int pixelsize;
  void (*write)(struct iter1d *iter,int pixel);
  int (*read)(const struct iter1d *iter);
};

static int iter1d_read_1(const struct iter1d *iter) {
  return ((*(iter->p))&iter->sub)?1:0;
}
static void iter1d_write_1(struct iter1d *iter,int pixel) {
  if (pixel&1) (*(iter->p))|=iter->sub;
  else (*(iter->p))&=~iter->sub;
}

static int iter1d_read_2(const struct iter1d *iter) {
  return ((*(iter->p))>>iter->sub)&3;
}
static void iter1d_write_2(struct iter1d *iter,int pixel) {
  uint8_t mask=3<<iter->sub;
  (*(iter->p))=((*(iter->p))&~mask)|((pixel<<iter->sub)&mask);
}

static int iter1d_read_4(const struct iter1d *iter) {
  return ((*(iter->p))>>iter->sub)&15;
}
static void iter1d_write_4(struct iter1d *iter,int pixel) {
  if (iter->sub) *(iter->p)=((*(iter->p))&0xf0)|(pixel&0x0f);
  else *(iter->p)=((*(iter->p))&0x0f)|(pixel<<4);
}

static int iter1d_read_8(const struct iter1d *iter) {
  return *(iter->p);
}
static void iter1d_write_8(struct iter1d *iter,int pixel) {
  *(iter->p)=pixel;
}

static int iter1d_read_16(const struct iter1d *iter) {
  return (iter->p[0]<<8)|iter->p[1];
}
static void iter1d_write_16(struct iter1d *iter,int pixel) {
  iter->p[0]=pixel>>8;
  iter->p[1]=pixel;
}

static int iter1d_read_24(const struct iter1d *iter) {
  return (iter->p[0]<<16)|(iter->p[1]<<8)|iter->p[2];
}
static void iter1d_write_24(struct iter1d *iter,int pixel) {
  iter->p[0]=pixel>>16;
  iter->p[1]=pixel>>8;
  iter->p[2]=pixel;
}

static int iter1d_read_32(const struct iter1d *iter) {
  return (iter->p[0]<<24)|(iter->p[1]<<16)|(iter->p[2]<<8)|iter->p[3];
}
static void iter1d_write_32(struct iter1d *iter,int pixel) {
  iter->p[0]=pixel>>24;
  iter->p[1]=pixel>>16;
  iter->p[2]=pixel>>8;
  iter->p[3]=pixel;
}

static void iter1d_init(struct iter1d *iter,void *v,int pixelsize) {
  iter->p=v;
  switch (iter->pixelsize=pixelsize) {
    case 1: iter->sub=0x80; iter->write=iter1d_write_1; iter->read=iter1d_read_1; break;
    case 2: iter->sub=6; iter->write=iter1d_write_2; iter->read=iter1d_read_2; break;
    case 4: iter->sub=4; iter->write=iter1d_write_4; iter->read=iter1d_read_4; break;
    case 8: iter->write=iter1d_write_8; iter->read=iter1d_read_8; break;
    case 16: iter->write=iter1d_write_16; iter->read=iter1d_read_16; break;
    case 24: iter->write=iter1d_write_24; iter->read=iter1d_read_24; break;
    case 32: iter->write=iter1d_write_32; iter->read=iter1d_read_32; break;
  }
}

static void iter1d_next(struct iter1d *iter) {
  switch (iter->pixelsize) {
    case 1: {
        if (iter->sub==1) { iter->sub=0x80; iter->p++; }
        else iter->sub>>=1;
      } break;
    case 2: {
        if (iter->sub) iter->sub-=2;
        else { iter->sub=6; iter->p++; }
      } break;
    case 4: {
        if (iter->sub) iter->sub=0;
        else { iter->sub=4; iter->p++; }
      } break;
    case 8: iter->p+=1; break;
    case 16: iter->p+=2; break;
    case 24: iter->p+=3; break;
    case 32: iter->p+=4; break;
  }
}

/* Generic reformat.
 */
 
static void image_reformat_inner(
  uint8_t *dst,int dstpixelsize,int dstw,int dsth,int dststride,
  const struct image *srcimage,
  int (*cvt)(int pixel,void *userdata),
  void *userdata
) {
  const uint8_t *srcrow=srcimage->v;
  int cpw=(dstw<srcimage->w)?dstw:srcimage->w;
  int cph=(dsth<srcimage->h)?dsth:srcimage->h;
  int yi=cph; for (;yi-->0;dst+=dststride,srcrow+=srcimage->stride) {
    // We kind of start from scratch at each row. Saves some headaches.
    struct iter1d dstiter,srciter;
    iter1d_init(&dstiter,dst,dstpixelsize);
    iter1d_init(&srciter,(void*)srcrow,srcimage->pixelsize);
    if (cvt) {
      int xi=cpw; for (;xi-->0;iter1d_next(&dstiter),iter1d_next(&srciter)) {
        dstiter.write(&dstiter,cvt(srciter.read(&srciter),userdata));
      }
    } else {
      int xi=cpw; for (;xi-->0;iter1d_next(&dstiter),iter1d_next(&srciter)) {
        dstiter.write(&dstiter,srciter.read(&srciter));
      }
    }
  }
}

int image_reformat_in_place(
  struct image *image,
  int pixelsize,int w,int h,
  int (*cvt)(int pixel,void *userdata),
  void *userdata
) {
  switch (pixelsize) {
    case 0: pixelsize=image->pixelsize; break;
    case 1: case 2: case 4: case 8: case 16: case 24: case 32: break;
    default: return -1;
  }
  if (!w) w=image->w;
  if (!h) h=image->h;
  if ((w<1)||(w>0x7fff)) return -1;
  if ((h<1)||(h>0x7fff)) return -1;
  int nstride=(w*pixelsize+7)>>3;
  if ((image->pixelsize==pixelsize)&&(image->w==w)&&(image->h==h)&&(image->stride==nstride)) return 0;
  if (nstride>INT_MAX/h) return -1;
  int nlen=nstride*h;
  if ((nlen<=image_get_pixels_length(image))&&(nstride<=image->stride)&&(pixelsize<=image->pixelsize)) {
    image_reformat_inner(image->v,pixelsize,w,h,nstride,image,cvt,userdata);
  } else {
    void *nv=calloc(1,nlen);
    if (!nv) return -1;
    image_reformat_inner(nv,pixelsize,w,h,nstride,image,cvt,userdata);
    free(image->v);
    image->v=nv;
  }
  image->pixelsize=pixelsize;
  image->w=w;
  image->h=h;
  image->stride=nstride;
  return 0;
}

/* Reformat conveniences.
 */
 
int image_force_rgba(struct image *image) {
  return image_reformat_in_place(image,32,0,0,0,0);
}

int image_canonicalize(struct image *image) {
  if (image->pixelsize<=8) return image_reformat_in_place(image,1,0,0,0,0);
  return image_reformat_in_place(image,32,0,0,0,0);
}

/* Format names.
 */
 
int image_format_eval(const char *src,int srcc) {
  if (!src) return 0;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  char norm[16];
  if (srcc>sizeof(norm)) return 0;
  int i=srcc; while (i-->0) {
    if ((src[i]>='A')&&(src[i]<='Z')) norm[i]=src[i]+0x20;
    else norm[i]=src[i];
  }
  #define _(tag) if ((srcc==sizeof(#tag)-1)&&!memcmp(norm,#tag,srcc)) return IMAGE_FORMAT_##tag;
  IMAGE_FORMAT_FOR_EACH
  #undef _
  return 0;
}

const char *image_format_repr(int format) {
  switch (format) {
    #define _(tag) case IMAGE_FORMAT_##tag: return #tag;
    IMAGE_FORMAT_FOR_EACH
    #undef _
  }
  return 0;
}

/* Guess format from serial. Every format we support has an unambiguous signature.
 */

int image_format_guess(const void *src,int srcc) {
  if (!src) return 0;
  #if IMAGE_FORMAT_rawimg
    if ((srcc>=4)&&!memcmp(src,"\x00rIm",4)) return IMAGE_FORMAT_rawimg;
  #endif
  #if IMAGE_FORMAT_qoi
    if ((srcc>=4)&&!memcmp(src,"qoif",4)) return IMAGE_FORMAT_qoi;
  #endif
  #if IMAGE_FORMAT_rlead
    if ((srcc>=4)&&!memcmp(src,"\x00rld",4)) return IMAGE_FORMAT_rlead;
  #endif
  #if IMAGE_FORMAT_png
    const uint8_t *b=src;
    if ((srcc>=8)&&!memcmp(src,"\x89PNG\r\n\x1a\n",8)) return IMAGE_FORMAT_png;
  #endif
  return 0;
}
