#include "mf_internal.h"

/* Unread token.
 */
 
void mf_token_reader_unread(struct mf_token_reader *reader) {
  reader->p=reader->prev.srcp;
}

void mf_token_reader_warp(struct mf_token_reader *reader,const struct mf_token *token) {
  if (!reader||!token) return;
  reader->p=token->srcp;
  reader->prev=*token;
}

/* Quietly skip space.
 */
 
void mf_token_reader_skip_space(struct mf_token_reader *reader) {
  if (!reader) return;
  for (;;) {
    int srcc=reader->c-reader->p;
    if (srcc<1) return;
    const char *src=reader->v+reader->p;
    if ((unsigned char)src[0]<=0x20) { reader->p++; continue; }
    if (src[0]!='/') return;
    if (srcc<2) return;
    if (src[1]=='/') {
      int srcp=2;
      while ((srcp<srcc)&&(src[srcp++]!=0x0a)) ;
      reader->p+=srcp;
      continue;
    }
    if (src[1]=='*') {
      int srcp=2;
      for (;;) {
        if (srcp>srcc-2) return; // don't advance
        if ((src[srcp]=='*')&&(src[srcp+1]=='/')) {
          srcp+=2;
          break;
        }
        srcp++;
      }
      reader->p+=srcp;
      continue;
    }
    return;
  }
}

/* Peek and test.
 */
 
int mf_token_reader_next_is(struct mf_token_reader *reader,const char *src,int srcc) {
  struct mf_token_reader save=*reader;
  struct mf_token token;
  int err=mf_token_reader_next(&token,reader,0);
  *reader=save;
  if (err<1) return 0;
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  if (token.c!=srcc) return 0;
  if (memcmp(token.v,src,srcc)) return 0;
  return 1;
}

/* Measure from "${" through "}".
 * If a close brace is missing, we'll keep reading until we find one, even beyond the containing string.
 * Expect hilarious errors in this case.
 */
 
static int mf_measure_gravestring_unit(const char *src,int srcc) {
  if ((srcc<2)||(src[0]!='$')||(src[1]!='{')) return -1;
  struct mf_token_reader reader={.v=src,.c=srcc,.p=2};
  struct mf_token token;
  int err,depth=1;
  while ((err=mf_token_reader_next(&token,&reader,0))>0) {
    if ((token.c==1)&&(token.v[0]=='{')) depth++;
    else if ((token.c==1)&&(token.v[0]=='}')) {
      depth--;
      if (!depth) return reader.p;
    }
  }
  return -1;
}

/* Next token.
 */
 
