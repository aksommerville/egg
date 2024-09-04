#include "render_internal.h"

/* Delete.
 */
 
static void render_texture_cleanup(struct render_texture *texture) {
  if (texture->texid) glDeleteTextures(1,&texture->texid);
  if (texture->fbid) glDeleteFramebuffers(1,&texture->fbid);
}
 
void render_del(struct render *render) {
  if (!render) return;
  if (render->texturev) {
    while (render->texturec-->0) render_texture_cleanup(render->texturev+render->texturec);
    free(render->texturev);
  }
  if (render->textmp) free(render->textmp);
  if (render->rvtxv) free(render->rvtxv);
  free(render);
}

/* New.
 */
 
struct render *render_new() {
  struct render *render=calloc(1,sizeof(struct render));
  if (!render) return 0;
  
  render->alpha=0xff;
  
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
  
  if (render_init_programs(render)<0) {
    render_del(render);
    return 0;
  }
  
  return render;
}

/* Drop textures.
 */
 
void render_drop_textures(struct render *render) {
  while (render->texturec>1) {
    render->texturec--;
    struct render_texture *texture=render->texturev+render->texturec;
    render_texture_cleanup(texture);
  }
}

/* Enumerate textures.
 */
 
int render_texid_by_index(const struct render *render,int p) {
  if (p<0) return -1;
  if (p>=render->texturec) return -1;
  return p+1;
}

/* Delete texture.
 */

void render_texture_del(struct render *render,int texid) {
  if ((texid<2)||(texid>render->texturec)) return; // sic "<2", no deleting the main
  texid--;
  struct render_texture *texture=render->texturev+texid;
  render_texture_cleanup(texture);
  memset(texture,0,sizeof(struct render_texture));
}

/* New texture of explicit id.
 */
 
int render_texture_require(struct render *render,int texid) {
  if (texid<1) return -1;
  while (texid>render->texturec) {
    if (render_texture_new(render)<1) return -1;
  }
  return 0;
}

/* New texture.
 */
 
int render_texture_new(struct render *render) {
  struct render_texture *texture=0;
  if (render->texturec<render->texturea) {
    texture=render->texturev+render->texturec++;
  } else {
    int i=render->texturec;
    struct render_texture *q=render->texturev;
    for (;i-->0;q++) {
      if (q->texid) continue;
      texture=q;
      break;
    }
    if (!texture) {
      int na=render->texturea+16;
      if (na>INT_MAX/sizeof(struct render_texture)) return 0;
      void *nv=realloc(render->texturev,sizeof(struct render_texture)*na);
      if (!nv) return 0;
      render->texturev=nv;
      render->texturea=na;
      texture=render->texturev+render->texturec++;
    }
  }
  memset(texture,0,sizeof(struct render_texture));
  
  glGenTextures(1,&texture->texid);
  if (!texture->texid) {
    glGenTextures(1,&texture->texid);
    if (!texture->texid) {
      return 0;
    }
  }
  
  glBindTexture(GL_TEXTURE_2D,texture->texid);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
  
  return (texture-render->texturev)+1;
}

/* Expected length of incoming image data.
 */
 
static int render_texture_measure(int w,int h,int stride,int fmt) {
  if ((w<1)||(h<1)||(stride<1)) return -1;
  int pixelsize=0;
  switch (fmt) {
    case EGG_TEX_FMT_RGBA: pixelsize=32; break;
    case EGG_TEX_FMT_A8: pixelsize=8; break;
    case EGG_TEX_FMT_A1: pixelsize=1; break;
  }
  if (pixelsize<1) return -1;
  if (w>(INT_MAX-7)/pixelsize) return -1;
  int bitsperrow=w*pixelsize;
  int minstride=(bitsperrow+7)>>3;
  if (stride!=minstride) return -1; // incoming strides must be exact (TODO should we work around it?)
  if (stride>INT_MAX/h) return -1;
  return stride*h;
}

static int render_minimum_stride(int w,int fmt) {
  if (w<1) return 0;
  if (w>0x7fff) return 0;
  switch (fmt) {
    case EGG_TEX_FMT_RGBA: return w<<2;
    case EGG_TEX_FMT_A8: return w;
    case EGG_TEX_FMT_A1: return (w+7)>>3;
  }
  return 0;
}

