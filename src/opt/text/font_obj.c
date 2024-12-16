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

int font_get_line_height(struct font *font) {
  return font->h;
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
 
static int font_page_add_image(struct font *font,struct font_page *page,const void *pixels,int w,int h,int stride,int handoff) {

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
  if (handoff) {
    page->v=(void*)pixels;
  } else {
    if (!(page->v=malloc(imglen))) return -1;
    memcpy(page->v,pixels,imglen);
  }
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
  int w,int h,int stride,
  int handoff
) {
  if (!font||!pixels||(w<1)||(h<1)) return -1;
  int minstride=(w+7)>>3;
  if (!stride) stride=minstride;
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
  
  int err=font_page_add_image(font,page,pixels,w,h,stride,handoff);
  if (err>=0) { // Confirm it didn't spill into the next page.
    if ((pagep+1<font->pagec)&&(page->codepoint+page->glyphc>page[1].codepoint)) {
      err=-1;
    }
  }
  if (err<0) {
    if (page->v&&!handoff) free(page->v);
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
  uint32_t id=(EGG_TID_image<<16)|imageid;
  int lo=0,hi=strings.resc;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
    uint32_t q=strings.resv[ck].id;
         if (id<q) hi=ck;
    else if (id>q) lo=ck;
    else {
      const void *src=strings.resv[ck].v;
      int srcc=strings.resv[ck].c;
      int w=0,h=0,pixelsize=0,pixlen;
      if ((pixlen=egg_image_decode_header(&w,&h,&pixelsize,src,srcc))<1) return -1;
      if (pixelsize!=1) return -1;
      void *pixels=malloc(pixlen);
      if (!pixels) return -1;
      if (egg_image_decode(pixels,pixlen,src,srcc)!=pixlen) {
        free(pixels);
        return -1;
      }
      if (font_add_image(font,codepoint,pixels,w,h,0,1)<0) {
        free(pixels);
        return -1;
      }
      return 0;
    }
  }
  return -1;
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
    if ((seqlen=text_utf8_decode(&codepoint,src+srcp,srcc-srcp))<1) { codepoint=(unsigned char)src[srcp]; seqlen=1; }
    srcp+=seqlen;
    w+=font_measure_glyph(font,codepoint);
  }
  return w;
}

/* Classify codepoint as letter, punctuation, or space, for line-breaking purposes.
 */
 
#define FONT_CHAR_SPACE 0
#define FONT_CHAR_LETTER 1
#define FONT_CHAR_PUNCT 2

static int font_classify_char(int ch) {
  if (ch<=0x20) return FONT_CHAR_SPACE;
  
  /* High code points are debatable.
   * I'm calling them letter because I reckon most punctuation is within G0, even for non-Roman languages.
   * Detailed classification of all of Unicode is I think out of the question.
   */
  if (ch>=0x80) return FONT_CHAR_LETTER;
  
  if ((ch>='a')&&(ch<='z')) return FONT_CHAR_LETTER;
  if ((ch>='A')&&(ch<='Z')) return FONT_CHAR_LETTER;
  if ((ch>='0')&&(ch<='9')) return FONT_CHAR_LETTER;
  if (ch=='_') return FONT_CHAR_LETTER; // Debatable.
  
  return FONT_CHAR_PUNCT;
}

/* Measure a token for line-breaking purposes.
 * Tokens consist of zero or more letters followed by zero or more punctuation.
 * This is 100% text, we're not measuring the glyphs' width.
 */
 
static int font_measure_token(const char *src,int srcc) {
  int srcp=0;
  int expect=FONT_CHAR_LETTER;
  while (srcp<srcc) {
    int ch,seqlen;
    if ((seqlen=text_utf8_decode(&ch,src+srcp,srcc-srcp))<1) { ch=(unsigned char)src[srcp]; seqlen=1; }
    int chcls=font_classify_char(ch);
    if (chcls!=expect) {
      if ((expect==FONT_CHAR_LETTER)&&(chcls==FONT_CHAR_PUNCT)) {
        expect=FONT_CHAR_PUNCT;
      } else {
        break;
      }
    }
    srcp+=seqlen;
  }
  return srcp;
}