int mf_token_reader_next(struct mf_token *token,struct mf_token_reader *reader,struct eggdev_minify_js *ctx) {
  if (!token||!reader) return -1;
  token->v=reader->v+reader->p;
  token->c=0;
  token->srcp=reader->p;
  token->fileid=reader->fileid;
  token->type=0;
  for (;;) {
  
    // EOF?
    if (reader->p>=reader->c) return 0;
    
    // Prep (token).
    const char *src=reader->v+reader->p;
    int srcc=reader->c-reader->p;
    token->v=src;
    token->srcp=reader->p;
    
    // Whitespace?
    if ((unsigned char)src[0]<=0x20) {
      reader->p++;
      continue;
    }
    
    // Comment?
    if ((srcc>=2)&&(src[0]=='/')) {
      if (src[1]=='/') {
        int srcp=2; while ((srcp<srcc)&&(src[srcp++]!=0x0a)) ;
        reader->p+=srcp;
        continue;
      }
      if (src[1]=='*') {
        int srcp=2;
        for (;;) {
          if (srcp>srcc-2) return mf_jserr_ifctx(ctx,token,"Unclosed block comment.");
          if ((src[srcp]=='*')&&(src[srcp+1]=='/')) { srcp+=2; break; }
          srcp++;
        }
        reader->p+=srcp;
        continue;
      }
    }
    
    // String?
    if ((src[0]=='"')||(src[0]=='\'')) {
      int srcp=1;
      for (;;) {
        if ((srcp>=srcc)||(src[srcp]==0x0a)) return mf_jserr_ifctx(ctx,token,"Unclosed string.");
        if (src[srcp]==src[0]) { srcp++; break; }
        if (src[srcp]=='\\') srcp+=2;
        else srcp+=1;
      }
      reader->p+=srcp;
      token->c=srcp;
      token->type=MF_TOKEN_TYPE_STRING;
      reader->prev=*token;
      return 1;
    }
    
    // Grave string?
    if (src[0]=='`') {
      int srcp=1;
      for (;;) {
        if (srcp>=srcc) return mf_jserr_ifctx(ctx,token,"Unclosed string.");
        if (src[srcp]=='`') { srcp++; break; }
        if ((srcp<=srcc-2)&&(src[srcp]=='$')&&(src[srcp+1]=='{')) {
          int len=mf_measure_gravestring_unit(src+srcp,srcc-srcp);
          if (len<2) {
            token->srcp+=srcp;
            return mf_jserr_ifctx(ctx,token,"Unclosed grave-string unit.");
          }
          srcp+=len;
          continue;
        }
        if (src[srcp]=='\\') srcp+=2;
        else srcp+=1;
      }
      reader->p+=srcp;
      token->c=srcp;
      token->type=MF_TOKEN_TYPE_GRAVESTRING;
      reader->prev=*token;
      return 1;
    }
    
    /* Regex?
     * These are the worst.
     * A correct parser would need to be aware of the syntatic context, but I refuse to do that just for the sake of inline regexes.
     * So instead we use some goofy heuristics.
     * It's a regex if:
     *  - Starts with '/'.
     *  - Closing '/' present on the same line, mindful of escapes.
     *  - Previous token is one of ( = ,
     */
    if ((src[0]=='/')&&(reader->prev.c==1)) {
      char pv=reader->prev.v[0];
      if ((pv=='(')||(pv=='=')||(pv==',')) {
        int srcp=1,ok=0;
        for (;;) {
          if (srcp>=srcc) break;
          if (src[srcp]==0x0a) break;
          if (src[srcp]=='/') {
            ok=1;
            srcp++;
            while ((srcp<srcc)&&(src[srcp]>='a')&&(src[srcp]<='z')) srcp++;
            break;
          }
          if (src[srcp]=='\\') srcp+=2;
          else srcp+=1;
        }
        if (ok) {
          reader->p+=srcp;
          token->c=srcp;
          token->type=MF_TOKEN_TYPE_REGEX;
          reader->prev=*token;
          return 1;
        }
      }
    }
    
    // Number? Leading sign is not part of the token; that's a unary operator.
    if ((src[0]>='0')&&(src[0]<='9')) {
      int srcp=1,dot=0;
      for (;srcp<srcc;srcp++) {
        if (JSIDENT(src[srcp])) continue;
        if ((src[srcp]=='.')&&!dot) {
          dot=1;
          continue;
        }
        break;
      }
      reader->p+=srcp;
      token->c=srcp;
      token->type=MF_TOKEN_TYPE_NUMBER;
      reader->prev=*token;
      return 1;
    }
    
    // Identifier?
    if (JSIDENT(src[0])) {
      int srcp=1;
      while ((srcp<srcc)&&JSIDENT(src[srcp])) srcp++;
      reader->p+=srcp;
      token->c=srcp;
      token->type=MF_TOKEN_TYPE_IDENTIFIER;
      reader->prev=*token;
      return 1;
    }
    
    // Everything else is an operator, even if it isn't.
    if ((token->c=mf_measure_js_operator(src,srcc,0))<1) return mf_jserr_ifctx(ctx,token,"Unexpected byte 0x%02x",(unsigned char)src[0]);
    reader->p+=token->c;
    token->type=MF_TOKEN_TYPE_OPERATOR;
    reader->prev=*token;
    return 1;
  }
}

/* Operators.
 */

