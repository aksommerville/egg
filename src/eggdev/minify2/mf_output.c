#include "mf_internal.h"

/* Output one token, with a leading space if needed.
 */
 
static int mf_js_output_token(struct sr_encoder *dst,struct eggdev_minify_js *ctx,const char *src,int srcc) {
  if (srcc<1) return 0;
  // Space if we would otherwise be putting two identifier chars next to each other.
  if (dst->c<=0) return sr_encode_raw(dst,src,srcc);
  char prev=((char*)(dst->v))[dst->c-1];
  if ((src[0]==prev)&&((src[0]=='+')||(src[0]=='-'))) {
    if (sr_encode_u8(dst,' ')<0) return -1;
  } else if (JSIDENT(src[0])&&JSIDENT(prev)) {
    if (sr_encode_u8(dst,' ')<0) return -1;
  }
  return sr_encode_raw(dst,src,srcc);
}

/* Unwrite one "," from output, if it's there.
 */
 
static void mf_js_drop_last_comma(struct sr_encoder *dst) {
  if (!dst||(dst->c<1)) return;
  if (((char*)dst->v)[dst->c-1]==',') dst->c--;
}
 
static void mf_js_drop_last_comma_or_semicolon(struct sr_encoder *dst) {
  if (!dst||(dst->c<1)) return;
  char last=((char*)dst->v)[dst->c-1];
  if ((last==',')||(last==';')) dst->c--;
}

/* ROOT
 */
 
static int mf_js_output_ROOT(struct sr_encoder *dst,struct eggdev_minify_js *ctx,struct mf_node *node) {
  int i=0,err;
  for (;i<node->childc;i++) {
    if ((err=mf_js_output(dst,ctx,node->childv[i]))<0) return err;
  }
  return 0;
}

/* BLOCK
 */
 
static int mf_js_block_must_use_brackets(struct mf_node *node) {
  if (node->childc>1) return 1;
  if (node->parent) {
    switch (node->parent->type) {
      case MF_NODE_TYPE_METHOD:
      case MF_NODE_TYPE_LAMBDA:
      case MF_NODE_TYPE_TRY:
      case MF_NODE_TYPE_FUNCTION:
      case MF_NODE_TYPE_IF: // Not always necessary. But we do need them if our parent has an "else" block, and our one child is an else-less if.
        return 1;
    }
  }
  return 0;
}
 
static int mf_js_output_BLOCK(struct sr_encoder *dst,struct eggdev_minify_js *ctx,struct mf_node *node) {
  int i=0,err;
  if (mf_js_block_must_use_brackets(node)) {
    if (sr_encode_u8(dst,'{')<0) return -1;
    for (;i<node->childc;i++) {
      if ((err=mf_js_output(dst,ctx,node->childv[i]))<0) return err;
    }
    mf_js_drop_last_comma_or_semicolon(dst);
    if (sr_encode_u8(dst,'}')<0) return -1;
  } else if (node->childc==0) {
    if (mf_js_output_token(dst,ctx,";",1)<0) return -1;
  } else if (node->childc==1) {
    if ((err=mf_js_output(dst,ctx,node->childv[0]))<0) return err;
  }
  return 0;
}

/* VALUE
 */
 
