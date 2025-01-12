#include "eggdev_internal.h"
#include "jst.h"
#include "opt/fs/fs.h"
#include "opt/serial/serial.h"
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <limits.h>
#include <stdio.h>

static int jst_minify_internal(struct jst_context *ctx);

/* Cleanup.
 */
 
static void jst_ident_cleanup(struct jst_ident *ident) {
  if (ident->src) free(ident->src);
  if (ident->dst) free(ident->dst);
}

void jst_context_cleanup(struct jst_context *ctx) {
  if (ctx->importedv) {
    while (ctx->importedc-->0) free(ctx->importedv[ctx->importedc]);
    free(ctx->importedv);
  }
  while (ctx->identc>0) {
    ctx->identc--;
    jst_ident_cleanup(ctx->identv+ctx->identc);
  }
  memset(ctx,0,sizeof(struct jst_context));
}

/* Imports list.
 */
 
static int jst_importedv_has(const struct jst_context *ctx,const char *path) {
  int i=ctx->importedc;
  while (i-->0) {
    if (!strcmp(ctx->importedv[i],path)) return 1;
  }
  return 0;
}

static int jst_importedv_add(struct jst_context *ctx,const char *path,int pathc) {
  if (!path||(pathc<1)) return -1;
  if (ctx->importedc>=ctx->importeda) {
    int na=ctx->importeda+32;
    if (na>INT_MAX/sizeof(void*)) return -1;
    void *nv=realloc(ctx->importedv,sizeof(void*)*na);
    if (!nv) return -1;
    ctx->importedv=nv;
    ctx->importeda=na;
  }
  char *nv=malloc(pathc+1);
  if (!nv) return -1;
  memcpy(nv,path,pathc);
  nv[pathc]=0;
  ctx->importedv[ctx->importedc++]=nv;
  return 0;
}

/* Identifiers list.
 */
 
static struct jst_ident *jst_ident_search(const struct jst_context *ctx,const char *src,int srcc) {
  struct jst_ident *ident=ctx->identv;
  int i=ctx->identc;
  for (;i-->0;ident++) {
    if (srcc!=ident->srcc) continue;
    if (memcmp(src,ident->src,srcc)) continue;
    return ident;
  }
  return 0;
}

static struct jst_ident *jst_ident_new_kv(struct jst_context *ctx,const char *src,int srcc,const char *dst,int dstc) {
  if (ctx->identc>=ctx->identa) {
    int na=ctx->identa+128;
    if (na>INT_MAX/sizeof(struct jst_ident)) return 0;
    void *nv=realloc(ctx->identv,sizeof(struct jst_ident)*na);
    if (!nv) return 0;
    ctx->identv=nv;
    ctx->identa=na;
  }
  char *nsrc=malloc(srcc+1);
  if (!nsrc) return 0;
  memcpy(nsrc,src,srcc);
  nsrc[srcc]=0;
  char *ndst=malloc(dstc+1);
  if (!ndst) { free(nsrc); return 0; }
  memcpy(ndst,dst,dstc);
  ndst[dstc]=0;
  struct jst_ident *ident=ctx->identv+ctx->identc++;
  memset(ident,0,sizeof(struct jst_ident));
  ident->src=nsrc;
  ident->srcc=srcc;
  ident->dst=ndst;
  ident->dstc=dstc;
  return ident;
}

static struct jst_ident *jst_ident_new(struct jst_context *ctx,const char *src,int srcc) {
  char tmp[8];
  int tmpc=jst_identifier_from_sequence(tmp,sizeof(tmp),ctx->identc);
  if ((tmpc<1)||(tmpc>sizeof(tmp))) return 0;
  return jst_ident_new_kv(ctx,src,srcc,tmp,tmpc);
}

/* Receive one real token.
 * If it's an identifier and we determine it can be reduced, emit the reduced identifier.
 * Otherwise emit verbatim.
 */
 