int mf_measure_js_operator(const char *src,int srcc,int *cls) {
  if (!src) return 0;
  if (srcc<1) return 0;
  #define OK(clstag,len) { \
    if (cls) *cls=MF_OPCLS_##clstag; \
    return len; \
  }
  if (srcc>=10) {
    if (!memcmp(src,"instanceof",10)&&((srcc==10)||!JSIDENT(src[10]))) OK(CMP,10)
  }
  if (srcc>=6) {
    if (!memcmp(src,"typeof",6)&&((srcc==6)||!JSIDENT(src[6]))) OK(UNARY,6)
    if (!memcmp(src,"delete",6)&&((srcc==6)||!JSIDENT(src[6]))) OK(UNARY,6)
  }
  if (srcc>=4) {
    if (!memcmp(src,"void",4)&&((srcc==4)||!JSIDENT(src[4]))) OK(UNARY,4)
    if (!memcmp(src,">>>=",4)) OK(ASSIGN,4)
  }
  if (srcc>=3) {
    if (!memcmp(src,"new",3)&&((srcc==3)||!JSIDENT(src[3]))) OK(NEW,3)
    if (!memcmp(src,"...",3)) OK(SPECIAL,3)
    if (!memcmp(src,"===",3)) OK(EQ,3)
    if (!memcmp(src,"!==",3)) OK(EQ,3)
    if (!memcmp(src,">>>",3)) OK(SHIFT,3)
    if (!memcmp(src,">>=",3)) OK(ASSIGN,3)
    if (!memcmp(src,"<<=",3)) OK(ASSIGN,3)
    if (!memcmp(src,"**=",3)) OK(ASSIGN,3)
    if (!memcmp(src,"?.[",3)) OK(MEMBER,3)
    if (!memcmp(src,"?.(",3)) OK(CALL,3)
  }
  if (srcc>=2) {
    if (src[0]==src[1]) switch (src[0]) {
      case '!': OK(UNARY,2)
      case '~': OK(UNARY,2)
      case '-': OK(FIX,2)
      case '+': OK(FIX,2)
      case '=': OK(EQ,2)
      case '>': OK(SHIFT,2)
      case '<': OK(SHIFT,2)
      case '*': OK(EXP,2)
      case '&': OK(LAN,2)
      case '|': OK(LOR,2)
    } else if (src[1]=='=') switch (src[0]) {
      case '>': OK(CMP,2)
      case '<': OK(CMP,2)
      case '!': OK(EQ,2)
      case '+': OK(ASSIGN,2)
      case '-': OK(ASSIGN,2)
      case '*': OK(ASSIGN,2)
      case '/': OK(ASSIGN,2)
      case '%': OK(ASSIGN,2)
      case '&': OK(ASSIGN,2)
      case '|': OK(ASSIGN,2)
      case '^': OK(ASSIGN,2)
    } else {
      if (!memcmp(src,"?.",2)) OK(MEMBER,2)
      if (!memcmp(src,"=>",2)) OK(LAMBDA,2)
      if (!memcmp(src,"in",2)&&((srcc==2)||!JSIDENT(src[2]))) OK(CMP,2)
    }
  }
  switch (src[0]) {
    case '+': OK(ADD,1) // Also unary, do we need to note that somehow?
    case '-': OK(ADD,1) // Also unary
    case '~': OK(UNARY,1)
    case '!': OK(UNARY,1)
    case '.': OK(MEMBER,1)
    case '[': OK(MEMBER,1)
    case '(': OK(CALL,1)
    case '*': OK(MLT,1)
    case '/': OK(MLT,1)
    case '%': OK(MLT,1)
    case '&': OK(BAN,1)
    case '|': OK(BOR,1)
    case '^': OK(BXR,1)
    case '=': OK(ASSIGN,1)
    case '?': OK(SELECT,1)
    case '<': OK(CMP,1)
    case '>': OK(CMP,1)
    case ',': OK(SEQ,1)
  }
  OK(SPECIAL,1)
  #undef OK
}

/* Sequential tiny identifiers.
 */
 
static int mf_identifier_by_index(char *dst,int dsta,int p) {
  if (p<0) return -1;
  
  /* Four slightly different alphabets.
   * When it's a single character, use everything except digits.
   * Two or more characters, everything after the first uses the full alphabet.
   * Two characters exactly, the lead may use lowercase letters but not "d", "i", or "o" (so we won't hit "in", "if", "of").
   * Three or more, we eliminate every leading letter that occurs in any keyword.
   */
  char single[]="abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_$";
  int singlec=sizeof(single)-1;
  char lead2[]="abcefghjklmnpqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_$";
  int lead2c=sizeof(lead2)-1;
  char lead[]="qupahjkzxmABCDEFGHIJKLMNOPQRSTUVWXYZ_$";
  int leadc=sizeof(lead)-1;
  char after[]="abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_$0123456789";
  int afterc=sizeof(after)-1;
  
  if (p<singlec) {
    if (dsta>=1) dst[0]=single[p];
    return 1;
  }
  p-=singlec;
  
  int doublec=lead2c*afterc;
  if (p<doublec) {
    if (dsta>=2) {
      dst[0]=lead2[p/afterc];
      dst[1]=after[p%afterc];
    }
    return 2;
  }
  p-=doublec;
  
  int range=leadc*afterc*afterc;
  int dstc=3;
  while (p>=range) {
    range*=afterc;
    dstc++;
  }
  if (dstc>dsta) return dstc;
  int i=dstc; while (i-->1) {
    dst[i]=after[p%afterc];
    p/=afterc;
  }
  dst[0]=lead[p];
  return dstc;
}
 
char *mf_next_identifier(struct eggdev_minify_js *ctx,int *len) {
  if (!ctx) return 0;
  char tmp[16];
  int tmpc=mf_identifier_by_index(tmp,sizeof(tmp),ctx->nextident++);
  if ((tmpc<1)||(tmpc>sizeof(tmp))) return 0;
  *len=tmpc;
  return mf_js_text_intern(ctx,tmp,tmpc);
}