static int mf_js_output_VALUE(struct sr_encoder *dst,struct eggdev_minify_js *ctx,struct mf_node *node) {

  /* If a grave string survives this far as type VALUE, it's intended to be literal.
   * Replace the quotes.
   */
  if ((node->token.c>=2)&&(node->token.v[0]=='`')&&(node->token.v[node->token.c-1]=='`')) {
    int hasquote=0,hasapos=0,i=node->token.c;
    const char *v=node->token.v;
    for (;i-->0;v++) {
      if (*v=='"') hasquote=1;
      else if (*v=='\'') hasapos=1;
    }
    // The typical case: Just lop off the graves and replace with quote or apostrophe.
    if (!hasquote) {
      if (mf_js_output_token(dst,ctx,"\"",1)<0) return -1;
      if (mf_js_output_token(dst,ctx,node->token.v+1,node->token.c-2)<0) return -1;
      if (mf_js_output_token(dst,ctx,"\"",1)<0) return -1;
      return 0;
    }
    if (!hasapos) {
      if (mf_js_output_token(dst,ctx,"'",1)<0) return -1;
      if (mf_js_output_token(dst,ctx,node->token.v+1,node->token.c-2)<0) return -1;
      if (mf_js_output_token(dst,ctx,"'",1)<0) return -1;
      return 0;
    }
    // sr_string_eval is JS-compatible but it treats grave as a plain quote.
    char tmp[1024];
    int tmpc=sr_string_eval(tmp,sizeof(tmp),node->token.v,node->token.c);
    if ((tmpc>=0)&&(tmpc<=sizeof(tmp))) {
      char tmp2[1024];
      int tmp2c=sr_string_repr(tmp2,sizeof(tmp2),tmp,tmpc);
      if ((tmp2c>=2)&&(tmp2c<=sizeof(tmp2))) {
        if (mf_js_output_token(dst,ctx,tmp2,tmp2c)<0) return -1;
        return 0;
      }
    }
    // Too long, or something weird. Let it emit verbatim.
  }

  return mf_js_output_token(dst,ctx,node->token.v,node->token.c);
}

/* EXPWRAP
 */
 
static int mf_js_output_EXPWRAP(struct sr_encoder *dst,struct eggdev_minify_js *ctx,struct mf_node *node) {
  int err;
  if (node->childc<1) return 0;
  if (node->childc>1) return mf_jserr(ctx,&node->token,"%s: childc=%d\n",__func__,node->childc);
  if ((err=mf_js_output(dst,ctx,node->childv[0]))<0) return err;
  if (mf_js_output_token(dst,ctx,";",1)<0) return -1;
  return 0;
}

/* CLASS
 */
 
static int mf_js_output_CLASS(struct sr_encoder *dst,struct eggdev_minify_js *ctx,struct mf_node *node) {
  if (mf_js_output_token(dst,ctx,"class",5)<0) return -1;
  if (mf_js_output_token(dst,ctx,node->token.v,node->token.c)<0) return -1;
  if (mf_js_output_token(dst,ctx,"{",1)<0) return -1;
  int i=0,err; for (;i<node->childc;i++) {
    if ((err=mf_js_output(dst,ctx,node->childv[i]))<0) return err;
  }
  if (mf_js_output_token(dst,ctx,"}",1)<0) return -1;
  return 0;
}

/* METHOD
 */
 
static int mf_js_output_METHOD(struct sr_encoder *dst,struct eggdev_minify_js *ctx,struct mf_node *node) {
  int err;
  if (node->childc!=2) return mf_jserr(ctx,&node->token,"%s: childc=%d",__func__,node->childc);
  if (node->argv[0]) {
    if (mf_js_output_token(dst,ctx,"static",6)<0) return -1;
  }
  if (mf_js_output_token(dst,ctx,node->token.v,node->token.c)<0) return -1;
  if ((err=mf_js_output(dst,ctx,node->childv[0]))<0) return err;
  if ((err=mf_js_output(dst,ctx,node->childv[1]))<0) return err;
  return 0;
}

/* PARAMLIST
 */
 
static int mf_js_output_PARAMLIST(struct sr_encoder *dst,struct eggdev_minify_js *ctx,struct mf_node *node) {
  int err,i;
  if (mf_js_output_token(dst,ctx,"(",1)<0) return -1;
  for (i=0;i<node->childc;i++) {
    if ((err=mf_js_output(dst,ctx,node->childv[i]))<0) return err;
    if ((err=mf_js_output_token(dst,ctx,",",1))<0) return err;
  }
  mf_js_drop_last_comma(dst);
  if (mf_js_output_token(dst,ctx,")",1)<0) return -1;
  return 0;
}

/* PARAM
 */
 