/* Expand to 32 from 1 bit.
 */
 
static void render_expand_1bit(uint32_t *dst,const uint8_t *src,int w,int h,int srcstride,uint32_t zero,uint32_t one) {
  int zc=0,oc=0;
  int yi=h;
  for (;yi-->0;dst+=w,src+=srcstride) {
    const uint8_t *srcp=src;
    uint32_t *dstp=dst;
    uint8_t mask=0x80;
    int xi=w;
    for (;xi-->0;dstp++) {
      if ((*srcp)&mask) oc++; else zc++;
      *dstp=((*srcp)&mask)?one:zero;
      if (mask==1) { mask=0x80; srcp++; }
      else mask>>=1;
    }
  }
}

/* Upload pixels to a texture. Null is legal.
 */
 
static int render_texture_upload(struct render *render,struct render_texture *texture,int w,int h,int stride,int fmt,const void *v) {
  int ifmt,glfmt,type,chanc;
  switch (fmt) {
    case EGG_TEX_FMT_RGBA: chanc=4; ifmt=GL_RGBA; glfmt=GL_RGBA; type=GL_UNSIGNED_BYTE; break;
    case EGG_TEX_FMT_A8: chanc=1; ifmt=GL_ALPHA; glfmt=GL_ALPHA; type=GL_UNSIGNED_BYTE; break;
    case EGG_TEX_FMT_A1: {
        int expstride=w<<2;
        int explen=expstride*h;
        if (explen>render->textmpa) {
          void *nv=realloc(render->textmp,explen);
          if (!nv) return -1;
          render->textmp=nv;
          render->textmpa=explen;
        }
        uint32_t zero,one;
        uint8_t alphabytes[4]={0,0,0,0xff};
        uint32_t alpha=*(uint32_t*)alphabytes;
        if (fmt==EGG_TEX_FMT_A1) {
          zero=0x00000000;
          one=alpha;
        } else {
          zero=alpha;
          one=0xffffffff;
        }
        render_expand_1bit(render->textmp,v,w,h,stride,zero,one);
        v=render->textmp;
        stride=expstride;
        chanc=4;
        ifmt=GL_RGBA;
        glfmt=GL_RGBA;
        type=GL_UNSIGNED_BYTE;
      } break;
    default: return -1;
  }
  if (stride!=w*chanc) return -1;
  glBindTexture(GL_TEXTURE_2D,texture->texid);
  glTexImage2D(GL_TEXTURE_2D,0,ifmt,w,h,0,glfmt,type,v);
  texture->w=w;
  texture->h=h;
  texture->fmt=fmt;
  texture->qual=0;
  texture->rid=0;
  return 0;
}

/* Examine decoded image, coerce to a valid format if necessary, and return the Egg texture format.
 */
 
static int render_force_valid(struct render *render,struct image *image) {
  switch (image->pixelsize) {
    case 1: return EGG_TEX_FMT_A1;
    case 8: return EGG_TEX_FMT_A8;
    case 32: return EGG_TEX_FMT_RGBA;
  }
  if (image_force_rgba(image)<0) return -1;
  return EGG_TEX_FMT_RGBA;
}

/* Load texture.
 */

int render_texture_load(struct render *render,int texid,int w,int h,int stride,int fmt,const void *src,int srcc) {
  if (!srcc) src=0;
  if ((texid<1)||(texid>render->texturec)) return -1;
  struct render_texture *texture=render->texturev+texid-1;
  
  /* If format is completely unspecified, (src) may be an encoded image.
   * Not permitted for texid 1.
   */
  if (!w&&!h&&!stride&&!fmt) {
    if (texid==1) return -1;
    struct image *image=image_decode(src,srcc);
    if (!image) return -1;
    if ((fmt=render_force_valid(render,image))<0) {
      image_del(image);
      return -1;
    }
    int err=render_texture_upload(render,texture,image->w,image->h,image->stride,fmt,image->v);
    image_del(image);
    return err;
  }
  
  /* Texid 1, dimensions must not change, except the first call.
   */
  if (texid==1) {
    if (texture->w&&texture->h) {
      if ((w!=texture->w)||(h!=texture->h)) return -1;
    }
  }
  
  /* Validate length, then upload.
   */
  if (stride<1) stride=render_minimum_stride(w,fmt);
  int expectsrcc=render_texture_measure(w,h,stride,fmt);
  if ((expectsrcc<1)||(src&&(srcc<expectsrcc))) return -1;
  if (render_texture_upload(render,texture,w,h,stride,fmt,src)<0) return -1;
  return 0;
}

