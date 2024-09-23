#include "text_internal.h"

/* Delete.
 */

void font_del(struct font *font) {
  if (!font) return;
  if (font->pagev) {
    while (font->pagec-->0) {
      struct font_page *page=font->pagev+font->pagec;
      if (page->v) free(page->v);
      if (page->glyphv) free(page->glyphv);
    }
    free(font->pagev);
  }
  free(font);
}

/* New.
 */

struct font *font_new() {
  struct font *font=calloc(1,sizeof(struct font));
  if (!font) return 0;
  return font;
}

/* Trivial accessors.
 */

void font_set_replacement(struct font *font,int codepoint) {
  font->replacement=codepoint;
}

/* Search pages.
 */
 
static int font_pagev_search(const struct font *font,int codepoint) {
  int lo=0,hi=font->pagec;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
    const struct font_page *page=font->pagev+ck;
         if (codepoint<page->codepoint) hi=ck;
    else if (codepoint>=page->codepoint+page->glyphc) lo=ck+1;
    else return ck;
  }
  return -lo-1;
}

/* Is any pixel in this rect on?
 * Caller ensures valid bounds.
 */
 
static int font_content_nonzero(const uint8_t *src,int stride,int x,int y,int w,int h) {
  src+=y*stride+(x>>3);
  uint8_t mask0=0x80>>(x&7);
  for (;h-->0;src+=stride) {
    const uint8_t *p=src;
    uint8_t mask=mask0;
    int xi=w;
    for (;xi-->0;) {
      if ((*p)&mask) return 1;
      if (mask==1) { mask=0x80; p++; }
      else mask>>=1;
    }
  }
  return 0;
}

/* Append glyph to page.
 */
 
static int font_page_add_glyph(struct font_page *page,int x,int y,int w) {
  if (page->glyphc>=page->glypha) {
    int na=page->glypha+32;
    if (na>INT_MAX/sizeof(struct font_glyph)) return -1;
    void *nv=realloc(page->glyphv,sizeof(struct font_glyph)*na);
    if (!nv) return -1;
    page->glyphv=nv;
    page->glypha=na;
  }
  struct font_glyph *glyph=page->glyphv+page->glyphc++;
  glyph->x=x;
  glyph->y=y;
  glyph->w=w;
  return 0;
}

/* Populate new page with image.
 * (page) must be zero except (codepoint).
 * We only touch (font) to set (h) if new, or to validate against it.
 * Caller must validate image.
 */
 
static int font_page_add_image(struct font *font,struct font_page *page,const void *pixels,int w,int h,int stride) {

  // If the font's row height is not yet established, take it from the Control Column of input.
  if (!font->h) {
    const uint8_t *srcrow=pixels;
    int y=0;
    if ((*srcrow)&0x80) return -1; // Top-left pixel must be zero (ie a Control Row).
    srcrow+=stride;
    y++;
    while ((y<h)&&((*srcrow)&0x80)) {
      font->h++;
      y++;
      srcrow+=stride;
    }
    if (!font->h) return -1; // No content in top ribbon.
  }
  
  // Now we have the global row height. Leftmost column must be zero followed by (h) ones, repeating to the end.
  if (h%(1+font->h)) return -1; // Excess height forbidden.
  int rowc=h/(1+font->h);
  if (rowc<1) return -1;
  const uint8_t *srcrow=pixels;
  int i=rowc;
  while (i-->0) {
    if ((*srcrow)&0x80) return -1;
    srcrow+=stride;
    int ii=font->h;
    while (ii-->0) {
      if (!((*srcrow)&0x80)) return -1;
      srcrow+=stride;
    }
  }
  
  // Copy the raw image.
  int imglen=stride*h;
  if (!(page->v=malloc(imglen))) return -1;
  memcpy(page->v,pixels,imglen);
  page->w=w;
  page->h=h;
  page->stride=stride;
  
  // Control Column looks kosher, so let's read it glyphwise.
  srcrow=pixels;
  int longstride=stride*(1+font->h);
  int y=1;
  for (i=rowc;i-->0;srcrow+=longstride,y+=1+font->h) {
    const uint8_t *srcp=srcrow; // Control Row.
    uint8_t srcmask=0x20;
    int x=2;
    uint8_t gbit=(*srcp)&0x40; // 'g' prefix are anchored at the current glyph.
    int gx=1;
    for (;x<w;x++) {
      if (
        (((*srcp)&srcmask)&&!gbit)||
        (!((*srcp)&srcmask)&&gbit)
      ) {
        if (font_page_add_glyph(page,gx,y,x-gx)<0) return -1;
        gbit=!gbit;
        gx=x;
      }
      if (srcmask==1) { srcmask=0x80; srcp++; }
      else srcmask>>=1;
    }
    if (font_content_nonzero(pixels,stride,gx,y,x-gx,font->h)) {
      if (font_page_add_glyph(page,gx,y,x-gx)<0) return -1;
    }
  }
  
  return 0;
}