static int mf_js_output_PARAM(struct sr_encoder *dst,struct eggdev_minify_js *ctx,struct mf_node *node) {
  int err;
  if (node->childc>1) return mf_jserr(ctx,&node->token,"%s: childc=%d",__func__,node->childc);
  if (node->argv[0]) {
    if (mf_js_output_token(dst,ctx,"...",3)<0) return -1;
  }
  if (mf_js_output_token(dst,ctx,node->token.v,node->token.c)<0) return -1;
  if (node->childc) {
    if (mf_js_output_token(dst,ctx,"=",1)<0) return -1;
    if ((err=mf_js_output(dst,ctx,node->childv[0]))<0) return err;
  }
  return 0;
}

/* DSPARAM
 */
 
static int mf_js_output_DSPARAM(struct sr_encoder *dst,struct eggdev_minify_js *ctx,struct mf_node *node) {
  if ((node->childc<1)||(node->childc>2)) return mf_jserr(ctx,&node->token,"%s: childc=%d",__func__,node->childc);
  int err;
  int startp=dst->c;
  if ((err=mf_js_output(dst,ctx,node->childv[0]))<0) return err; // Should be PARAMLIST, and will incorrectly emit surrounding "()".
  int endp=dst->c-1;
  if ((startp<endp)&&(node->token.c==1)) {
    char *dstv=(char*)dst->v;
    if ((dstv[startp]=='(')&&(dstv[endp]==')')) {
      if (node->token.v[0]=='{') {
        dstv[startp]='{';
        dstv[endp]=='}';
      } else {
        dstv[startp]=='[';
        dstv[endp]=']';
      }
    }
  }
  if (node->childc>=2) {
    if (mf_js_output_token(dst,ctx,"=",1)<0) return -1;
    if ((err=mf_js_output(dst,ctx,node->childv[1]))<0) return err;
  }
  return 0;
}

/* IF
 */
 
static int mf_js_output_IF(struct sr_encoder *dst,struct eggdev_minify_js *ctx,struct mf_node *node) {
  int err;
  if ((node->childc<2)||(node->childc>3)) return mf_jserr(ctx,&node->token,"%s: childc=%d",__func__,node->childc);
  if (mf_js_output_token(dst,ctx,"if(",3)<0) return -1;
  if ((err=mf_js_output(dst,ctx,node->childv[0]))<0) return err;
  if (mf_js_output_token(dst,ctx,")",1)<0) return -1;
  if ((err=mf_js_output(dst,ctx,node->childv[1]))<0) return err;
  if (node->childc>=3) {
    if (mf_js_output_token(dst,ctx,"else",4)<0) return -1;
    if ((err=mf_js_output(dst,ctx,node->childv[2]))<0) return err;
  }
  return 0;
}

/* OP
 */
 
static int mf_js_output_OP_naked(struct sr_encoder *dst,struct eggdev_minify_js *ctx,struct mf_node *node) {
  int err,i;
  
  /* When there's just one operand, it's "OP A"
   */
  if (node->childc==1) {
    if (mf_js_output_token(dst,ctx,node->token.v,node->token.c)<0) return -1;
    if ((err=mf_js_output(dst,ctx,node->childv[0]))<0) return err;
    return 0;
  }
  
  /* Ternary selection is unique.
   */
  if ((node->childc==3)&&(node->token.c==1)&&(node->token.v[0]=='?')) {
    if ((err=mf_js_output(dst,ctx,node->childv[0]))<0) return err;
    if (mf_js_output_token(dst,ctx,"?",1)<0) return -1;
    if ((err=mf_js_output(dst,ctx,node->childv[1]))<0) return err;
    if (mf_js_output_token(dst,ctx,":",1)<0) return -1;
    if ((err=mf_js_output(dst,ctx,node->childv[2]))<0) return err;
    return 0;
  }
  
  /* All others (typically binary) are "A OP B [OP C [...]]"
   */
  if ((err=mf_js_output(dst,ctx,node->childv[0]))<0) return err;
  for (i=1;i<node->childc;i++) {
    if (mf_js_output_token(dst,ctx,node->token.v,node->token.c)<0) return -1;
    if ((err=mf_js_output(dst,ctx,node->childv[i]))<0) return err;
  }
  return 0;
}
 