/* Texture origin (commentary we stash on the client's behalf).
 */
 
void render_texture_set_origin(struct render *render,int texid,int qual,int rid) {
  if ((texid<1)||(texid>render->texturec)) return;
  struct render_texture *texture=render->texturev+texid-1;
  texture->qual=qual;
  texture->rid=rid;
}

void render_texture_get_origin(int *qual,int *rid,const struct render *render,int texid) {
  if ((texid<1)||(texid>render->texturec)) return;
  struct render_texture *texture=render->texturev+texid-1;
  *qual=texture->qual;
  *rid=texture->rid;
}
  
/* Get texture properties.
 */
 
void render_texture_get_header(int *w,int *h,int *fmt,const struct render *render,int texid) {
  if ((texid<1)||(texid>render->texturec)) return;
  struct render_texture *texture=render->texturev+texid-1;
  if (w) *w=texture->w;
  if (h) *h=texture->h;
  if (fmt) *fmt=texture->fmt;
}

/* Get texture pixels.
 */
 
void *render_texture_get_pixels(int *w,int *h,int *fmt,struct render *render,int texid) {
  if (!w||!h||!fmt) return 0;
  if ((texid<1)||(texid>render->texturec)) return 0;
  struct render_texture *texture=render->texturev+texid-1;
  //if (!texture->fbid) return 0; // We can only read from textures that have an associated framebuffer.
  if (render_texture_require_fb(texture)<0) return 0;
  *w=texture->w;
  *h=texture->h;
  *fmt=texture->fmt;
  int stride=render_minimum_stride(texture->w,texture->fmt);
  int len=render_texture_measure(texture->w,texture->h,stride,texture->fmt);
  if (len<1) return 0;
  void *dst=malloc(len);
  if (!dst) return 0;
  int glfmt=GL_RGBA,gltype=GL_UNSIGNED_BYTE;
  switch (texture->fmt) {
    case EGG_TEX_FMT_RGBA: break;
    case EGG_TEX_FMT_A8: glfmt=GL_ALPHA; break;
    case EGG_TEX_FMT_A1: break;
    default: free(dst); return 0;
  }
  glBindFramebuffer(GL_FRAMEBUFFER,texture->fbid);
  glReadPixels(0,0,texture->w,texture->h,glfmt,gltype,dst);
  return dst;
}

/* Allocate framebuffer if needed.
 */
 
int render_texture_require_fb(struct render_texture *texture) {
  if (texture->fbid) return 0;
  glGenFramebuffers(1,&texture->fbid);
  if (!texture->fbid) {
    glGenFramebuffers(1,&texture->fbid);
    if (!texture->fbid) return -1;
  }
  glBindFramebuffer(GL_FRAMEBUFFER,texture->fbid);
  glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,texture->texid,0);
  glBindFramebuffer(GL_FRAMEBUFFER,0);
  return 0;
}

/* Transform coords.
 */
 
void render_coords_fb_from_screen(struct render *render,int *x,int *y) {
  if ((render->outw>0)&&(render->outh>0)&&(render->texturec>=1)) {
    int fbw=render->texturev[0].w;
    int fbh=render->texturev[0].h;
    // Ensure when it's OOB for the window, we report OOB for the framebuffer, don't let it round off.
    if (*x<render->outx) *x=-1;
    else if (*x>=render->outx+render->outw) *x=fbw;
    else *x=((*x-render->outx)*fbw)/render->outw;
    if (*y<render->outy) *y=-1;
    else if (*y>=render->outy+render->outh) *y=fbh;
    else *y=((*y-render->outy)*fbh)/render->outh;
  }
}

void render_coords_screen_from_fb(struct render *render,int *x,int *y) {
  if ((render->outw>0)&&(render->outh>0)&&(render->texturec>=1)) {
    int fbw=render->texturev[0].w;
    int fbh=render->texturev[0].h;
    if ((fbw>0)&&(fbh>0)) {
      *x=render->outx+(*x*render->outw)/fbw;
      *y=render->outy+(*y*render->outh)/fbh;
    }
  }
}