static int jst_minify_token(struct jst_context *ctx,const char *token,int tokenc) {
  if (!token||(tokenc<1)) return -1;
  
  /* I often write bytes as "0xNN", 4 characters, and they can definitely use no more than 3 if we rephrase decimal.
   */
  if ((tokenc>=3)&&!memcmp(token,"0x",2)) {
    int v;
    if (sr_int_eval(&v,token,tokenc)>=2) {
      char tmp[16];
      int tmpc=sr_decsint_repr(tmp,sizeof(tmp),v);
      if ((tmpc>0)&&(tmpc<=sizeof(tmp))&&(tmpc<tokenc)) {
        if (ctx->dst->c&&jst_isident(((char*)ctx->dst->v)[ctx->dst->c-1])&&jst_isident(token[0])) {
          if (sr_encode_u8(ctx->dst,' ')<0) return -1;
        }
        if (sr_encode_raw(ctx->dst,tmp,tmpc)<0) return -1;
        return 0;
      }
    }
  }
  
  /* If it's a fractional literal number, lop off any trailing zeroes.
   */
  if ((token[0]>='0')&&(token[0]<='9')) {
    int isdecfloat=1,dot=0;
    int i=0; for (;i<tokenc;i++) {
      if (token[i]=='.') { dot=1; continue; }
      if ((token[i]>='0')&&(token[i]<='9')) continue;
      isdecfloat=0;
      break;
    }
    if (isdecfloat&&dot) {
      while (tokenc&&(token[tokenc-1]=='0')) tokenc--;
      if (tokenc&&(token[tokenc-1]=='.')) tokenc--;
    }
  }
  
  /* If we've marked this token for inline replacement, do that.
   */
  const struct jst_ident *ident=jst_ident_search(ctx,token,tokenc);
  if (ident) {
    token=ident->dst;
    tokenc=ident->dstc;
  }
  
  /* The general case, just emit the identifier.
   */
 _verbatim_:;
  if (ctx->dst->c&&jst_isident(((char*)ctx->dst->v)[ctx->dst->c-1])&&jst_isident(token[0])) {
    if (sr_encode_u8(ctx->dst,' ')<0) return -1;
  }
  if (sr_encode_raw(ctx->dst,token,tokenc)<0) return -1;
  return 0;
}

/* Measure and emit one string interpolation unit.
 * Input should start right *after* the "${", and we do not consume the terminating "}".
 */
 
static int jst_minify_interpolable_unit(struct jst_context *ctx,const char *src,int srcc) {
  int srcp=0,depth=0,err;
  for (;;) {
    if ((err=jst_measure_space(ctx,src+srcp,srcc-srcp))<0) return err;
    srcp+=err;
    if (srcp>=srcc) return -1;
    if (src[srcp]=='}') {
      if (!depth--) return srcp;
      if (sr_encode_u8(ctx->dst,'}')<0) return -1;
      srcp++;
    } else if (src[srcp]=='{') {
      depth++;
      if (sr_encode_u8(ctx->dst,'{')<0) return -1;
      srcp++;
    } else {
      const char *token=src+srcp;
      int tokenc=jst_measure_token(ctx,token,srcc-srcp);
      if (tokenc<1) return jst_error(ctx,token,"Failed to measure token.");
      if ((err=jst_minify_token(ctx,token,tokenc))<0) return err;
      srcp+=tokenc;
    }
  }
}

/* Emit a grave string with possible interpolation.
 */
 
static int jst_minify_interpolable_string(struct jst_context *ctx,const char *src,int srcc) {
  if (!src||(srcc<2)||(src[0]!='`')||(src[srcc-1]!='`')) return -1;
  src++;
  srcc-=2;
  int srcp=0,err;
  if (sr_encode_u8(ctx->dst,'`')<0) return -1;
  while (srcp<srcc) {
    if ((srcp<=srcc-2)&&(src[srcp]=='$')&&(src[srcp+1]=='{')) {
      if (sr_encode_raw(ctx->dst,"${",2)<0) return -1;
      srcp+=2;
      if ((err=jst_minify_interpolable_unit(ctx,src+srcp,srcc-srcp))<0) return err;
      srcp+=err;
      if ((srcp>=srcc)||(src[srcp]!='}')) return jst_error(ctx,src+srcp,"Expected '}' to close string interpolation unit.");
      srcp++;
      if (sr_encode_u8(ctx->dst,'}')<0) return -1;
    } else {
      if (sr_encode_u8(ctx->dst,src[srcp])<0) return -1;
      srcp++;
    }
  }
  if (sr_encode_u8(ctx->dst,'`')<0) return -1;
  return 0;
}