static int mf_js_output_OP_parens(struct sr_encoder *dst,struct eggdev_minify_js *ctx,struct mf_node *node) {
  int err;
  if (mf_js_output_token(dst,ctx,"(",1)<0) return -1;
  if ((err=mf_js_output_OP_naked(dst,ctx,node))<0) return -1;
  if (mf_js_output_token(dst,ctx,")",1)<0) return 1;
  return 0;
}
 
static int mf_js_output_OP(struct sr_encoder *dst,struct eggdev_minify_js *ctx,struct mf_node *node) {
  if (node->childc<1) return mf_jserr(ctx,&node->token,"%s: childc=%d",__func__,node->childc);
  
  int opcls=node->argv[0];
  if (node->parent) switch (node->parent->type) {
    case MF_NODE_TYPE_OP: {
        int popcls=node->parent->argv[0];
        if (popcls>opcls) return mf_js_output_OP_parens(dst,ctx,node);
        //TODO This is probably not the whole story. What about RTL operators?
      } break;
    //TODO We are probably responsible for the "if", "while", and "do" parentheses.
  }
  return mf_js_output_OP_naked(dst,ctx,node);
}

/* RETURN
 */
 
static int mf_js_output_RETURN(struct sr_encoder *dst,struct eggdev_minify_js *ctx,struct mf_node *node) {
  int err;
  if (node->childc>1) return mf_jserr(ctx,&node->token,"%s: childc=%d",__func__,node->childc);
  if (mf_js_output_token(dst,ctx,"return",6)<0) return -1;
  if (node->childc>=1) {
    if ((err=mf_js_output(dst,ctx,node->childv[0]))<0) return err;
  }
  if (mf_js_output_token(dst,ctx,";",1)<0) return -1;
  return 0;
}

/* LAMBDA
 */
 
static int mf_js_output_LAMBDA(struct sr_encoder *dst,struct eggdev_minify_js *ctx,struct mf_node *node) {
  int err;
  if (node->childc!=2) return mf_jserr(ctx,&node->token,"%s: childc=%d",__func__,node->childc);
  struct mf_node *paramlist=node->childv[0];
  struct mf_node *body=node->childv[1];
  
  /* With just one parameter, do not emit parens.
   * We *do* emit parens for zero params. The optimization steps before us should insert a fake 1-character param if possible.
   * We don't make that decision here because it's a heavy decision (how do we know some identifier we make up isn't being used?).
   */
  if (paramlist->childc==1) {
    if ((err=mf_js_output(dst,ctx,paramlist->childv[0]))<0) return err;
  } else {
    if ((err=mf_js_output(dst,ctx,paramlist))<0) return err;
  }
  
  if (mf_js_output_token(dst,ctx,"=>",2)<0) return -1;
  if ((err=mf_js_output(dst,ctx,body))<0) return err;
  return 0;
}

/* CALL
 */
 
static int mf_js_output_CALL(struct sr_encoder *dst,struct eggdev_minify_js *ctx,struct mf_node *node) {
  int err,i;
  if (node->childc<1) return mf_jserr(ctx,&node->token,"%s: childc=%d",__func__,node->childc);
  if ((err=mf_js_output(dst,ctx,node->childv[0]))<0) return err;
  if (mf_js_output_token(dst,ctx,node->token.v,node->token.c)<0) return -1; // "(" or "?.("
  for (i=1;i<node->childc;i++) {
    if ((err=mf_js_output(dst,ctx,node->childv[i]))<0) return err;
    if (mf_js_output_token(dst,ctx,",",1)<0) return -1;
  }
  mf_js_drop_last_comma(dst);
  if (mf_js_output_token(dst,ctx,")",1)<0) return -1;
  return 0;
}

/* INDEX
 */
 
