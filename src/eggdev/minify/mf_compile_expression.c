#include "mf_internal.h"

/* Destructured parameter.
 * (parent) is DSPARAM and the previous token was '{' or '['.
 * We consume thru the matching '}' or ']'.
 */
 
static int mf_js_compile_destructured_param(struct mf_node *parent,struct eggdev_minify_js *ctx,struct mf_token_reader *reader) {
  int err;
  char closer=(parent->token.v[0]=='[')?']':'}';
  for (;;) {
    struct mf_token token;
    if ((err=mf_token_reader_next(&token,reader,ctx))<0) return err;
    if ((token.c==1)&&(token.v[0]==closer)) return 0;
    if (!err) return mf_jserr(ctx,&parent->token,"Unclosed destructuring parameter.");
    //TODO Are destructuring params allowed to have their own rest parameters?
    if (token.type!=MF_TOKEN_TYPE_IDENTIFIER) return mf_jserr(ctx,&token,"Expected identifier or '%c'.",closer);
    struct mf_node *k=mf_node_spawn(parent);
    if (!k) return -1;
    k->token=token;
    k->type=MF_NODE_TYPE_PARAM;
    if ((err=mf_token_reader_next(&token,reader,ctx))<0) return err;
    if ((token.c==1)&&(token.v[0]==closer)) return 0;
    if ((token.c!=1)&&(token.v[0]!=',')) return mf_jserr(ctx,&token,"Expected '%c' or ','.",closer);
  }
}

/* Parameter list.
 */
 
int mf_js_compile_paramlist(struct mf_node *parent,struct eggdev_minify_js *ctx,struct mf_token_reader *reader) {
  int err;
  struct mf_node *node=mf_node_spawn(parent);
  if (!node) return -1;
  node->type=MF_NODE_TYPE_PARAMLIST;
  if ((err=mf_token_reader_next(&node->token,reader,ctx))<0) return err;
  if ((node->token.c!=1)||(node->token.v[0]!='(')) return mf_jserr(ctx,&node->token,"Expected parameter list.");
  for (;;) {
    struct mf_token token;
    if ((err=mf_token_reader_next(&token,reader,ctx))<0) return err;
    if (!err) return mf_jserr(ctx,&node->token,"Unclosed parameter list.");
    if ((token.c==1)&&(token.v[0]==')')) break;
    struct mf_node *param=0;
    
    /* Destructuring parameters.
     */
    if ((token.c==1)&&((token.v[0]=='[')||(token.v[0]=='{'))) {
      if (!(param=mf_node_spawn(node))) return -1;
      param->type=MF_NODE_TYPE_DSPARAM;
      param->token=token;
      if ((err=mf_js_compile_destructured_param(param,ctx,reader))<0) return err;
    
    /* Regular and rest parameters.
     */
    } else {
      int rest=0;
      if ((token.c==3)&&!memcmp(token.v,"...",3)) {
        rest=1;
        if ((err=mf_token_reader_next(&token,reader,ctx))<0) return err;
      }
      if (token.type!=MF_TOKEN_TYPE_IDENTIFIER) return mf_jserr(ctx,&token,"Expected identifier.");
      if (!(param=mf_node_spawn(node))) return -1;
      if (!param) return -1;
      param->type=MF_NODE_TYPE_PARAM;
      param->token=token;
      param->argv[0]=rest;
    }
    
    /* Is there an initializer?
     */
    if ((err=mf_token_reader_next(&token,reader,ctx))<0) return err;
    if ((token.c==1)&&(token.v[0]=='=')) {
      if ((err=mf_js_compile_expression(param,ctx,reader))<0) return err;
      if ((err=mf_token_reader_next(&token,reader,ctx))<0) return err;
    }
    if ((token.c==1)&&(token.v[0]==')')) break;
    if ((token.c!=1)||(token.v[0]!=',')) return mf_jserr(ctx,&token,"Expected ',' or ')'");
  }
  return 0;
}

