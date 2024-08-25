#include "eggdev_internal.h"
#include "eggdev_html.h"
#include <stdarg.h>

/* Report error.
 */
 
static int eggdev_html_error(struct eggdev_html_reader *reader,const char *fmt,...) {
  if (!reader->refname||!fmt||!fmt[0]) return -1;
  fprintf(stderr,"%s:%d: ",reader->refname,eggdev_lineno(reader->src,reader->srcp));
  va_list vargs;
  va_start(vargs,fmt);
  vfprintf(stderr,fmt,vargs);
  fprintf(stderr,"\n");
  return -2;
}

/* Init.
 */
 
void eggdev_html_reader_init(struct eggdev_html_reader *reader,const void *src,int srcc,const char *refname) {
  if (!src||(srcc<0)||(srcc&&!src)) srcc=0;
  memset(reader,0,sizeof(struct eggdev_html_reader));
  reader->src=src;
  reader->srcc=srcc;
  reader->refname=refname;
}

/* Next expression.
 */

int eggdev_html_reader_next(void *dstpp,struct eggdev_html_reader *reader) {
  if (reader->srcp>=reader->srcc) return 0;
  const char *token=reader->src+reader->srcp;
  int tokena=reader->srcc-reader->srcp;
  
  // Plain text.
  if (token[0]!='<') {
    int tokenc=1;
    while ((tokenc<tokena)&&(token[tokenc]!='<')) tokenc++;
    reader->srcp+=tokenc;
    *(const void**)dstpp=token;
    return tokenc;
  }
  
  // Comment.
  if ((tokena>=4)&&!memcmp(token,"<!--",4)) {
    int tokenc=4;
    for (;;) {
      if (tokenc>tokena-3) return eggdev_html_error(reader,"Unlcosed comment.");
      if (!memcmp(token+tokenc,"-->",3)) {
        tokenc+=3;
        break;
      }
      tokenc++;
    }
    reader->srcp+=tokenc;
    *(const void**)dstpp=token;
    return tokenc;
  }
  
  // DOCTYPE, PI, OPEN, CLOSE, SINGLETON: Terminate at '>'.
  int tokenc=1;
  for (;;) {
    if (tokenc>=tokena) return eggdev_html_error(reader,"Unclosed tag.");
    if (token[tokenc++]=='>') break;
  }
  reader->srcp+=tokenc;
  *(const void**)dstpp=token;
  
  // If it's an OPEN or CLOSE tag, apply to the stack.
  switch (eggdev_html_expression_type(token,tokenc)) {
    case EGGDEV_HTML_EXPR_OPEN: {
        if (reader->tagc>=EGGDEV_HTML_DEPTH_LIMIT) {
          return eggdev_html_error(reader,"HTML depth limit %d exceeded.",EGGDEV_HTML_DEPTH_LIMIT);
        }
        struct eggdev_html_tag *tag=reader->tagv+reader->tagc++;
        if ((tag->namec=eggdev_html_get_tag_name(&tag->name,token,tokenc))<1) {
          return eggdev_html_error(reader,"Malformed open tag.");
        }
      } break;
    case EGGDEV_HTML_EXPR_CLOSE: {
        const char *name=0;
        int namec=eggdev_html_get_tag_name(&name,token,tokenc);
        if (namec<1) return eggdev_html_error(reader,"Malformed close tag.");
        if (reader->tagc<1) {
          return eggdev_html_error(reader,"Unexpected closing tag </%.*s>",namec,name);
        }
        const struct eggdev_html_tag *tag=reader->tagv+(--(reader->tagc));
        if ((tag->namec!=namec)||memcmp(tag->name,name,namec)) {
          return eggdev_html_error(reader,"Unexpected closing tag </%.*s>, expected </%.*s>",namec,name,tag->namec,tag->name);
        }
      } break;
  }
  
  return tokenc;
}

/* Get expression type.
 */
 