/* Swap out what's needed to enter alternate files.
 * Provide a blank jst_context at push, and the same at pop. It won't need cleaned up.
 */
 
static void jst_push(struct jst_context *tmp,struct jst_context *ctx,const char *src,int srcc,const char *refname) {
  tmp->src=ctx->src;
  tmp->srcc=ctx->srcc;
  tmp->refname=ctx->refname;
  ctx->src=src;
  ctx->srcc=srcc;
  ctx->refname=refname;
  ctx->pvtoken=0;
  ctx->pvtokenc=0;
}

static void jst_pop(struct jst_context *ctx,const struct jst_context *tmp) {
  ctx->src=tmp->src;
  ctx->srcc=tmp->srcc;
  ctx->refname=tmp->refname;
  ctx->pvtoken=0;
  ctx->pvtokenc=0;
}

/* Parse an import statement, resolve path, read file, reenter minification.
 * Input begin immediately after the 'import' token.
 * Returns consumed length of source.
 */
 
static int jst_import(struct jst_context *ctx,const char *src,int srcc) {

  /* First, validate the syntax, measure thru semicolon, and extract the relative path.
   */
  int srcp=0,err;
  const char *relpath=0;
  int relpathc=0;
  const char *expectv[]={"{",".","from","'",";"};
  int expectc=sizeof(expectv)/sizeof(void*);
  #define EXPECTNEXT { \
    if (!expectc) return -1; \
    expectc--; \
    memmove(expectv,expectv+1,sizeof(void*)*expectc); \
  }
  for (;;) {
    if ((err=jst_measure_space(ctx,src+srcp,srcc-srcp))<0) return jst_error(ctx,src+srcp,"Unspecified lexical error.");
    srcp+=err;
    if (srcp>=srcc) break;
    const char *token=src+srcp;
    int tokenc=jst_measure_token(ctx,token,srcc-srcp);
    if (tokenc<1) return jst_error(ctx,token,"Failed to measure next token.");
    srcp+=tokenc;
    
    const char *expect=expectv[0];
    if (expect[0]=='.') { // Inside curlies. No need to validate hard, just fail on "as", and advance on "}".
      if ((tokenc==2)&&!memcmp(token,"as",2)) return jst_error(ctx,token,"Renaming imports not permitted.");
      if ((tokenc==1)&&(token[0]=='}')) {
        EXPECTNEXT
        continue;
      }
      // Anything else should be a comma or identifier, and we ignore it.
      continue;
    }
    if (expect[0]=='\'') { // Require import path, here's the important part.
      if ((tokenc<2)||((token[0]!='"')&&(token[0]!='\''))) return jst_error(ctx,token,"Expected quoted string for import, before '%.*s'.",tokenc,token);
      relpath=token+1;
      relpathc=tokenc-2;
      EXPECTNEXT
      continue;
    }
    if (expect[0]==';') {
      if ((tokenc!=1)||(token[0]!=';')) return jst_error(ctx,token,"Expected ';' before '%.*s'.",tokenc,token);
      EXPECTNEXT
      break;
    }
    if (memcmp(expect,token,tokenc)||expect[tokenc]) {
      return jst_error(ctx,token,"Expected '%s' before '%.*s'.",expect,tokenc,token);
    }
    EXPECTNEXT
  }
  #undef EXPECTNEXT
  if (expectc) return jst_error(ctx,src,"Malformed import.");
  if (!relpath||!relpathc) return jst_error(ctx,src,"Malformed import.");
  
  /* Determine full path and check list of prior imports.
   */
  char path[1024];
  int pathc=jst_resolve_path(path,sizeof(path),ctx->refname,-1,relpath,relpathc);
  if ((pathc<1)||(pathc>=sizeof(path))) return jst_error(ctx,relpath,"Failed to resolve import path.");
  if (jst_importedv_has(ctx,path)) return srcp;
  if (jst_importedv_add(ctx,path,pathc)<0) return -1;
  
  /* Reenter for the imported file.
   */
  char *subsrc=0;
  int subsrcc=file_read(&subsrc,path);
  if (subsrcc<0) return jst_error(ctx,relpath,"%s: Failed to read file.",path);
  struct jst_context tmp={0};
  jst_push(&tmp,ctx,subsrc,subsrcc,path);
  err=jst_minify_internal(ctx);
  jst_pop(ctx,&tmp);
  if (err<0) return err;
  free(subsrc);
  return srcp;
}

