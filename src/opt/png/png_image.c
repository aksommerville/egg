#include "png.h"
#include <stdlib.h>
#include <limits.h>
#include <string.h>

#define PNG_IMAGE_SIZE_LIMIT 8192

/* Delete image.
 */

void png_image_del(struct png_image *image) {
  if (!image) return;
  if (image->refc-->1) return;
  
  if (image->pixels&&image->ownpixels) free(image->pixels);
  if (image->chunkv) {
    while (image->chunkc-->0) {
      void *v=image->chunkv[image->chunkc].v;
      if (v) free(v);
    }
    free(image->chunkv);
  }
  if (image->keepalive) png_image_del(image->keepalive);
  
  free(image);
}

/* Retain.
 */
 
int png_image_ref(struct png_image *image) {
  if (!image) return -1;
  if (image->refc<1) return -1;
  if (image->refc==INT_MAX) return -1;
  image->refc++;
  return 0;
}

/* New.
 */

struct png_image *png_image_new(int w,int h,uint8_t depth,uint8_t colortype) {
  
  // Straight zeroes is a special case, that's "give me a valid object and i'll fill it in".
  if (!w&&!h&&!depth&&!colortype) {
    struct png_image *image=calloc(1,sizeof(struct png_image));
    if (!image) return 0;
    image->refc=1;
    return image;
  }
  
  // Dimensions (0,0) remain legal to request no pixels, but now you must provide valid (depth,colortype).
  if ((w<0)||(w>PNG_IMAGE_SIZE_LIMIT)) return 0;
  if ((h<0)||(h>PNG_IMAGE_SIZE_LIMIT)) return 0;
  if ((!w||!h)&&(w||h)) return 0;
  int chanc=png_channel_count_for_colortype(colortype);
  if (chanc<1) return 0;
  switch (depth) {
    case 1: case 2: case 4: case 8: case 16: break;
    default: return 0;
  }
  int pixelsize=chanc*depth;
  if (pixelsize>=8) {
    if (pixelsize%8) return 0;
  } else {
    if (8%pixelsize) return 0;
  }
  struct png_image *image=calloc(1,sizeof(struct png_image));
  if (!image) return 0;
  image->refc=1;
  image->w=w;
  image->h=h;
  image->stride=(pixelsize*w+7)>>3;
  image->depth=depth;
  image->colortype=colortype;
  if (w) {
    if (!(image->pixels=calloc(image->stride,image->h))) {
      free(image);
      return 0;
    }
    image->ownpixels=1;
  }
  return image;
}

/* Copy PLTE and tRNS if they exist.
 * Any other extra chunks, we drop them.
 */
 
static int png_image_copy_important_chunks(
  struct png_image *dst,
  const struct png_image *src
) {
  const struct png_chunk *chunk=src->chunkv;
  int i=src->chunkc;
  for (;i-->0;chunk++) {
    if ((chunk->chunkid==PNG_CHUNKID_PLTE)||(chunk->chunkid==PNG_CHUNKID_tRNS)) {
      if (png_image_add_chunk(dst,chunk->chunkid,chunk->v,chunk->c)<0) return -1;
    }
  }
  return 0;
}

/* Helper for pixel transfer during reformat.
 */
 
struct png_reformat_context {
  int x0;
  int y0;
  struct png_image *dst;
};

static int png_reformat_cb(struct png_image *src,int x,int y,uint32_t rgba,void *userdata) {
  struct png_reformat_context *ctx=userdata;
  png_image_write(ctx->dst,x-ctx->x0,y-ctx->y0,rgba);
  return 0;
}

/* Reformat.
 */