/* Given a token (or whatever) too long for (wlimit), return the maximum length we can fit.
 * Implies that we are now breaking willy-nilly between any characters.
 * We may return something longer than (wlimit) only if it's a single glyph.
 */

static int font_break_token(int *tokenw,const struct font *font,const char *src,int srcc,int wlimit) {
  int srcp=0;
  *tokenw=0;
  while (srcp<srcc) {
    int ch,seqlen;
    if ((seqlen=text_utf8_decode(&ch,src+srcp,srcc-srcp))<1) { ch=(unsigned char)src[srcp]; seqlen=1; }
    int glyphw=font_measure_glyph(font,ch);
    if (!srcp||((*tokenw)+glyphw<=wlimit)) {
      srcp+=seqlen;
      (*tokenw)+=glyphw;
    } else {
      break;
    }
  }
  return srcp;
}

/* Break one line.
 * Returns the advancement in (src). >=*linec, but advancement includes trailing whitespace where linec and linew do not.
 * Returns <1 only when input is empty.
 */
 
static int font_break_line(int *linec,int *linew,const struct font *font,const char *src,int srcc,int wlimit) {
  int srcp=0;
  *linec=*linew=0;
  
  while (srcp<srcc) {
    int prelinec=*linec,prelinew=*linew,ch,seqlen,lf=0;
  
    // Consume whitespace even if it exceeds the limit, but stop after LF.
    while (srcp<srcc) {
      if ((seqlen=text_utf8_decode(&ch,src+srcp,srcc-srcp))<1) { ch=(unsigned char)src[srcp]; seqlen=1; }
      if (font_classify_char(ch)!=FONT_CHAR_SPACE) break;
      srcp+=seqlen;
      (*linec)+=seqlen;
      (*linew)+=font_measure_glyph(font,ch);
      if (ch==0x0a) { lf=1; break; }
    }
    
    // If we ate an LF or the whitespace crossed our width limit, strike the last spaces from the reported line and finish.
    if (lf||(srcp>=srcc)||(*linew>=wlimit)) {
      *linec=prelinec;
      *linew=prelinew;
      return srcp;
    }
  
    // Measure the next non-space token.
    int tokenc=font_measure_token(src+srcp,srcc-srcp);
    if (tokenc<1) tokenc=1; // ...it can't be empty but let's be sure.
    int tokenw=font_measure_line(font,src+srcp,tokenc);
    
    // If the token fits, keep it and proceed.
    if ((*linew)+tokenw<=wlimit) {
      srcp+=tokenc;
      (*linec)+=tokenc;
      (*linew)+=tokenw;
      continue;
    }
    
    // If we've consumed something already, even leading space, ignore the next token and stop here.
    if (srcp) break;
    
    // We have a token too long to fit on one line. Re-measure and break anywhere.
    // If the very first glyph is wider than the limit, report it as such. That's the only time we violate the limit.
    tokenc=font_break_token(&tokenw,font,src+srcp,tokenc,wlimit);
    srcp+=tokenc;
    (*linec)+=tokenc;
    (*linew)+=tokenw;
    break;
  }
  
  return srcp;
}

/* Break lines.
 */

int font_break_lines(struct font_line *dst,int dsta,const struct font *font,const char *src,int srcc,int wlimit) {
  if (!src) return 0;
  if (wlimit<1) return 0;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  int dstc=0,srcp=0;
  while (srcp<srcc) {
    int advc,linec,linew;
    if ((advc=font_break_line(&linec,&linew,font,src+srcp,srcc-srcp,wlimit))<1) return -1;
    if (dstc<dsta) {
      struct font_line *line=dst+dstc;
      line->v=src+srcp;
      line->c=linec;
      line->w=linew;
    }
    dstc++;
    srcp+=advc;
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
  int dstw=1;
  const struct font_line *qline=linev;
  int qi=linec;
  for (;qi-->0;qline++) {
    if (qline->w>dstw) dstw=qline->w;
  }
  int dsth=linec*font->h;
  if (dsth>hlimit) dsth=hlimit;
  if ((dstw<1)||(dsth<1)||(dstw>4096)||(dsth>4096)) {
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