/* Spawn a string node under (parent), containing the unquoted text provided.
 * Text main contain escapes. It should be sourced from within some other string literal.
 */
 
static int mf_js_spawn_string_node_unquoted(struct mf_node *parent,struct eggdev_minify_js *ctx,const char *src,int srcc) {
  char tmp[1024];
  int tmpc=2+srcc;
  if (tmpc>sizeof(tmp)) return -1;
  tmp[0]='"';
  memcpy(tmp+1,src,srcc);
  tmp[tmpc-1]='"';
  char *atom=mf_js_text_intern(ctx,tmp,tmpc);
  if (!atom) return -1;
  struct mf_node *node=mf_node_spawn(parent);
  if (!node) return -1;
  node->type=MF_NODE_TYPE_VALUE;
  node->token.v=atom;
  node->token.c=tmpc;
  node->token.srcp=parent->token.srcp;
  node->token.fileid=parent->token.fileid;
  return 0;
}

/* Spawn a node for some expression embedded in a grave string.
 * (src) begins with "${", and consumes the corresponding "}" on success.
 * Returns length consumed.
 */
 
static int mf_js_spawn_grave_expression(struct mf_node *parent,struct eggdev_minify_js *ctx,const char *src,int srcc,struct mf_token *logtoken) {
  if ((srcc<2)||memcmp(src,"${",2)) return -1;
  struct mf_token_reader reader={
    .v=src+2,
    .c=srcc-2,
    .fileid=parent->token.fileid,
  };
  int err=mf_js_compile_expression(parent,ctx,&reader);
  if (err<0) return err;
  if (reader.p>=reader.c) return mf_jserr(ctx,logtoken,"Unexpected EOF in grave expression.");
  if (reader.v[reader.p]!='}') return mf_jserr(ctx,logtoken,"Expected '}'");
  return 2+reader.p+1;
}

/* Grave string.
 * (node)'s token is the string, and we overwrite (node) with the split-out concatenation op.
 */
 
static int mf_js_compile_gravestring(struct mf_node *node,struct eggdev_minify_js *ctx) {
  int err;

  /* Yoink the token and confirm it's a grave string, then lop off the quotes.
   */
  struct mf_token token0=node->token;
  const char *src=token0.v;
  int srcc=token0.c;
  if ((srcc<2)||(src[0]!='`')||(src[srcc-1]!='`')) return -1;
  src++;
  srcc-=2;
  
  /* Measure the first raw text extent (including escape sequences).
   * If it's the entire token, rephrase the node as VALUE, ie string, and we're done.
   */
  int srcp=0;
  while (srcp<srcc) {
    if ((srcp<=srcc-2)&&(src[srcp]=='$')&&(src[srcp+1]=='{')) break;
    if (src[srcp]=='\\') srcp+=2;
    else srcp+=1;
  }
  if (srcp>=srcc) {
    node->type=MF_NODE_TYPE_VALUE;
    return 0;
  }
  
  /* An expression exists. Rephrase the node as binary add, and create a string node for the initial text.
   */
  node->type=MF_NODE_TYPE_OP;
  node->token.v="+";
  node->token.c=1;
  if ((err=mf_js_spawn_string_node_unquoted(node,ctx,src,srcp))<0) return err;
  
  /* Keep reading to the end.
   * At each "${", create a new token reader to spawn an expression.
   * We can add arbitrarily many operands, as long as there's at least two.
   */
  for (;;) {
    int plainp=srcp;
    while (srcp<srcc) {
      if ((srcp<=srcc-2)&&(src[srcp]=='$')&&(src[srcp+1]=='{')) break;
      if (src[srcp]=='\\') srcp+=2;
      else srcp+=1;
    }
    if (plainp<srcp) {
      if ((err=mf_js_spawn_string_node_unquoted(node,ctx,src+plainp,srcp-plainp))<0) return err;
    }
    if (srcp>=srcc) break;
    if ((err=mf_js_spawn_grave_expression(node,ctx,src+srcp,srcc-srcp,&token0))<0) return err;
    if (!err) return -1;
    srcp+=err;
  }
      
  return 0;
}