struct png_image *png_image_reformat(
  struct png_image *image,
  int x,int y,int w,int h,
  uint8_t depth,uint8_t colortype,
  int always_copy
) {
  if (!image) return 0;
  if (!w) w=image->w-x;
  if (!h) h=image->h-y;
  if (!depth) {
    depth=image->depth;
    if (!colortype) colortype=image->colortype;
  }
  if ((w<1)||(w>PNG_IMAGE_SIZE_LIMIT)) return 0;
  if ((h<1)||(h>PNG_IMAGE_SIZE_LIMIT)) return 0;
  int chanc=png_channel_count_for_colortype(colortype);
  if (chanc<1) return 0;
  switch (depth) {
    case 1: case 2: case 4: case 8: case 16: break;
    default: return 0;
  }
  int pixelsize=chanc*depth;
  if (pixelsize>=8) {
    if (pixelsize%8) return 0;
  } else {
    if (8%pixelsize) return 0;
  }
  
  /* Now that we know the final geometry, check for two possible optimizations:
   *  1. Use (image) as is, just retain it.
   *  2. Create a view pointing into (image->pixels).
   */
  if (!always_copy) {
    if (!x&&!y&&(w==image->w)&&(h==image->h)) {
      if ((depth==image->depth)&&(colortype==image->colortype)) {
        if (png_image_ref(image)>=0) return image;
      }
    }
    if ((depth==image->depth)&&(colortype==image->colortype)) {
      if ((x>=0)&&(x+w<=image->w)) {
        if ((y>=0)&&(y+h<=image->h)) {
          int align=x*pixelsize;
          if (!(align&7)) {
            if (png_image_ref(image)>=0) {
              struct png_image *dst=calloc(1,sizeof(struct png_image));
              if (!dst) {
                png_image_del(image);
              } else {
                dst->refc=1;
                dst->w=w;
                dst->h=h;
                dst->stride=image->stride;
                dst->depth=depth;
                dst->colortype=colortype;
                dst->pixels=((char*)image->pixels)+y*image->stride+(align>>3);
                dst->ownpixels=0;
                dst->keepalive=image;
                png_image_copy_important_chunks(dst,image);
                return dst;
              }
            }
          }
        }
      }
    }
  }
  
  // Make a new image to convert into.
  struct png_image *dst=png_image_new(w,h,depth,colortype);
  if (!dst) return 0;
  
  // Select the copy region. We only need to clip against (image), not (dst).
  int srcx=x,srcy=y,dstx=0,dsty=0,cpw=w,cph=h;
  if (srcx<0) { dstx-=srcx; cpw+=srcx; srcx=0; }
  if (srcy<0) { dsty-=srcy; cph+=srcy; srcy=0; }
  if (srcx+cpw>image->w) cpw=image->w-srcx;
  if (srcy+cph>image->h) cph=image->h-srcy;
  if ((cpw>0)&&(cph>0)) {
    // If format didn't change, try a rowwise memcpy.
    if ((depth==image->depth)&&(colortype==image->colortype)) {
      // Confirm that each row lines up on a byte boundary on both ends, in both images.
      if (depth<8) {
        int srcposition=pixelsize*srcx;
        if (srcposition&7) goto _general_copy_;
        int dstposition=pixelsize*dstx;
        if (dstposition&7) goto _general_copy_;
        if ((cpw*pixelsize)&7) goto _general_copy_;
      }
      char *dstrow=((char*)dst->pixels)+dsty*dst->stride+((dstx*pixelsize+7)>>3);
      const char *srcrow=((char*)image->pixels)+srcy*image->stride+((srcx*pixelsize+7)>>3);
      int cpc=(cpw*pixelsize+7)>>3;
      for (;cph-->0;dstrow+=dst->stride,srcrow+=image->stride) {
        memcpy(dstrow,srcrow,cpc);
      }
    // General case: A wasteful iteration process.
    // We do all the bounds checking and format selection for each pixel, it's kind of ridiculous.
    } else {
     _general_copy_:;
      int suby=cph; while (suby-->0) {
        int subx=cpw; while (subx-->0) {
          png_image_write(dst,dstx+subx,dsty+suby,png_image_read(image,srcx+subx,srcy+suby));
        }
      }
    }
  }
  
  png_image_copy_important_chunks(dst,image);
  return dst;
}

/* Read one pixel as RGBA.
 */
 
static uint32_t png_read_plte_as_rgba(const struct png_image *image,uint8_t ix) {
  int p=ix*3;
  const struct png_chunk *chunk=image->chunkv;
  int i=image->chunkc;
  for (;i-->0;chunk++) {
    if (chunk->chunkid!=PNG_CHUNKID_PLTE) continue;
    if (p>chunk->c-3) return 0;
    const uint8_t *v=chunk->v+p;
    return (v[0]<<24)|(v[1]<<16)|(v[2]<<8)|0xff;
  }
  return 0;
}
 