/* Add image.
 */

int font_add_image(
  struct font *font,
  int codepoint,
  const void *pixels,
  int w,int h,int stride
) {
  if (!font||!pixels||(w<1)||(h<1)) return -1;
  int minstride=(w+7)>>3;
  if (stride) stride=minstride;
  else if (stride<minstride) return -1;
  
  int pagep=font_pagev_search(font,codepoint);
  if (pagep>=0) return -1;
  pagep=-pagep-1;
  if (font->pagec>=font->pagea) {
    int na=font->pagea+8;
    if (na>INT_MAX/sizeof(struct font_page)) return -1;
    void *nv=realloc(font->pagev,sizeof(struct font_page)*na);
    if (!nv) return -1;
    font->pagev=nv;
    font->pagea=na;
  }
  struct font_page *page=font->pagev+pagep;
  memmove(page+1,page,sizeof(struct font_page)*(font->pagec-pagep));
  font->pagec++;
  memset(page,0,sizeof(struct font_page));
  page->codepoint=codepoint;
  
  int err=font_page_add_image(font,page,pixels,w,h,stride);
  if (err>=0) { // Confirm it didn't spill into the next page.
    if ((pagep+1<font->pagec)&&(page->codepoint+page->glyphc>page[1].codepoint)) {
      err=-1;
    }
  }
  if (err<0) {
    if (page->v) free(page->v);
    if (page->glyphv) free(page->glyphv);
    font->pagec--;
    memmove(page,page+1,sizeof(struct font_page)*(font->pagec-pagep));
    return -1;
  }
  return 0;
}

/* Add image from resource.
 */

int font_add_image_resource(struct font *font,int codepoint,int imageid) {
  #if USE_image
    uint32_t id=(EGG_TID_image<<16)|imageid;
    int lo=0,hi=strings.resc;
    while (lo<hi) {
      int ck=(lo+hi)>>1;
      uint32_t q=strings.resv[ck].id;
           if (id<q) hi=ck;
      else if (id>q) lo=ck;
      else {
        struct image *image=image_decode(strings.resv[ck].v,strings.resv[ck].c);
        if (!image) break;
        int err=image_reformat_in_place(image,1,0,0,0,0);
        if (err>=0) {
          err=font_add_image(font,codepoint,image->v,image->w,image->h,image->stride);
        }
        image_del(image);
        return err;
      }
    }
  #endif
  return -1;
}

/* Character properties.
 */
 
static int font_codepoint_is_breakable(int codepoint) {
  if ((codepoint>='0')&&(codepoint<='9')) return 0;
  if ((codepoint>='a')&&(codepoint<='z')) return 0;
  if ((codepoint>='A')&&(codepoint<='Z')) return 0;
  // Non-ASCII letters are all breaking points but really shouldn't be. TODO?
  return 1;
}

/* Measure glyph.
 */
 
static int font_measure_glyph(const struct font *font,int codepoint) {
  int pagep=font_pagev_search(font,codepoint);
  if (pagep<0) return 0;
  const struct font_page *page=font->pagev+pagep;
  const struct font_glyph *glyph=page->glyphv+(codepoint-page->codepoint);
  return glyph->w;
}

/* Measure line.
 */

int font_measure_line(const struct font *font,const char *src,int srcc) {
  if (!src) return 0;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  int srcp=0,w=0;
  while (srcp<srcc) {
    int seqlen,codepoint;
    if ((seqlen=text_utf8_decode(&codepoint,src+srcp,srcc-srcp))<1) break;
    srcp+=seqlen;
    w+=font_measure_glyph(font,codepoint);
  }
  return w;
}