/* Test for lambdas.
 * (reader) must have just emitted "(".
 * We'll read ahead to check for "=>", then return to beginning position.
 */
 
static int mf_js_is_lambda(struct mf_token_reader *reader) {
  struct mf_token_reader save=*reader;
  struct mf_token token;
  int err,result=0;
  for (;;) {
    if ((err=mf_token_reader_next(&token,reader,0))<=0) {
      *reader=save;
      return 0;
    }
    if ((token.c==1)&&(token.v[0]==')')) break;
    if (token.type==MF_TOKEN_TYPE_IDENTIFIER) ;
    else if ((token.c==1)&&(token.v[0]=='(')) ;
    else if ((token.c==1)&&(token.v[0]==',')) ;
    else if ((token.c==3)&&!memcmp(token.v,"...",3)) ;
    else break;
  }
  if ((err=mf_token_reader_next(&token,reader,0))>0) {
    if ((token.c==2)&&!memcmp(token.v,"=>",2)) result=1;
  }
  *reader=save;
  return result;
}

/* Body of lambda, after the rocket.
 */
 
static int mf_js_compile_lambda_body(struct mf_node *node,struct eggdev_minify_js *ctx,struct mf_token_reader *reader) {
  struct mf_token token;
  int err=mf_token_reader_next(&token,reader,ctx);
  if (err<0) return err;
  mf_token_reader_unread(reader);
  if ((token.c==1)&&(token.v[0]=='{')) {
    return mf_js_compile_statement(node,ctx,reader);
  } else {
    return mf_js_compile_expression_with_limit(node,ctx,reader,MF_OPCLS_SEQ+2);
  }
}

/* Inline object.
 * (node->token) is set but it's otherwise fresh.
 * (reader) is positioned right after the opening "{" and must consume the closing "}".
 */
 
static int mf_js_compile_object(struct mf_node *node,struct eggdev_minify_js *ctx,struct mf_token_reader *reader) {
  int err;
  struct mf_token token;
  node->type=MF_NODE_TYPE_OBJECT;
  for (;;) {
  
    if ((err=mf_token_reader_next(&token,reader,ctx))<0) return err;
    if ((token.c==1)&&(token.v[0]=='}')) return 0;
    mf_token_reader_unread(reader);
    
    struct mf_node *field=mf_node_spawn(node);
    if (!field) return -1;
    field->type=MF_NODE_TYPE_FIELD;
    field->token=token;
    if ((err=mf_js_compile_expression_with_limit(field,ctx,reader,MF_OPCLS_SEQ+2))<0) return err;
    
    if ((err=mf_token_reader_next(&token,reader,ctx))<0) return err;
    if ((token.c==1)&&(token.v[0]==':')) {
      if ((err=mf_js_compile_expression_with_limit(field,ctx,reader,MF_OPCLS_SEQ+2))<0) return err;
      if ((err=mf_token_reader_next(&token,reader,ctx))<0) return err;
    }
    
    if ((token.c==1)&&(token.v[0]=='}')) return 0;
    if ((token.c!=1)||(token.v[0]!=',')) return mf_jserr(ctx,&token,"Expected ',' or '}'.");
  }
}

/* Inline array.
 * (node->token) is set but it's otherwise fresh.
 * (reader) is positioned right after the opening "[" and must consume the closing "]".
 */
 
static int mf_js_compile_array(struct mf_node *node,struct eggdev_minify_js *ctx,struct mf_token_reader *reader) {
  int err;
  struct mf_token token;
  node->type=MF_NODE_TYPE_ARRAY;
  for (;;) {
  
    // We have to check for close both fore and aft, to allow a trailing comma.
    if ((err=mf_token_reader_next(&token,reader,ctx))<0) return err;
    if ((token.c==1)&&(token.v[0]==']')) return 0;
    mf_token_reader_unread(reader);
    
    if ((err=mf_js_compile_expression_with_limit(node,ctx,reader,MF_OPCLS_SEQ+2))<0) return err;
    if ((err=mf_token_reader_next(&token,reader,ctx))<0) return err;
    if ((token.c==1)&&(token.v[0]==']')) return 0;
    if ((token.c!=1)||(token.v[0]!=',')) return mf_jserr(ctx,&token,"Expected ',' or ']'.");
  }
}