static int mf_js_output_INDEX(struct sr_encoder *dst,struct eggdev_minify_js *ctx,struct mf_node *node) {
  if (node->childc!=2) return mf_jserr(ctx,&node->token,"%s: childc=%d",__func__,node->childc);
  int err;
  if ((err=mf_js_output(dst,ctx,node->childv[0]))<0) return err;
  if (mf_js_output_token(dst,ctx,node->token.v,node->token.c)<0) return -1;
  if ((err=mf_js_output(dst,ctx,node->childv[1]))<0) return err;
  if (mf_js_output_token(dst,ctx,"]",1)<0) return -1;
  return 0;
}

/* DECL
 */
 
static int mf_js_output_DECL(struct sr_encoder *dst,struct eggdev_minify_js *ctx,struct mf_node *node) {
  int err,i;
  if (mf_js_output_token(dst,ctx,node->token.v,node->token.c)<0) return -1;
  for (i=0;i<node->childc;i++) {
    if ((err=mf_js_output(dst,ctx,node->childv[i]))<0) return err;
    if (mf_js_output_token(dst,ctx,",",1)<0) return -1;
  }
  mf_js_drop_last_comma(dst);
  if (mf_js_output_token(dst,ctx,";",1)<0) return -1;
  return 0;
}

/* THROW
 */
 
static int mf_js_output_THROW(struct sr_encoder *dst,struct eggdev_minify_js *ctx,struct mf_node *node) {
  if (node->childc>1) return mf_jserr(ctx,&node->token,"%s: childc=%d",__func__,node->childc);
  if (mf_js_output_token(dst,ctx,"throw",5)<0) return -1;
  if (node->childc>=1) {
    int err=mf_js_output(dst,ctx,node->childv[0]);
    if (err<0) return err;
  }
  if (mf_js_output_token(dst,ctx,";",1)<0) return -1;
  return 0;
}

/* ARRAY
 */
 
static int mf_js_output_ARRAY(struct sr_encoder *dst,struct eggdev_minify_js *ctx,struct mf_node *node) {
  int err,i;
  if (mf_js_output_token(dst,ctx,"[",1)<0) return -1;
  for (i=0;i<node->childc;i++) {
    if ((err=mf_js_output(dst,ctx,node->childv[i]))<0) return err;
    if (mf_js_output_token(dst,ctx,",",1)<0) return -1;
  }
  mf_js_drop_last_comma(dst);
  if (mf_js_output_token(dst,ctx,"]",1)<0) return -1;
  return 0;
}

/* OBJECT
 */
 
static int mf_js_output_OBJECT(struct sr_encoder *dst,struct eggdev_minify_js *ctx,struct mf_node *node) {
  int err,i;
  if (mf_js_output_token(dst,ctx,"{",1)<0) return -1;
  for (i=0;i<node->childc;i++) {
    if ((err=mf_js_output(dst,ctx,node->childv[i]))<0) return err;
    if (mf_js_output_token(dst,ctx,",",1)<0) return -1;
  }
  mf_js_drop_last_comma(dst);
  if (mf_js_output_token(dst,ctx,"}",1)<0) return -1;
  return 0;
}

/* SWITCH
 */
 
static int mf_js_output_SWITCH(struct sr_encoder *dst,struct eggdev_minify_js *ctx,struct mf_node *node) {
  if (node->childc<1) return mf_jserr(ctx,&node->token,"%s: childc=%d",__func__,node->childc);
  int err,i;
  if (mf_js_output_token(dst,ctx,"switch(",7)<0) return -1;
  if ((err=mf_js_output(dst,ctx,node->childv[0]))<0) return err;
  if (mf_js_output_token(dst,ctx,"){",2)<0) return -1;
  for (i=1;i<node->childc;i++) {
    if ((err=mf_js_output(dst,ctx,node->childv[i]))<0) return err;
  }
  if (mf_js_output_token(dst,ctx,"}",1)<0) return -1;
  return 0;
}

/* CASE
 */
 
