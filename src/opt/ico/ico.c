#include "ico.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>
#include <stdint.h>

#if USE_png
  #include "opt/png/png.h"
#endif

/* Delete.
 */
 
static void ico_image_cleanup(struct ico_image *image) {
  if (image->v) free(image->v);
}

void ico_del(struct ico *ico) {
  if (!ico) return;
  if (ico->imagev) {
    while (ico->imagec-->0) ico_image_cleanup(ico->imagev+ico->imagec);
    free(ico->imagev);
  }
  free(ico);
}

/* Add image.
 */
 
static struct ico_image *ico_add_image(struct ico *ico) {
  if (ico->imagec>=ico->imagea) {
    int na=ico->imagea+4;
    if (na>INT_MAX/sizeof(struct ico_image)) return 0;
    void *nv=realloc(ico->imagev,sizeof(struct ico_image)*na);
    if (!nv) return 0;
    ico->imagev=nv;
    ico->imagea=na;
  }
  struct ico_image *image=ico->imagev+ico->imagec++;
  memset(image,0,sizeof(struct ico_image));
  return image;
}

/* Decode file.
 * Nothing super heavy. Just identify all of the constituent images.
 */
 
static int ico_decode(struct ico *ico,const uint8_t *src,int srcc) {
  
  if (srcc<6) return -1;
  if (memcmp(src,"\0\0\1\0",4)) return -1; // Signature and file type: icon
  int imagec=src[4]|(src[5]<<8);
  int srcp=6;
  while (imagec-->0) {
    if (srcp>srcc-16) return -1;
    int w=src[srcp++]; if (!w) w=256;
    int h=src[srcp++]; if (!h) h=256;
    int ctc=src[srcp++];
    int rsv=src[srcp++];
    if (rsv) return -1;
    int planec=src[srcp]|(src[srcp+1]<<8); srcp+=2;
    int pixelsize=src[srcp]|(src[srcp+1]<<8); srcp+=2;
    int paylen=src[srcp]|(src[srcp+1]<<8)|(src[srcp+2]<<16)|(src[srcp+3]<<24); srcp+=4;
    int payp=src[srcp]|(src[srcp+1]<<8)|(src[srcp+2]<<16)|(src[srcp+3]<<24); srcp+=4;
    if ((paylen<0)||(payp<0)||(payp>srcc-paylen)) return -1;
    struct ico_image *image=ico_add_image(ico);
    if (!image) return -1;
    image->serial=src+payp;
    image->serialc=paylen;
    image->w=w;
    image->h=h;
    image->ctc=ctc;
    image->planec=planec;
    image->pixelsize=pixelsize;
    // (stride,v) do not get populated yet
  }
  
  return 0;
}

/* Decode from BMP.
 * Since we're resolving from a color table anyway, might as well produce RGBA and pretend it's PNG.
 */
 
static int ico_image_decode_bmp(
  struct ico *ico,struct ico_image *image,
  const uint8_t *ct,
  const uint8_t *bits,int bitstride,
  const uint8_t *mask,int maskstride
) {
  if (image->v) return -1;
  if ((image->pixelsize!=4)||(image->ctc<16)) return -1; // For now, only accept 4-bit index with a complete color table.
  if (!(image->v=malloc(image->w*image->h*4))) return -1;
  uint8_t *dstrow=((uint8_t*)image->v)+image->w*4*(image->h-1);
  int yi=image->h;
  for (;yi-->0;dstrow-=image->w*4,bits+=bitstride,mask+=maskstride) {
    uint8_t *dstp=dstrow;
    const uint8_t *bitsp=bits;
    const uint8_t *maskp=mask;
    uint8_t maskmask=0x80;
    int bitshift=4;
    int xi=image->w;
    for (;xi-->0;dstp+=4) {
      int ix=((*bitsp)>>bitshift)&15;
      if (bitshift) bitshift=0;
      else { bitshift=4; bitsp++; }
      const uint8_t *bgrx=ct+(ix<<2);
      dstp[0]=bgrx[2];
      dstp[1]=bgrx[1];
      dstp[2]=bgrx[0];
      dstp[3]=((*maskp)&maskmask)?0x00:0xff; // NB mask is inverted. zero means opaque and one transparent
      if (maskmask==1) { maskmask=0x80; maskp++; }
      else maskmask>>=1;
    }
  }
  image->pixelsize=32;
  image->stride=image->w<<2;
  image->depth=8;
  image->colortype=6;
  return 0;
}

/* Decode image.
 */
 