/* Compile a primary expression, overwriting (node).
 */
 
int mf_js_compile_primary_expression(struct mf_node *node,struct eggdev_minify_js *ctx,struct mf_token_reader *reader) {
  int err;
  if ((err=mf_token_reader_next(&node->token,reader,ctx))<0) return err;
  if (!node->token.c) return mf_jserr(ctx,&node->token,"Expected expression before EOF.");
  
  /* Check for unary operators.
   * It's important that we do this before the general "VALUE" case, because some operators are expressed as identifiers.
   */
  int opcls=0;
  if (mf_measure_js_operator(node->token.v,node->token.c,&opcls)==node->token.c) {
    switch (opcls) {
      case MF_OPCLS_ADD: // Only two operators in this class, and they are both also unary.
      case MF_OPCLS_UNARY:
      case MF_OPCLS_FIX:
      case MF_OPCLS_NEW:
      {
        node->type=MF_NODE_TYPE_OP;
        node->argv[0]=opcls;
        struct mf_node *a=mf_node_spawn(node);
        return mf_js_compile_limited_expression(a,ctx,reader,opcls);
      }
    }
  }
  
  /* Inline non-lambda function.
   */
  if ((node->token.c==8)&&!memcmp(node->token.v,"function",8)) {
    node->type=MF_NODE_TYPE_FUNCTION;
    struct mf_token token;
    if ((err=mf_token_reader_next(&token,reader,ctx))<0) return err;
    if (token.type==MF_TOKEN_TYPE_IDENTIFIER) {
      node->token=token;
    } else {
      mf_token_reader_unread(reader);
    }
    if ((err=mf_js_compile_paramlist(node,ctx,reader))<0) return err;
    if (!mf_token_reader_next_is(reader,"{",1)) return mf_jserr(ctx,&reader->prev,"Expected function body.");
    return mf_js_compile_statement(node,ctx,reader);
  }
  
  /* "..." is kind of special.
   * It's convenient for us to treat it as a unary operator.
   */
  if ((node->token.c==3)&&!memcmp(node->token.v,"...",3)) {
    node->type=MF_NODE_TYPE_OP;
    node->argv[0]=MF_OPCLS_UNARY;
    struct mf_node *a=mf_node_spawn(node);
    return mf_js_compile_limited_expression(a,ctx,reader,MF_OPCLS_UNARY);
  }
  
  /* Identifier could be the start of a single-param lambda.
   * If it is, then the next token must be "=>".
   */
  if (node->token.type==MF_TOKEN_TYPE_IDENTIFIER) {
    struct mf_token next;
    if ((err=mf_token_reader_next(&next,reader,ctx))<0) return err;
    if ((next.c==2)&&!memcmp(next.v,"=>",2)) {
      struct mf_node *param=mf_node_spawn(node);
      if (!param) return -1;
      param->type=MF_NODE_TYPE_VALUE;
      param->token=node->token;
      node->token=next;
      node->type=MF_NODE_TYPE_LAMBDA;
      return mf_js_compile_lambda_body(node,ctx,reader);
    }
    mf_token_reader_unread(reader);
  }
  
  /* Identifiers, numbers, strings, and plain regexes are simple: we're done.
   */
  if (
    (node->token.type==MF_TOKEN_TYPE_IDENTIFIER)||
    (node->token.type==MF_TOKEN_TYPE_NUMBER)||
    (node->token.type==MF_TOKEN_TYPE_STRING)||
    (node->token.type==MF_TOKEN_TYPE_REGEX)
  ) {
    node->type=MF_NODE_TYPE_VALUE;
    return 0;
  }
  
  /* Grave strings get unrolled right here.
   */
  if (node->token.type==MF_TOKEN_TYPE_GRAVESTRING) {
    return mf_js_compile_gravestring(node,ctx);
  }
  
  /* Parenthesized expressions are complicated by lambdas.
   * At open paren, we need to read ahead to check for the rocket.
   */
  if ((node->token.c==1)&&(node->token.v[0]=='(')) {
    struct mf_token token;
    if (mf_js_is_lambda(reader)) {
      mf_token_reader_unread(reader);
      node->type=MF_NODE_TYPE_LAMBDA;
      if ((err=mf_js_compile_paramlist(node,ctx,reader))<0) return err;
      if ((err=mf_token_reader_next(&token,reader,ctx))<0) return err;
      if ((token.c!=2)||memcmp(token.v,"=>",2)) return mf_jserr(ctx,&token,"Expected '=>'");
      node->token=token;
      return mf_js_compile_lambda_body(node,ctx,reader);
    } else {
      if ((err=mf_js_compile_limited_expression(node,ctx,reader,MF_OPCLS_SEQ+2))<0) return err;
      if ((err=mf_token_reader_next(&token,reader,ctx))<0) return err;
      if ((token.c!=1)||(token.v[0]!=')')) return mf_jserr(ctx,&token,"Expected ')'");
    }
    return 0;
  }
  
  /* Inline object.
   */
  if ((node->token.c==1)&&(node->token.v[0]=='{')) {
    return mf_js_compile_object(node,ctx,reader);
  }
  
  /* Inline array.
   */
  if ((node->token.c==1)&&(node->token.v[0]=='[')) {
    return mf_js_compile_array(node,ctx,reader);
  }
  
  return mf_jserr(ctx,&node->token,"Expected expression before '%.*s'.",node->token.c,node->token.v);
}