static int mf_js_output_CASE(struct sr_encoder *dst,struct eggdev_minify_js *ctx,struct mf_node *node) {
  int err;
  if (node->childc>1) return mf_jserr(ctx,&node->token,"%s: childc=%d",__func__,node->childc);
  if (mf_js_output_token(dst,ctx,node->token.v,node->token.c)<0) return -1;
  if (node->childc>=1) {
    if ((err=mf_js_output(dst,ctx,node->childv[0]))<0) return err;
  }
  if (mf_js_output_token(dst,ctx,":",1)<0) return -1;
  return 0;
}

/* TRY
 */
 
static int mf_js_output_TRY(struct sr_encoder *dst,struct eggdev_minify_js *ctx,struct mf_node *node) {
  int reqc=1,err;
  if (node->argv[0]) reqc+=2;
  if (node->argv[1]) reqc+=1;
  if (node->childc!=reqc) return mf_jserr(ctx,&node->token,"%s: childc=%d catch=%d finally=%d",__func__,node->childc,node->argv[0],node->argv[1]);
  if (mf_js_output_token(dst,ctx,"try",3)<0) return -1;
  if ((err=mf_js_output(dst,ctx,node->childv[0]))<0) return err;
  int childp=1;
  if (node->argv[0]) {
    if (mf_js_output_token(dst,ctx,"catch(",6)<0) return -1;
    if ((err=mf_js_output(dst,ctx,node->childv[childp++]))<0) return err;
    if (mf_js_output_token(dst,ctx,")",1)<0) return -1;
    if ((err=mf_js_output(dst,ctx,node->childv[childp++]))<0) return err;
  }
  if (node->argv[1]) {
    if (mf_js_output_token(dst,ctx,"finally",7)<0) return -1;
    if ((err=mf_js_output(dst,ctx,node->childv[childp++]))<0) return err;
  }
  return 0;
}

/* FOR1
 */
 
static int mf_js_output_FOR1(struct sr_encoder *dst,struct eggdev_minify_js *ctx,struct mf_node *node) {
  int err;
  if (node->childc!=3) return mf_jserr(ctx,&node->token,"%s: childc=%d",__func__,node->childc);
  if (mf_js_output_token(dst,ctx,"for(",4)<0) return -1;
  switch (node->argv[1]) {
    case 1: if (mf_js_output_token(dst,ctx,"const",5)<0) return -1; break;
    case 2: if (mf_js_output_token(dst,ctx,"let",3)<0) return -1; break;
    case 3: if (mf_js_output_token(dst,ctx,"var",3)<0) return -1; break;
    default: return mf_jserr(ctx,&node->token,"%s: argv[1]=%d",__func__,node->argv[1]);
  }
  if ((err=mf_js_output(dst,ctx,node->childv[0]))<0) return err;
  switch (node->argv[0]) {
    case 1: if (mf_js_output_token(dst,ctx,"in",2)<0) return -1; break;
    case 2: if (mf_js_output_token(dst,ctx,"of",2)<0) return -1; break;
    default: return mf_jserr(ctx,&node->token,"%s: argv[0]=%d",__func__,node->argv[0]);
  }
  if ((err=mf_js_output(dst,ctx,node->childv[1]))<0) return err;
  if (mf_js_output_token(dst,ctx,")",1)<0) return -1;
  if ((err=mf_js_output(dst,ctx,node->childv[2]))<0) return err;
  return 0;
}

/* FOR3
 */
 
static int mf_js_output_FOR3(struct sr_encoder *dst,struct eggdev_minify_js *ctx,struct mf_node *node) {
  if (node->childc!=4) return mf_jserr(ctx,&node->token,"%s: childc=%d",__func__,node->childc);
  int err;
  if (mf_js_output_token(dst,ctx,"for(",4)<0) return -1;
  if ((err=mf_js_output(dst,ctx,node->childv[0]))<0) return err; // init is a statement, so it produces the semicolon for us.
  if ((err=mf_js_output(dst,ctx,node->childv[1]))<0) return err;
  if (mf_js_output_token(dst,ctx,";",1)<0) return -1;
  if ((err=mf_js_output(dst,ctx,node->childv[2]))<0) return err;
  if (mf_js_output_token(dst,ctx,")",1)<0) return -1;
  if ((err=mf_js_output(dst,ctx,node->childv[3]))<0) return err;
  return 0;
}

