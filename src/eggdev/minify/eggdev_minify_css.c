/* eggdev_minify_css.c
 * There's not much CSS in Egg's HTMLs, so we're not going to be very careful about it.
 * Just eliminate whitespace where we can.
 */

#include "mf_internal.h"

/* Cleanup.
 */
 
void eggdev_minify_css_cleanup(struct eggdev_minify_css *ctx) {
}

/* Measure space.
 * No errors.
 * If we encounter an unclosed comment, we'll stop before it.
 */
 
static int mf_css_measure_space(const char *src,int srcc) {
  int srcp=0;
  while (srcp<srcc) {
    if ((unsigned char)src[srcp]<=0x20) {
      srcp++;
      continue;
    }
    if ((srcp<=srcc-2)&&(src[srcp]=='/')&&(src[srcp+1]=='*')) {
      const char *cmt=src+srcp;
      int cmtc=srcc-srcp;
      int cmtp=2;
      for (;;) {
        if (cmtp>cmtc-2) return srcp;
        if ((cmt[cmtp]=='*')&&(cmt[cmtp+1]=='/')) { cmtp+=2; break; }
        cmtp++;
      }
      srcp+=cmtp;
      continue;
    }
    break;
  }
  return srcp;
}

/* Nonzero if we need a space between these tokens.
 * The nature of our tokenization (NOT correct CSS tokenization!) is that there was always a space there to begin with.
 * CSS does require space in some places, so if we're not sure it's "yes".
 */
 
static int mf_css_requires_space(const char *a,int ac,const char *b,int bc) {
  if ((ac<1)||(bc<1)) return 0;
  
  // No need for spaces before or after ':' ';' '{' '}'
  switch (a[ac-1]) {
    case ':': case ';': case '{': case '}': return 0;
  }
  switch (b[0]) {
    case ':': case ';': case '{': case '}': return 0;
  }
  
  // Eliminate the last semicolon of a block.
  if ((a[ac-1]==';')&&(b[0]=='}')) return 0;
  
  // And assume everything else is necesssary.
  return 1;
}

/* Minify CSS, main entry point.
 */
 
int eggdev_minify_css(struct eggdev_minify_css *ctx) {
  int dstc0=ctx->dst->c;
  int srcp=0;
  for (;;) {
    srcp+=mf_css_measure_space(ctx->src+srcp,ctx->srcc-srcp);
    if (srcp>=ctx->srcc) break;
    const char *token=ctx->src+srcp;
    int tokenc=0;
    while ((srcp<ctx->srcc)&&((unsigned char)ctx->src[srcp++]>0x20)) tokenc++;
    if (tokenc<1) {
      fprintf(stderr,"%s:%d: Failed to read CSS token\n",ctx->srcpath,eggdev_lineno(ctx->src,srcp));
      return -2;
    }
    if ((tokenc>=2)&&(token[0]=='/')&&(token[0]=='*')) {
      fprintf(stderr,"%s:%d: Unclosed comment.\n",ctx->srcpath,eggdev_lineno(ctx->src,srcp));
      return -2;
    }
    if (mf_css_requires_space((char*)ctx->dst->v+dstc0,ctx->dst->c-dstc0,token,tokenc)) {
      if (sr_encode_u8(ctx->dst,' ')<0) return -1;
    }
    if (sr_encode_raw(ctx->dst,token,tokenc)<0) return -1;
  }
  return 0;
}
