#include "jst.h"
#include <string.h>

/* Measure whitespace and comments.
 */
 
int jst_measure_space(struct jst_context *ctx,const char *src,int srcc) {
  int srcp=0;
  while (srcp<srcc) {
    if ((unsigned char)src[srcp]<=0x20) {
      srcp++;
      continue;
    }
    if (srcp>srcc-2) break;
    if (src[srcp]!='/') break;
    if (src[srcp+1]=='/') {
      srcp+=2;
      while ((srcp<srcc)&&(src[srcp++]!=0x0a)) ;
      continue;
    }
    if (src[srcp+1]=='*') {
      int openp=srcp;
      srcp+=2;
      for (;;) {
        if (srcp>srcc-2) return jst_error(ctx,src+openp,"Unclosed block comment.");
        if ((src[srcp]=='*')&&(src[srcp+1]=='/')) {
          srcp+=2;
          break;
        }
        srcp++;
      }
      int cmtc=srcp-openp;
      if ((cmtc==11)&&!memcmp(src+openp,"/*IGNORE{*/",11)) ctx->ignore=1;
      else if ((cmtc==11)&&!memcmp(src+openp,"/*}IGNORE*/",11)) ctx->ignore=0;
      continue;
    }
    break;
  }
  return srcp;
}

/* Measure string.
 * First character is the quote and we trust it blindly.
 */
 
static int jst_measure_string(struct jst_context *ctx,const char *src,int srcc) {
  int srcp=1;
  for (;;) {
    if (srcp>=srcc) return jst_error(ctx,src,"Unclosed string.");
    if (src[srcp]==src[0]) return srcp+1;
    if (src[srcp]=='\\') srcp+=2;
    else srcp+=1;
  }
}

/* Measure real tokens.
 */
 
int jst_measure_token(struct jst_context *ctx,const char *src,int srcc) {
  if (!src||(srcc<1)) return 0;
  
  /* Inline regex is problematic.
   * We know it's not "//" or "/*"; those would be picked off by jst_measure_space().
   * But is it a regex or a division operator?
   * We don't have a sense of primary vs secondary expression position.
   * Division can't happen after "(" or "=", and I think a literal regex would always be preceded by one of those.
   * Beyond that, a regex is just a string.
   */
  if (src[0]=='/') {
    if ((ctx->pvtokenc==1)&&(ctx->pvtoken[0]=='(')) return jst_measure_string(ctx,src,srcc);
    if ((ctx->pvtokenc==1)&&(ctx->pvtoken[0]=='=')) return jst_measure_string(ctx,src,srcc);
  }
  
  if ((src[0]=='"')||(src[0]=='\'')||(src[0]=='`')) return jst_measure_string(ctx,src,srcc);
  
  if ((src[0]>='0')&&(src[0]<='9')) {
    int srcp=1;
    while (srcp<srcc) {
      if (src[srcp]=='.') ;
      else if (jst_isident(src[srcp])) ;
      else break;
      srcp++;
    }
    return srcp;
  }
  
  if (jst_isident(src[0])) {
    int srcp=1;
    while ((srcp<srcc)&&jst_isident(src[srcp])) srcp++;
    return srcp;
  }
  
  // A real parser would want to check for multibyte operators, but we don't need to.
  return 1;
}

/* Test for reserved words.
 */

// Includes reserved words and other conditionally reserved things, the broadest set of things that shouldn't be used as identifiers.
// Also, oddly "constructor" and "window" were not in the list, but ought to be.
// https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Lexical_grammar
static const char *jst_keywords[]={
  "abstract",
  "arguments",
  "as",
  "async",
  "await",
  "boolean",
  "break",
  "byte",
  "case",
  "catch",
  "char",
  "class",
  "const",
  "constructor",
  "continue",
  "debugger",
  "default",
  "delete",
  "do",
  "double",
  "else",
  "enum",
  "eval",
  "export",
  "extends",
  "false",
  "final",
  "finally",
  "float",
  "for",
  "from",
  "function",
  "get",
  "goto",
  "if",
  "implements",
  "import",
  "in",
  "instanceof",
  "int",
  "interface",
  "let",
  "long",
  "native",
  "new",
  "null",
  "of",
  "package",
  "private",
  "protected",
  "public",
  "return",
  "set",
  "short",
  "static",
  "super",
  "switch",
  "synchronized",
  "this",
  "throw",
  "throws",
  "transient",
  "true",
  "try",
  "typeof",
  "var",
  "void",
  "volatile",
  "while",
  "window",
  "with",
  "yield",
};