/* WHILE
 */
 
static int mf_js_output_WHILE(struct sr_encoder *dst,struct eggdev_minify_js *ctx,struct mf_node *node) {
  int err;
  if (node->childc!=2) return mf_jserr(ctx,&node->token,"%s: childc=%d",__func__,node->childc);
  if (mf_js_output_token(dst,ctx,"while(",6)<0) return -1;
  if ((err=mf_js_output(dst,ctx,node->childv[0]))<0) return err;
  if (mf_js_output_token(dst,ctx,")",1)<0) return -1;
  if ((err=mf_js_output(dst,ctx,node->childv[1]))<0) return err;
  return 0;
}

/* DO
 */
 
static int mf_js_output_DO(struct sr_encoder *dst,struct eggdev_minify_js *ctx,struct mf_node *node) {
  int err;
  if (node->childc!=2) return mf_jserr(ctx,&node->token,"%s: childc=%d",__func__,node->childc);
  if (mf_js_output_token(dst,ctx,"do",2)<0) return -1;
  if ((err=mf_js_output(dst,ctx,node->childv[0]))<0) return err;
  if (mf_js_output_token(dst,ctx,"while(",6)<0) return -1;
  if ((err=mf_js_output(dst,ctx,node->childv[1]))<0) return err;
  if (mf_js_output_token(dst,ctx,");",2)<0) return err;
  return 0;
}

/* LOOPCTL
 */
 
static int mf_js_output_LOOPCTL(struct sr_encoder *dst,struct eggdev_minify_js *ctx,struct mf_node *node) {
  if (mf_js_output_token(dst,ctx,node->token.v,node->token.c)<0) return -1;
  if (mf_js_output_token(dst,ctx,";",1)<0) return -1;
  return 0;
}

/* POSTFIX
 */
 
static int mf_js_output_POSTFIX(struct sr_encoder *dst,struct eggdev_minify_js *ctx,struct mf_node *node) {
  if (node->childc!=1) return mf_jserr(ctx,&node->token,"%s: childc=%d",__func__,node->childc);
  //TODO Do we need parens?
  int err=mf_js_output(dst,ctx,node->childv[0]);
  if (err<0) return err;
  if (mf_js_output_token(dst,ctx,node->token.v,node->token.c)<0) return -1;
  return 0;
}

/* FUNCTION
 */
 
static int mf_js_output_FUNCTION(struct sr_encoder *dst,struct eggdev_minify_js *ctx,struct mf_node *node) {
  if (node->childc!=2) return mf_jserr(ctx,&node->token,"%s: childc=%d",__func__,node->childc);
  int err;
  if (mf_js_output_token(dst,ctx,"function",8)<0) return -1;
  if (node->token.c&&((node->token.c!=8)||memcmp(node->token.v,"function",8))) {
    if (mf_js_output_token(dst,ctx,node->token.v,node->token.c)<0) return -1;
  }
  if ((err=mf_js_output(dst,ctx,node->childv[0]))<0) return err;
  if ((err=mf_js_output(dst,ctx,node->childv[1]))<0) return err;
  return 0;
}

/* FIELD
 */
 
static int mf_js_output_FIELD(struct sr_encoder *dst,struct eggdev_minify_js *ctx,struct mf_node *node) {
  if ((node->childc<1)||(node->childc>2)) return mf_jserr(ctx,&node->token,"%s: childc=%d",__func__,node->childc);
  int err;
  if ((err=mf_js_output(dst,ctx,node->childv[0]))<0) return err;
  if (node->childc>=2) {
    if (mf_js_output_token(dst,ctx,":",1)<0) return -1;
    if ((err=mf_js_output(dst,ctx,node->childv[1]))<0) return err;
  }
  return 0;
}