uint32_t png_image_read(const struct png_image *image,int x,int y) {
  if (!image) return 0;
  if ((x<0)||(y<0)||(x>=image->w)||(y>=image->h)) return 0;
  const uint8_t *row=((uint8_t*)image->pixels)+y*image->stride;
  switch (image->colortype) {
    case 0: switch (image->depth) {
        case 1: row+=x>>3; if ((*row)&(0x80>>(x&7))) return 0xffffffff; return 0x000000ff;
        case 2: { row+=x>>2; uint8_t y=((*row)>>(6-(x&3)*2))&3; y|=y<<2; y|=y<<4; return (y<<24)|(y<<16)|(y<<8)|0xff; }
        case 4: { row+=x>>1; uint8_t y=(x&1)?((*row)&15):((*row)>>4); y|=y<<4; return (y<<24)|(y<<16)|(y<<8)|0xff; }
        case 8: return (row[x]<<24)|(row[x]<<16)|(row[x]<<8)|0xff;
        case 16: row+=x<<1; return ((*row)<<24)|((*row)<<16)|((*row)<<8)|0xff;
      } break;
    case 2: switch (image->depth) {
        case 8: row+=x*3; return (row[0]<<24)|(row[1]<<16)|(row[2]<<8)|0xff;
        case 16: row+=x*6; return (row[0]<<24)|(row[2]<<16)|(row[4]<<8)|0xff;
      } break;
    case 3: switch (image->depth) {
        case 1: row+=x>>3; return png_read_plte_as_rgba(image,((*row)&(0x80>>(x&7)))?1:0);
        case 2: row+=x>>2; return png_read_plte_as_rgba(image,((*row)>>(6-(x&3)*2))&3);
        case 4: row+=x>>1; return png_read_plte_as_rgba(image,(x&1)?((*row)&15):((*row)>>4));
        case 8: row+=x;    return png_read_plte_as_rgba(image,*row);
      } break;
    case 4: switch (image->depth) {
        case 8: row+=x<<1; return (row[0]<<24)|(row[0]<<16)|(row[0]<<8)|row[1];
        case 16: row+=x<<2; return (row[0]<<24)|(row[0]<<16)|(row[0]<<8)|row[2];
      } break;
    case 6: switch (image->depth) {
        case 8: row+=x<<2; return (row[0]<<24)|(row[1]<<16)|(row[2]<<8)|row[3];
        case 16: row+=x<<3; return (row[0]<<24)|(row[2]<<16)|(row[4]<<8)|row[6];
      } break;
  }
  return 0;
}

/* Write one pixel from RGBA.
 */
 
static int png_lookup_plte(const struct png_image *image,uint8_t r,uint8_t g,uint8_t b) {
  const struct png_chunk *chunk=image->chunkv;
  int i=image->chunkc;
  for (;i-->0;chunk++) {
    if (chunk->chunkid!=PNG_CHUNKID_PLTE) continue;
    int best=-1,bestscore=INT_MAX;
    const uint8_t *v=chunk->v;
    int c=chunk->c/3;
    int p=0; for (;p<c;p++,v+=3) {
      int dr=r-v[0]; if (dr<0) dr=-dr;
      int dg=g-v[1]; if (dg<0) dg=-dg;
      int db=b-v[2]; if (db<0) db=-db;
      int score=dr+dg+db;
      if (!score) return p;
      if (score<bestscore) {
        best=p;
        bestscore=score;
      }
    }
    return best;
  }
  return -1;
}
 
void png_image_write(struct png_image *image,int x,int y,uint32_t rgba) {
  if (!image) return;
  if ((x<0)||(y<0)||(x>=image->w)||(y>=image->h)) return;
  uint8_t r=rgba>>24,g=rgba>>16,b=rgba>>8,a=rgba;
  uint8_t *row=((uint8_t*)image->pixels)+y*image->stride;
  switch (image->colortype) {
    case 0: switch (image->depth) {
        case 1: row+=x>>3; if (r+g+b>=384) (*row)|=(0x80>>(x&7)); else (*row)&=~(0x80>>(x&7)); return;
        case 2: { row+=x>>2; uint8_t luma=(r+g+b)/3; luma>>=6; int shift=6-(x&3)*2; uint8_t mask=3<<shift; (*row)=((*row)&~mask)|(luma<<shift); } return;
        case 4: { row+=x>>1; uint8_t luma=(r+g+b)/3; if (x&1) (*row)=((*row)&0xf0)|(luma>>4); else (*row)=((*row)&0x0f)|(luma&0xf0); } return;
        case 8: row[x]=(r+g+b)/3; return;
        case 16: row+=x<<1; row[0]=row[1]=(r+g+b)/3; return;
      } break;
    case 2: switch (image->depth) {
        case 8: row+=x*3; row[0]=r; row[1]=g; row[2]=b; return;
        case 16: row+=x*6; row[0]=row[1]=r; row[2]=row[3]=g; row[4]=row[5]=b; return;
      } break;
    case 3: {
        int p=png_lookup_plte(image,r,g,b);
        switch (image->depth) {
          case 1: row+=x>>3; if (p&1) (*row)|=(0x80>>(x&7)); else (*row)&=~(0x80>>(x&7)); return;
          case 2: { row+=x>>2; p&=3; int shift=6-(x&3)*2; uint8_t mask=3<<shift; (*row)=((*row)&~mask)|(p<<shift); } return;
          case 4: row+=x>>1; if (x&1) (*row)=((*row)&0xf0)|(p&0x0f); else (*row)=((*row)&0x0f)|(p<<4); return;
          case 8: row[x]=p; return;
        }
      } break;
    case 4: switch (image->depth) {
        case 8: row+=x<<1; row[0]=(r+g+b)/3; row[1]=a; return;
        case 16: row+=x<<2; row[0]=row[1]=(r+g+b)/3; row[2]=row[3]=a; return;
      } break;
    case 6: switch (image->depth) {
        case 8: row+=x<<2; row[0]=r; row[1]=g; row[2]=b; row[3]=a; return;
        case 16: row+=x<<3; row[0]=row[1]=r; row[2]=row[3]=g; row[4]=row[5]=b; row[6]=row[7]=a; return;
      } break;
  }
}