/* Break lines.
 */

int font_break_lines(struct font_line *dst,int dsta,const struct font *font,const char *src,int srcc,int wlimit) {
  if (!src) return 0;
  if (wlimit<1) return 0;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  int dstc=0,srcp=0;
  while (srcp<srcc) {
    const char *line=src+srcp;
    int linec=0;
    int linea=srcc-srcp;
    int linew=0;
    
    // If there's an explicit LF in the future, that's our limit (inclusive).
    int i=0; for (;i<linea;i++) {
      if (line[i]==0x0a) {
        linea=i+1;
        break;
      }
    }
    
    /* Consume free space and atoms until the line with the next atom would be too long.
     * When that's found, break between the free space and atom.
     * If the atom is at the line's start, repeat the process one codepoint at a time.
     */
    while (linec<linea) {
      int freec=0;
      while ((linec+freec<linea)&&((unsigned char)line[linec+freec]<=0x20)) freec++;
      int freew=font_measure_line(font,line+linec,freec);
      linec+=freec;
      linew+=freew;
      if (linew>=wlimit) break;
      const char *atom=line+linec;
      int atomc=0,atomw=0;
      while (linec+atomc<linea) {
        int seqlen,codepoint;
        if ((seqlen=text_utf8_decode(&codepoint,line+linec+atomc,linea-atomc-linec))<1) {
          srcc=srcp+linec+atomc;
          break;
        }
        if (atomc&&font_codepoint_is_breakable(codepoint)) break;
        atomc+=seqlen;
        atomw+=font_measure_glyph(font,codepoint);
      }
      if (linew&&(linew+atomw>wlimit)) break; // reject atom, pick it up next line.
      linec+=atomc;
      linew+=atomw;
    }
    
    // (linec) should be nonzero by this point. Might be zero due to a misencode or something? Stop then.
    if (!linec) break;
    
    // Advance (srcp), then trim trailing whitespace, then record the line.
    srcp+=linec;
    while (linec&&((unsigned char)line[linec-1]<=0x20)) linec--;
    if (dstc<dsta) {
      dst[dstc].v=line;
      dst[dstc].c=linec;
      dst[dstc].w=linew;
    }
    dstc++;
  }
  return dstc;
}

/* Copy from A1 bitmap to RGBA bitmap.
 * We clip output bounds but not input, those are presumed valid.
 */
 
static void font_copy_bits(
  uint8_t *dst,int dstw,int dsth,int dststride,int dstx,int dsty,
  const uint8_t *src,int srcw,int srch,int srcstride,int srcx,int srcy,
  int w,int h,
  uint32_t rgba
) {
  if (dstx<0) { w+=dstx; srcx-=dstx; dstx=0; }
  if (dsty<0) { h+=dsty; srcy-=dsty; dsty=0; }
  if (dstx>dstw-w) w=dstw-dstx;
  if (dsty>dsth-h) h=dsth-dsty;
  if ((w<1)||(h<1)) return;
  uint8_t r=rgba>>24,g=rgba>>16,b=rgba>>8,a=rgba;
  dst+=dststride*dsty+(dstx<<2);
  src+=srcstride*srcy+(srcx>>3);
  uint8_t srcmask0=0x80>>(srcx&7);
  int yi=h;
  for (;yi-->0;dst+=dststride,src+=srcstride) {
    uint8_t *dstp=dst;
    const uint8_t *srcp=src;
    uint8_t srcmask=srcmask0;
    int xi=w;
    for (;xi-->0;dstp+=4) {
      if ((*srcp)&srcmask) {
        dstp[0]=r;
        dstp[1]=g;
        dstp[2]=b;
        dstp[3]=a;
      }
      if (srcmask==1) { srcmask=0x80; srcp++; }
      else srcmask>>=1;
    }
  }
}

/* Render glyph.
 */