/* Output text for node.
 */
 
int mf_js_output(struct sr_encoder *dst,struct eggdev_minify_js *ctx,struct mf_node *node) {
  if (!dst||!ctx||!node) return -1;
  int err=-1;
  switch (node->type) {
    case MF_NODE_TYPE_ROOT: err=mf_js_output_ROOT(dst,ctx,node); break;
    case MF_NODE_TYPE_BLOCK: err=mf_js_output_BLOCK(dst,ctx,node); break;
    case MF_NODE_TYPE_VALUE: err=mf_js_output_VALUE(dst,ctx,node); break;
    case MF_NODE_TYPE_EXPWRAP: err=mf_js_output_EXPWRAP(dst,ctx,node); break;
    case MF_NODE_TYPE_CLASS: err=mf_js_output_CLASS(dst,ctx,node); break;
    case MF_NODE_TYPE_METHOD: err=mf_js_output_METHOD(dst,ctx,node); break;
    case MF_NODE_TYPE_PARAMLIST: err=mf_js_output_PARAMLIST(dst,ctx,node); break;
    case MF_NODE_TYPE_PARAM: err=mf_js_output_PARAM(dst,ctx,node); break;
    case MF_NODE_TYPE_DSPARAM: err=mf_js_output_DSPARAM(dst,ctx,node); break;
    case MF_NODE_TYPE_IF: err=mf_js_output_IF(dst,ctx,node); break;
    case MF_NODE_TYPE_OP: err=mf_js_output_OP(dst,ctx,node); break;
    case MF_NODE_TYPE_RETURN: err=mf_js_output_RETURN(dst,ctx,node); break;
    case MF_NODE_TYPE_LAMBDA: err=mf_js_output_LAMBDA(dst,ctx,node); break;
    case MF_NODE_TYPE_CALL: err=mf_js_output_CALL(dst,ctx,node); break;
    case MF_NODE_TYPE_INDEX: err=mf_js_output_INDEX(dst,ctx,node); break;
    case MF_NODE_TYPE_DECL: err=mf_js_output_DECL(dst,ctx,node); break;
    case MF_NODE_TYPE_THROW: err=mf_js_output_THROW(dst,ctx,node); break;
    case MF_NODE_TYPE_ARRAY: err=mf_js_output_ARRAY(dst,ctx,node); break;
    case MF_NODE_TYPE_OBJECT: err=mf_js_output_OBJECT(dst,ctx,node); break;
    case MF_NODE_TYPE_SWITCH: err=mf_js_output_SWITCH(dst,ctx,node); break;
    case MF_NODE_TYPE_CASE: err=mf_js_output_CASE(dst,ctx,node); break;
    case MF_NODE_TYPE_TRY: err=mf_js_output_TRY(dst,ctx,node); break;
    case MF_NODE_TYPE_FOR1: err=mf_js_output_FOR1(dst,ctx,node); break;
    case MF_NODE_TYPE_FOR3: err=mf_js_output_FOR3(dst,ctx,node); break;
    case MF_NODE_TYPE_WHILE: err=mf_js_output_WHILE(dst,ctx,node); break;
    case MF_NODE_TYPE_DO: err=mf_js_output_DO(dst,ctx,node); break;
    case MF_NODE_TYPE_LOOPCTL: err=mf_js_output_LOOPCTL(dst,ctx,node); break;
    case MF_NODE_TYPE_POSTFIX: err=mf_js_output_POSTFIX(dst,ctx,node); break;
    case MF_NODE_TYPE_FUNCTION: err=mf_js_output_FUNCTION(dst,ctx,node); break;
    case MF_NODE_TYPE_FIELD: err=mf_js_output_FIELD(dst,ctx,node); break;
  }
  if (err<0) {
    if (err!=-2) err=mf_jserr(ctx,&node->token,"Unspecified error outputting node. type=%d text='%.*s'",node->type,node->token.c,node->token.v);
    return err;
  }
  return 0;
}