/* Compile expression into existing node, with a precedence limit.
 */
 
int mf_js_compile_limited_expression(struct mf_node *node,struct eggdev_minify_js *ctx,struct mf_token_reader *reader,int opcls) {

  /* Start with a primary expression.
   */
  if (!node) return -1;
  if (node->childc) {
    return mf_jserr(ctx,&node->token,"!!! %s childc=%d",__func__,node->childc);
  }
  int err=mf_js_compile_primary_expression(node,ctx,reader);
  if (err<0) return err;
  
  /* Consume binary operators up to our precedence limit.
   */
  int rtl=mf_opcls_rtl(opcls);
  if (rtl) opcls++;
  for (;;) {
    struct mf_token token={0};
    if ((err=mf_token_reader_next(&token,reader,ctx))<0) return err;
    if (!err) return mf_jserr(ctx,&token,"Unexpected EOF");
    
    // Not an operator? Done.
    int nopcls=0;
    if (mf_measure_js_operator(token.v,token.c,&nopcls)!=token.c) {
      mf_token_reader_unread(reader);
      break;
    }
    
    // Precedence at or below the context precedence? Done. There will be some outer layer that consumes it.
    // Also non-operator things have precedence zero and will always return here.
    if (nopcls<=opcls) {
      mf_token_reader_unread(reader);
      break;
    }
    
    #define BODYSNATCH \
      struct mf_node *lvalue=mf_node_new(); \
      if (!lvalue|| \
        (mf_node_transfer(lvalue,node)<0)|| \
        (mf_node_add_child(node,lvalue,-1)<0) \
      ) { \
        mf_node_del(lvalue); \
        return -1; \
      } \
      mf_node_del(lvalue); \
      node->token=token; \
      memset(node->argv,0,sizeof(node->argv));
    
    /* Function call?
     */
    if (nopcls==MF_OPCLS_CALL) { // "(" or "?.("
      BODYSNATCH
      node->type=MF_NODE_TYPE_CALL;
      struct mf_token_reader save=*reader;
      if ((err=mf_token_reader_next(&token,reader,ctx))<0) return err;
      if (!err) return mf_jserr(ctx,&node->token,"Unclosed argument list.");
      if ((token.c==1)&&(token.v[0]==')')) {
        // No args, we're done.
      } else {
        *reader=save;
        for (;;) {
          if (mf_token_reader_next_is(reader,")",1)) {
            if ((err=mf_token_reader_next(&token,reader,ctx))<0) return err;
            break;
          }
          if ((err=mf_js_compile_expression_with_limit(node,ctx,reader,MF_OPCLS_SEQ+2))<0) return err;
          if ((err=mf_token_reader_next(&token,reader,ctx))<0) return err;
          if ((token.c==1)&&(token.v[0]==')')) break;
          if ((token.c==1)&&(token.v[0]==',')) continue;
        }
      }
      continue;
    }
    
    /* Index reference?
     */
    if (
      ((token.c==1)&&(token.v[0]=='['))||
      ((token.c==3)&&!memcmp(token.v,"?.[",3))
    ) {
      BODYSNATCH
      node->type=MF_NODE_TYPE_INDEX;
      if ((err=mf_js_compile_expression(node,ctx,reader))<0) return err;
      if ((err=mf_token_reader_next(&token,reader,ctx))<0) return err;
      if ((token.c!=1)||(token.v[0]!=']')) return mf_jserr(ctx,&token,"Expected ']'.");
      continue;
    }
    
    /* Ternary selection?
     */
    if ((token.c==1)&&(token.v[0]=='?')) {
      BODYSNATCH
      node->type=MF_NODE_TYPE_OP;
      node->argv[0]=MF_OPCLS_SELECT;
      if ((err=mf_js_compile_expression_with_limit(node,ctx,reader,MF_OPCLS_SELECT))<0) return err;
      if ((err=mf_token_reader_next(&token,reader,ctx))<0) return err;
      if ((token.c!=1)||(token.v[0]!=':')) return mf_jserr(ctx,&token,"Expected ':'.");
      if ((err=mf_js_compile_expression_with_limit(node,ctx,reader,MF_OPCLS_SELECT))<0) return err;
      continue;
    }
    
    /* Postfix?
     */
    if (nopcls==MF_OPCLS_FIX) {
      BODYSNATCH
      node->type=MF_NODE_TYPE_POSTFIX;
      continue;
    }
    
    /* Regular binary operators.
     * Make a node for this binary operation, copy (node) over it, and turn (node) into a binary operator.
     * And then reenter for the rvalue.
     */
    BODYSNATCH
    node->type=MF_NODE_TYPE_OP;
    node->argv[0]=nopcls;
    if ((err=mf_js_compile_expression_with_limit(node,ctx,reader,nopcls))<0) return err;
    
    #undef BODYSNATCH
  }
  return 0;
}

/* Compile expression into parent.
 * This is the usual non-recursive entry point.
 */
 
int mf_js_compile_expression_with_limit(struct mf_node *parent,struct eggdev_minify_js *ctx,struct mf_token_reader *reader,int opcls) {
  struct mf_node *node=mf_node_spawn(parent);
  int err=mf_js_compile_limited_expression(node,ctx,reader,opcls);
  if (err<0) return err;
  return 0;
}

/* Compile parenthesized expression.
 */
 
int mf_js_compile_parenthesized_expression(struct mf_node *parent,struct eggdev_minify_js *ctx,struct mf_token_reader *reader) {
  int err;
  struct mf_token token;
  if ((err=mf_token_reader_next(&token,reader,ctx))<0) return err;
  if ((token.c!=1)||(token.v[0]!='(')) return mf_jserr(ctx,&token,"Expected parenthesized expression.");
  if ((err=mf_js_compile_expression(parent,ctx,reader))<0) return err;
  if ((err=mf_token_reader_next(&token,reader,ctx))<0) return err;
  if ((token.c!=1)||(token.v[0]!=')')) return mf_jserr(ctx,&token,"Expected ')' to complete expression.");
  return 0;
}