/* Parse a "const" declaration.
 */
 
static int jst_const(struct jst_context *ctx,const char *src,int srcc) {

  // If the previous token was an open paren, it must be "for (const abc of ...)". Emit "const" and return.
  if ((ctx->pvtokenc==1)&&(ctx->pvtoken[0]=='(')) {
    if (sr_encode_raw(ctx->dst,"const",5)<0) return -1;
    return 0;
  }

  const char *token;
  int srcp=0,err,started=0,tokenc;
  #define NEXT { \
    if ((err=jst_measure_space(ctx,src+srcp,srcc-srcp))<0) return jst_error(ctx,src+srcp,"Unspecified lexical error."); \
    srcp+=err; \
    if (srcp>=srcc) return jst_error(ctx,src+srcp,"Expected ';' to end 'const' declaration."); \
    token=src+srcp; \
    tokenc=jst_measure_token(ctx,token,srcc-srcp); \
    if (tokenc<1) return jst_error(ctx,token,"Failed to measure next token."); \
    srcp+=tokenc; \
  }
  for (;;) {
  
    // First token must be an identifier or semicolon.
    NEXT
    if ((tokenc==1)&&(token[0]==';')) break;
    const char *k=token;
    int kc=tokenc;
    
    // Actually, the first token can also be '[' or '{' for destructuring assignment. Don't touch those.
    if (!started&&(kc==1)&&((k[0]=='[')||(k[0]=='{'))) {
      if (sr_encode_raw(ctx->dst,"const",5)<0) return -1;
      return 0;
    }
    
    // Next must be '='.
    NEXT
    if ((tokenc!=1)||(token[0]!='=')) return jst_error(ctx,token,"Expected '=' before '%.*s'",tokenc,token);
    
    // Next is the value, which can be any expression.
    // Optimistically assume that it will be one token.
    NEXT
    const char *v=token;
    int vc=tokenc;
    
    // Now pull the even nexter token.
    // If it's comma or semicolon, we can consider inlining the constant.
    // Consider both key and value length for inlining. eg `const pi = 3.14159`, no sense inlining because "pi" is shorter.
    NEXT
    const char *far=token;
    int farc=tokenc;
    if ((farc==1)&&((far[0]==',')||(far[0]==';'))&&(vc<kc)) {
      struct jst_ident *ident=jst_ident_new_kv(ctx,k,kc,v,vc);
      if (!ident) return -1;
      if (far[0]==';') break;
      continue;
    }
    
    // Emit either "const" or a comma.
    if (started) {
      if (sr_encode_u8(ctx->dst,',')<0) return -1;
    } else {
      // No need to check for preceding identifier, I think. Start of a statement must have semicolon or '}' behind it.
      if (sr_encode_raw(ctx->dst,"const ",6)<0) return -1;
      started=1;
    }
    
    // Emit key, equal, and whatever "v" is.
    if (sr_encode_raw(ctx->dst,k,kc)<0) return -1;
    if (sr_encode_u8(ctx->dst,'=')<0) return -1;
    if (jst_minify_token(ctx,v,vc)<0) return -1;
    
    int depth=0;
    if ((vc==1)&&((v[0]=='(')||(v[0]=='{')||(v[0]=='['))) depth++;
    
    // "far", the token after "v", could still be a terminator (if v is longer than k). Check and handle that.
    // Actually, make a loop of it, and emit "far" until we find the terminator.
    for (;;) {
      if (depth&&(farc==1)&&((far[0]==')')||(far[0]=='}')||(far[0]==']'))) {
        depth--;
      } else if ((farc==1)&&((far[0]=='(')||(far[0]=='{')||(far[0]=='['))) {
        depth++;
      } else if (!depth) {
        if ((farc==1)&&(far[0]==',')) break;
        if ((farc==1)&&(far[0]==';')) break;
      }
      if (jst_minify_token(ctx,far,farc)<0) return -1;
      NEXT
      far=token;
      farc=tokenc;
    }
    if ((farc==1)&&(far[0]==';')) break;
  }
  if (started) {
    if (sr_encode_u8(ctx->dst,';')<0) return -1;
  }
  #undef NEXT
  return srcp;
}