static int ico_image_decode(struct ico *ico,struct ico_image *image) {
  if (image->v) return 0;
  
  // I'm told the data can be a straight PNG file.
  if ((image->serialc>=8)&&!memcmp(image->serial,"\x89PNG\r\n\x1a\n",8)) {
    #if USE_png
      struct png_image *png=png_decode(image->serial,image->serialc);
      if (!png) return -1;
      image->v=png->pixels;
      png->pixels=0;
      image->w=png->w;
      image->h=png->h;
      image->stride=png->stride;
      image->depth=png->depth;
      image->colortype=png->colortype;
      png_image_del(png);
      return 0;
    #else
      return -1;
    #endif
  }
  
  // Otherwise it should be BMP, minus the outer header.
  if (image->serialc>=16) {
    const uint8_t *v=image->serial;
    int hdrlen=v[0]|(v[1]<<8)|(v[2]<<16)|(v[3]<<24);
    int w=v[4]|(v[5]<<8)|(v[6]<<16)|(v[7]<<24);
    int h=v[8]|(v[9]<<8)|(v[10]<<16)|(v[11]<<24);
    int mystery=v[12]|(v[13]<<8);
    int pixelsize=v[14]|(v[15]<<8);
    // I've only tested with 4-bit BMP icons. This could surely bear to generalize somewhat:
    if ((hdrlen>=16)&&(w==image->w)&&(h==image->h<<1)&&(mystery==1)&&(pixelsize==image->pixelsize)) {
      int srcstride=(w*pixelsize+7)>>3;
      if (srcstride&3) srcstride=(srcstride+3)&~3;
      int maskstride=(w+7)>>3;
      if (maskstride&3) maskstride=(maskstride+3)&~3;
      int expectlen=hdrlen+image->ctc*4+srcstride*image->h+maskstride*image->h;
      if (expectlen<=image->serialc) {
        const uint8_t *ct=v+hdrlen;
        const uint8_t *bits=ct+image->ctc*4;
        const uint8_t *mask=bits+srcstride*image->h;
        return ico_image_decode_bmp(ico,image,ct,bits,srcstride,mask,maskstride);
      }
    }
  }
  
  return -1;
}

/* Rewrite decoded image in place as RGBA.
 * ...actually we haven't needed this.
 * PNG should be RGBA in the first place, and BMP use a color table which we resolve at decode to RGBA.
 */
 
static int ico_image_rewrite_rgba(struct ico *ico,struct ico_image *image) {
  fprintf(stderr,"%s TODO\n",__func__);
  //TODO
  return -1;
}

/* New.
 */

struct ico *ico_new(const void *src,int srcc) {
  if (!src||(srcc<0)) return 0;
  struct ico *ico=calloc(1,sizeof(struct ico));
  if (!ico) return 0;
  if (ico_decode(ico,src,srcc)<0) {
    ico_del(ico);
    return 0;
  }
  return ico;
}

/* Nonzero if image is RGBA with minimum stride.
 */
 
static int ico_image_is_rgba(const struct ico_image *image) {
  if (!image->v) return 0;
  if (image->pixelsize!=32) return 0;
  if (image->stride!=image->w<<2) return 0;
  if (image->depth!=8) return 0;
  if (image->colortype!=6) return 0;
  return 1;
}

/* Nonzero if (a) is closer to (w,h) than (b).
 */
 
static int ico_image_better(const struct ico_image *a,const struct ico_image *b,int w,int h) {
  int adw=a->w-w; if (adw<0) adw=0;
  int adh=a->h-h; if (adh<0) adh=0;
  int bdw=b->w-w; if (bdw<0) bdw=0;
  int bdh=b->h-h; if (bdh<0) bdh=0;
  int ad=adw+adh;
  int bd=bdw+bdh;
  return (ad<bd);
}

/* Decode one image plane and force RGBA if requested.
 */
 
struct ico_image *ico_ready_image(
  struct ico *ico,
  struct ico_image *image,
  int force_rgba
) {
  if (!image->v) {
    if (ico_image_decode(ico,image)<0) return 0;
  }
  if (force_rgba&&!ico_image_is_rgba(image)) {
    if (ico_image_rewrite_rgba(ico,image)<0) return 0;
  }
  return image;
}

/* Get image.
 */

struct ico_image *ico_get_image(struct ico *ico,int w,int h,int obo,int force_rgba) {
  if (!ico) return 0;
  struct ico_image *nearest=0;
  struct ico_image *image=ico->imagev;
  int i=ico->imagec;
  for (;i-->0;image++) {
    if ((image->w==w)&&(image->h==h)) return ico_ready_image(ico,image,force_rgba);
    if (!nearest||(obo&&ico_image_better(image,nearest,w,h))) nearest=image;
  }
  if (obo&&nearest) return ico_ready_image(ico,nearest,force_rgba);
  return 0;
}