/* Add chunk.
 */
 
int png_image_add_chunk(struct png_image *image,uint32_t chunkid,const void *v,int c) {
  if (!image||(c<0)||(c&&!v)) return -1;
  if (image->chunkc>=image->chunka) {
    int na=image->chunka+4;
    if (na>INT_MAX/sizeof(struct png_chunk)) return -1;
    void *nv=realloc(image->chunkv,na*sizeof(struct png_chunk));
    if (!nv) return -1;
    image->chunkv=nv;
    image->chunka=na;
  }
  void *nv=0;
  if (c) {
    if (!(nv=malloc(c))) return -1;
    memcpy(nv,v,c);
  }
  struct png_chunk *chunk=image->chunkv+image->chunkc++;
  chunk->chunkid=chunkid;
  chunk->v=nv;
  chunk->c=c;
  return 0;
}

/* Format properties.
 */

void png_depth_colortype_legal(uint8_t *depth,uint8_t *colortype) {
  switch (*colortype) {
    case 0: switch (*depth) {
        case 1: case 2: case 4: case 8: case 16: return;
        default: *depth=8; return;
      }
    case 2: switch (*depth) {
        case 8: case 16: return;
        default: *depth=8; return;
      }
    case 3: switch (*depth) {
        case 1: case 2: case 4: case 8: return;
        default: *depth=8; return;
      }
    case 4: switch (*depth) {
        case 8: case 16: return;
        default: *depth=8; return;
      }
    case 6: switch (*depth) {
        case 8: case 16: return;
        default: *depth=8; return;
      }
  }
  *depth=8;
  *colortype=6;
}

void png_depth_colortype_8bit(uint8_t *depth,uint8_t *colortype) {
  *depth=8; // legal for all colortype
}

void png_depth_colortype_luma(uint8_t *depth,uint8_t *colortype) {
  switch (*colortype) {
    case 0:
    case 4:
      return;
    case 2:
    case 3: *colortype=0; return;
    case 6: *colortype=4; return;
  }
  *colortype=0;
}

void png_depth_colortype_rgb(uint8_t *depth,uint8_t *colortype) {
  switch (*colortype) {
    case 2:
    case 6:
      return;
    case 0:
    case 3: *colortype=2; if (*depth<8) *depth=8; return;
    case 4: *colortype=6; return;
  }
  *colortype=2;
  if (*depth<8) *depth=8;
}

void png_depth_colortype_alpha(uint8_t *depth,uint8_t *colortype) {
  switch (*colortype) {
    case 0: *colortype=4; if (*depth<8) *depth=8; return;
    case 2: *colortype=6; if (*depth<8) *depth=8; return;
    case 3: *colortype=6; if (*depth<8) *depth=8; return;
    case 4: return;
    case 6: return;
  }
  *colortype=6;
  if (*depth<8) *depth=8;
}

void png_depth_colortype_opaque(uint8_t *depth,uint8_t *colortype) {
  switch (*colortype) {
    case 0:
    case 2:
    case 3:
      return;
    case 4: *colortype=0; return;
    case 6: *colortype=2; return;
  }
  *colortype=2;
  if (*depth<8) *depth=8;
}

int png_channel_count_for_colortype(uint8_t colortype) {
  switch (colortype) {
    case 0: return 1;
    case 2: return 3;
    case 3: return 1;
    case 4: return 2;
    case 6: return 4;
  }
  return 0;
}