/* Minify, top level.
 */

int jst_minify(struct sr_encoder *dst,struct jst_context *ctx,const char *src,int srcc,const char *refname) {
  int err;
  if (!dst||!ctx) return -1;
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  if (!refname) refname="<unknown>";
  ctx->startp=dst->c;
  ctx->dst=dst;
  ctx->src=src;
  ctx->srcc=srcc;
  ctx->refname=refname;
  ctx->pvtokenc=0;
  if ((err=jst_minify_internal(ctx))<0) return err;
  return 0;
}

static int jst_minify_internal(struct jst_context *ctx) {
  int dstc0=ctx->dst->c;
  const char *src=ctx->src;
  int srcc=ctx->srcc;
  int srcp=0,err;
  for (;;) {
    if ((err=jst_measure_space(ctx,src+srcp,srcc-srcp))<0) return jst_error(ctx,src+srcp,"Unspecified lexical error.");
    srcp+=err;
    if (srcp>=srcc) break;
    const char *token=src+srcp;
    int tokenc=jst_measure_token(ctx,token,srcc-srcp);
    if (tokenc<1) return jst_error(ctx,token,"Failed to measure next token.");
    srcp+=tokenc;
    
    if (ctx->ignore) continue;
    
    if ((tokenc==6)&&!memcmp(token,"import",6)) {
      // At "import", inline the referenced file recursively.
      if ((err=jst_import(ctx,src+srcp,srcc-srcp))<0) return jst_error(ctx,token,"Unspecified error resolving import.");
      srcp+=err;
      
    } else if ((tokenc==6)&&!memcmp(token,"export",6)) {
      // "export" is meaningless to us, since we're concatenating all imports.
      
    } else if ((tokenc==5)&&!memcmp(token,"const",5)) {
      // Pull out any "const" declarations whose values are scalars smaller than the label; we'll inline them.
      if ((err=jst_const(ctx,src+srcp,srcc-srcp))<0) return jst_error(ctx,token,"Unspecified error evaluating 'const' declaration.");
      srcp+=err;
      
    } else if ((tokenc>=2)&&(token[0]=='`')) {
      // Interpolable strings, we need to enter each "${...}" unit.
      if ((err=jst_minify_interpolable_string(ctx,token,tokenc))<0) return jst_error(ctx,token,"Unspecified error inside string.");
      
    } else {
      // Everything else, check for 1:1 token reduction or emit verbatim.
      if ((err=jst_minify_token(ctx,token,tokenc))<0) return jst_error(ctx,token,"Unspecified error.");
      
    }
    ctx->pvtoken=token;
    ctx->pvtokenc=tokenc;
  }
  // Output a newline at the end of each file. It's not needed, but it's cheap I feel goes a long way for traceability.
  if (ctx->dst->c>dstc0) { // ...but only if we actually emitted something.
    if (sr_encode_u8(ctx->dst,0x0a)<0) return -1;
  }
  // IGNORE blocks implicitly close at end of file.
  ctx->ignore=0;
  return 0;
}

/* Log error.
 */
 
int jst_error(struct jst_context *ctx,const char *src,const char *fmt,...) {
  if (ctx->logged_error) return -2;
  ctx->logged_error=1;
  if ((src>=ctx->src)&&(src<=ctx->src+ctx->srcc)) {
    fprintf(stderr,"%s:%d: ",ctx->refname,eggdev_lineno(ctx->src,src-ctx->src));
  } else {
    fprintf(stderr,"%s: ",ctx->refname);
  }
  va_list vargs;
  va_start(vargs,fmt);
  vfprintf(stderr,fmt,vargs);
  fprintf(stderr,"\n");
  return -2;
}