int jst_is_keyword(const char *src,int srcc) {
  if (!src||(srcc<1)) return 0;
  int lo=0,hi=sizeof(jst_keywords)/sizeof(void*);
  while (lo<hi) {
    int ck=(lo+hi)>>1;
    int cmp=memcmp(src,jst_keywords[ck],srcc);
    if (!cmp) {
      if (jst_keywords[ck][srcc]) cmp=-1;
      else return 1;
    }
    if (cmp<0) hi=ck;
    else lo=ck+1;
  }
  return 0;
}

/* Resolve path with reference and relative parts.
 */
 
int jst_resolve_path(char *dst,int dsta,const char *pfx,int pfxc,const char *sfx,int sfxc) {
  if (!pfx) pfxc=0; else if (pfxc<0) { pfxc=0; while (pfx[pfxc]) pfxc++; }
  if (!sfx) sfxc=0; else if (sfxc<0) { sfxc=0; while (sfx[sfxc]) sfxc++; }
  
  // Empty and absolute paths are forbidden.
  if ((sfxc<1)||(sfx[0]=='/')) return -1;
  
  // Remove basename from (pfx).
  while (pfxc&&(pfx[pfxc-1]!='/')) pfxc--;
  
  // Resolve leading empty, dot, and double-dot components of (sfx).
  while (sfxc) {
    if (sfx[0]=='/') {
      sfxc--;
      sfx++;
      continue;
    }
    const char *comp=sfx;
    int compc=0;
    while ((compc<sfxc)&&(comp[compc]!='/')) compc++;
    if ((compc==1)&&(comp[0]=='.')) {
      sfx+=1;
      sfxc-=1;
      continue;
    }
    if ((compc==2)&&(comp[0]=='.')&&(comp[1]=='.')) {
      sfx+=2;
      sfxc-=2;
      while (pfxc&&(pfx[pfxc-1]=='/')) pfxc--;
      while (pfxc&&(pfx[pfxc-1]!='/')) pfxc--;
      continue;
    }
    break;
  }
  
  int dstc=pfxc+sfxc;
  if (dstc<=dsta) {
    memcpy(dst,pfx,pfxc);
    memcpy(dst+pfxc,sfx,sfxc);
    if (dstc<dsta) dst[dstc]=0;
  }
  return dstc;
}

/* Unique identifier based on a seed.
 */
 
int jst_identifier_from_sequence(char *dst,int dsta,int seq) {
  if (seq<0) return -1;
  
  // For the first 53 identifiers, exhaust single letters and underscore.
  if (seq<53) {
    if (dsta<1) return 1;
    if (seq<26) dst[0]='a'+seq;
    else if (seq<52) dst[0]='A'+seq-26;
    else seq='_';
    if (dsta>1) dst[1]=0;
    return 1;
  }
  seq-=53;
  
  /* After 53, use limited alphabets for the first two characters: [a-z_] [A-Z0-9_]
   * This reduces the chance of accidentally using a keyword or global.
   */
  int first=seq%27; seq/=27;
  int second=seq%37; seq/=37;
  if (dsta>=2) {
    dst[0]=(first<26)?('a'+first):'_';
    if (second<26) dst[1]='a'+second;
    else if (second<36) dst[1]='0'+second-26;
    else dst[1]='_';
  }
  int dstc=2;
  
  // For all remaining digits use the full alphabet [a-zA-Z0-9_]
  while (seq>0) {
    int digit=seq%63; seq/=63;
    if (dstc<dsta) {
      if (digit<26) dst[dstc]='a'+digit;
      else if (digit<52) dst[dstc]='A'+digit-26;
      else if (digit<62) dst[dstc]='0'+digit-52;
      else dst[dstc]='_';
    }
    dstc++;
  }
  
  if (dstc<dsta) dst[dstc]=0;
  return dstc;
}