int eggdev_html_expression_type(const char *src,int srcc) {
  if (!src) return EGGDEV_HTML_EXPR_SPACE;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  if (!srcc) return EGGDEV_HTML_EXPR_SPACE;
  
  // Starts with anything other than '<', it's TEXT or SPACE.
  if (src[0]!='<') {
    for (;srcc-->0;src++) {
      if ((unsigned char)(*src)>0x20) return EGGDEV_HTML_EXPR_TEXT;
    }
    return EGGDEV_HTML_EXPR_SPACE;
  }
  
  // COMMENT and CLOSE are identifiable by the second character.
  if (srcc>=2) {
    if (src[1]=='!') return EGGDEV_HTML_EXPR_COMMENT;
    if (src[1]=='/') return EGGDEV_HTML_EXPR_CLOSE;
  }
  
  // Skip to the end and look for the trailing slash to distinguish SINGLETON from OPEN.
  int p=srcc;
  while (p&&(src[--p]!='>')) ;
  if (src[p]=='>') {
    while (p--) {
      if (src[p]=='/') return EGGDEV_HTML_EXPR_SINGLETON;
      if ((unsigned char)src[p]>0x20) break;
    }
  }
  return EGGDEV_HTML_EXPR_OPEN;
}

/* Close the tag on top of stack.
 */

int eggdev_html_close(void *dstpp,struct eggdev_html_reader *reader) {
  if (reader->tagc<1) return eggdev_html_error(reader,"No open tags.");
  
  // Beware, tag may be overwritten as we work. (tag->name) is safe to retain.
  const struct eggdev_html_tag *tag=reader->tagv+(reader->tagc-1);
  const char *name=tag->name;
  int namec=tag->namec;
  int tagc0=reader->tagc-1;
  
  const char *expr;
  int exprc;
  const char *dst=reader->src+reader->srcp;
  while ((exprc=eggdev_html_reader_next(&expr,reader))>0) {
    if (reader->tagc!=tagc0) continue;
    if (eggdev_html_expression_type(expr,exprc)!=EGGDEV_HTML_EXPR_CLOSE) continue;
    const char *q=0;
    int qc=eggdev_html_get_tag_name(&q,expr,exprc);
    if ((qc!=namec)||memcmp(q,name,namec)) return eggdev_html_error(reader,"Unexpected closing tag </%.*s>, expected </%.*s>",qc,q,namec,name);
    *(const char**)dstpp=dst;
    return (int)(expr-dst);
  }
  return eggdev_html_error(reader,"Closing tag </%.*s> not found.",namec,name);
}

/* Tag name from text.
 */
 
int eggdev_html_get_tag_name(void *dstpp,const char *src,int srcc) {
  if (!src) return 0;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  if ((srcc<1)||(src[0]!='<')) return 0;
  int srcp=1;
  while (srcp<srcc) {
    if ((unsigned char)src[srcp]<=0x20) { srcp++; continue; }
    if (src[srcp]=='/') { srcp++; continue; }
    break;
  }
  *(const void**)dstpp=src+srcp;
  int dstc=0;
  while (srcp<srcc) {
    if ((unsigned char)src[srcp]<=0x20) break;
    if (src[srcp]=='/') break;
    if (src[srcp]=='>') break;
    srcp++;
    dstc++;
  }
  return dstc;
}

/* Iterate tag attributes.
 */

int eggdev_html_for_each_attribute(
  const char *src,int srcc,
  int (*cb)(const char *k,int kc,const char *v,int vc,void *userdata),
  void *userdata
) {
  if (!src) return 0;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  if ((srcc<1)||(src[0]!='<')) return 0;
  int srcp=1,err;
  
  // Skip any leading whitespace, then the tag name.
  // If we encounter a slash, it's a closing tag and has no attributes.
  while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
  while ((srcp<srcc)&&((unsigned char)src[srcp]>0x20)) {
    if (src[srcp]=='/') return 0;
    if (src[srcp]=='>') return 0;
    srcp++;
  }
  
  // Now we're beyond the tag name. Read space-delimited `key="value"` pairs until '>'.
  while (srcp<srcc) {
    if ((unsigned char)src[srcp]<=0x20) { srcp++; continue; }
    if (src[srcp]=='/') { srcp++; continue; }
    if (src[srcp]=='>') break;
    const char *k=src+srcp;
    int kc=0;
    while (srcp<srcc) {
      if (src[srcp]=='=') break;
      if (src[srcp]=='/') break;
      if (src[srcp]=='>') break;
      if ((unsigned char)src[srcp]<=0x20) break;
      srcp++;
      kc++;
    }
    while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
    const char *v=0;
    int vc=0;
    if ((srcp<srcc)&&(src[srcp]=='=')) {
      srcp++;
      while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
      if ((srcp<srcc)&&((src[srcp]=='"')||(src[srcp]=='\''))) {
        char quote=src[srcp++];
        v=src+srcp;
        while ((srcp<srcc)&&(src[srcp++]!=quote)) vc++;
      } else {
        v=src+srcp;
        while ((srcp<srcc)&&(src[srcp]!='/')&&(src[srcp]!='>')&&((unsigned char)src[srcp++]>0x20)) vc++;
      }
    }
    if (err=cb(k,kc,v,vc,userdata)) return err;
  }
  return 0;
}