int font_render_glyph(
  void *dst,int dstw,int dsth,int dststride,int dstx,int dsty,
  const struct font *font,
  int codepoint,
  uint32_t rgba
) {
  int pagep=font_pagev_search(font,codepoint);
  if (pagep<0) return 0;
  const struct font_page *page=font->pagev+pagep;
  const struct font_glyph *glyph=page->glyphv+(codepoint-page->codepoint);
  font_copy_bits(dst,dstw,dsth,dststride,dstx,dsty,page->v,page->w,page->h,page->stride,glyph->x,glyph->y,glyph->w,font->h,rgba);
  return glyph->w;
}

/* Render one-line string.
 */

int font_render_string(
  void *dst,int dstw,int dsth,int dststride,int dstx,int dsty,
  const struct font *font,
  const char *src,int srcc,
  uint32_t rgba
) {
  if (!src) return 0;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  int srcp=0;
  int dstx0=dstx;
  while (srcp<srcc) {
    int seqlen,codepoint;
    if ((seqlen=text_utf8_decode(&codepoint,src+srcp,srcc-srcp))<1) break;
    srcp+=seqlen;
    int dx=font_render_glyph(dst,dstw,dsth,dststride,dstx,dsty,font,codepoint,rgba);
    if (dx<0) break;
    dstx+=dx;
  }
  return dstx-dstx0;
}

/* Render to new texture.
 */

int font_tex_oneline(const struct font *font,const char *src,int srcc,int wlimit,uint32_t rgba) {
  if (!font||!src) return -1;
  if (font->h<1) return -1;
  if (wlimit<1) return -1;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  if (!srcc) return -1;
  int dstw=font_measure_line(font,src,srcc);
  if (dstw>wlimit) dstw=wlimit;
  int dsth=font->h;
  void *dst=calloc(dstw*dsth,4);
  if (!dst) return -1;
  font_render_string(dst,dstw,dsth,dstw<<2,0,0,font,src,srcc,rgba);
  int texid=egg_texture_new();
  if ((texid<0)||(egg_texture_load_raw(texid,EGG_TEX_FMT_RGBA,dstw,dsth,dstw<<2,dst,dstw*dsth*4)<0)) {
    free(dst);
    return -1;
  }
  free(dst);
  return texid;
}

int font_tex_multiline(const struct font *font,const char *src,int srcc,int wlimit,int hlimit,uint32_t rgba) {
  if (!font||!src) return -1;
  if (font->h<1) return -1;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  if (!srcc) return -1;
  int linea=(hlimit+font->h-1)/font->h;
  if (linea<1) return -1;
  struct font_line *linev=malloc(sizeof(struct font_line)*linea);
  if (!linev) return -1;
  int linec=font_break_lines(linev,linea,font,src,srcc,wlimit);
  if (linec<0) linec=0;
  else if (linec>linea) linec=linea;
  int dstw=wlimit;
  int dsth=linec*font->h;
  if (dsth>hlimit) dsth=hlimit;
  if ((dstw>4096)||(dsth>4096)) {
    free(linev);
    return -1;
  }
  void *dst=calloc(dstw*dsth,4);
  if (!dst) {
    free(linev);
    return -1;
  }
  const struct font_line *line=linev;
  int i=linec,y=0;
  for (;i-->0;line++,y+=font->h) {
    font_render_string(dst,dstw,dsth,dstw<<2,0,y,font,line->v,line->c,rgba);
  }
  free(linev);
  int texid=egg_texture_new();
  if ((texid<0)||(egg_texture_load_raw(texid,EGG_TEX_FMT_RGBA,dstw,dsth,dstw<<2,dst,dstw*dsth*4)<0)) {
    free(dst);
    return -1;
  }
  free(dst);
  return texid;
}

int font_texres_oneline(const struct font *font,int rid,int p,int wlimit,uint32_t rgba) {
  const char *src=0;
  int srcc=strings_get(&src,rid,p);
  if (srcc<1) return -1;
  return font_tex_oneline(font,src,srcc,wlimit,rgba);
}

int font_texres_multiline(const struct font *font,int rid,int p,int wlimit,int hlimit,uint32_t rgba) {
  const char *src=0;
  int srcc=strings_get(&src,rid,p);
  if (srcc<1) return -1;
  return font_tex_multiline(font,src,srcc,wlimit,hlimit,rgba);
}