/* Get attribute, convenience.
 */
 
struct eggdev_html_get_attribute {
  const char *k;
  int kc;
  void *dstpp;
};

static int eggdev_html_get_attribute_cb(const char *k,int kc,const char *v,int vc,void *userdata) {
  struct eggdev_html_get_attribute *ctx=userdata;
  if (ctx->kc!=kc) return 0;
  if (memcmp(ctx->k,k,kc)) return 0;
  *(const void**)(ctx->dstpp)=v;
  return vc;
}

int eggdev_html_get_attribute(void *dstpp,const char *src,int srcc,const char *k,int kc) {
  if (!k) return 0;
  if (kc<0) { kc=0; while (k[kc]) kc++; }
  if (!kc) return 0;
  struct eggdev_html_get_attribute ctx={
    .k=k,
    .kc=kc,
    .dstpp=dstpp,
  };
  return eggdev_html_for_each_attribute(src,srcc,eggdev_html_get_attribute_cb,&ctx);
}

/* Measure and evaluate escape.
 */
 
static int eggdev_html_escape_eval_inner(const char *src,int srcc) {
  // Not going to do every named escape, there's like hundreds of them.
  switch (srcc) {
    case 2: {
        if (!memcmp(src,"lt",2)) return '<';
        if (!memcmp(src,"gt",2)) return '>';
      } break;
    case 3: {
        if (!memcmp(src,"amp",3)) return '&';
      } break;
    case 4: {
        if (!memcmp(src,"quot",4)) return '"';
        if (!memcmp(src,"apos",4)) return '\'';
      } break;
  }
  if ((srcc>=1)&&(src[0]=='#')) {
    if ((srcc>=2)&&(src[1]=='x')) {
      int codepoint=0,i=2;
      for (;i<srcc;i++) {
        int digit=sr_digit_eval(src[i]);
        if ((digit<0)||(digit>=16)) return -1;
        codepoint<<=4;
        codepoint|=digit;
      }
      return codepoint;
    } else {
      int codepoint=0,i=1;
      for (;i<srcc;i++) {
        int digit=src[i]-'0';
        if ((digit<0)||(digit>=10)) return -1;
        codepoint*=10;
        codepoint+=digit;
      }
      return codepoint;
    }
  }
  return -1;
}
 
static int eggdev_html_escape_eval(int *codepoint,const char *src,int srcc) {
  if (!src) return -1;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  if (srcc<1) return -1;
  if (src[0]!='&') return -1;
  int i=0; for (;i<srcc;i++) {
    if (src[i]==';') {
      int err=eggdev_html_escape_eval_inner(src+1,i-1);
      if (err<0) return -1;
      *codepoint=err;
      return i+1;
    }
  }
  return -1;
}

/* Evaluate text.
 */

int eggdev_html_text_eval(char *dst,int dsta,const char *src,int srcc) {
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  while (srcc&&((unsigned char)src[srcc-1]<=0x20)) srcc--;
  while (srcc&&((unsigned char)src[0]<=0x20)) { srcc--; src++; }
  int dstc=0,srcp=0;
  while (srcp<srcc) {
    if (src[srcp]=='&') {
      int seqlen,codepoint;
      if ((seqlen=eggdev_html_escape_eval(&codepoint,src+srcp,srcc-srcp))<1) {
        if (dstc<dsta) dst[dstc]='&';
        dstc++;
        srcp++;
      } else { 
        int err=sr_utf8_encode(dst+dstc,dsta-dstc,codepoint);
        if (err<0) return -1;
        dstc+=err;
        srcp+=seqlen;
      }
    } else {
      if (dstc<dsta) dst[dstc]=src[srcp];
      dstc++;
      srcp++;
    }
  }
  if (dstc<dsta) dst[dstc]=src[srcp];
  return dstc;
}
