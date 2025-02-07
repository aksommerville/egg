#include "mf_internal.h"
#include <math.h>

/* Logical identity of Javascript primitive expression.
 */
 
static int mf_eval_lid(const char *src,int srcc) {
  if (!src||(srcc<1)) return 0;
  if ((srcc==3)&&!memcmp(src,"NaN",3)) return 0;
  if ((srcc==4)&&!memcmp(src,"null",4)) return 0;
  if ((srcc==4)&&!memcmp(src,"true",4)) return 1;
  if ((srcc==5)&&!memcmp(src,"false",5)) return 0;
  if ((srcc==9)&&!memcmp(src,"undefined",9)) return 0;
  if ((srcc==2)&&(src[0]==src[1])&&((src[0]=='"')||(src[0]=='\'')||(src[0]=='`'))) return 0;
  if (((src[0]>='0')&&(src[0]<='9'))||(src[0]=='.')) {
    int i=srcc; while (i-->0) {
      if (src[i]=='.') continue;
      if (src[i]!='0') return 1;
    }
    return 0;
  }
  return 1;
}

/* Expression type, private symbols.
 */
 
#define MF_JS_TYPE_UNKNOWN 0 /* not an expression, eg empty */
#define MF_JS_TYPE_NULL 1
#define MF_JS_TYPE_UNDEFINED 2
#define MF_JS_TYPE_BOOLEAN 3
#define MF_JS_TYPE_NUMBER 4
#define MF_JS_TYPE_STRING 5
#define MF_JS_TYPE_OBJECT 6 /* including function, and all else */

static int mf_js_expression_type(const char *src,int srcc) {
  if (!src||(srcc<1)) return MF_JS_TYPE_UNKNOWN;
  if ((srcc==4)&&!memcmp(src,"null",4)) return MF_JS_TYPE_NULL;
  if ((srcc==9)&&!memcmp(src,"undefined",9)) return MF_JS_TYPE_UNDEFINED;
  if ((srcc==4)&&!memcmp(src,"true",4)) return MF_JS_TYPE_BOOLEAN;
  if ((srcc==5)&&!memcmp(src,"false",5)) return MF_JS_TYPE_BOOLEAN;
  if ((srcc==3)&&!memcmp(src,"NaN",3)) return MF_JS_TYPE_NUMBER;
  if (src[0]=='"') return MF_JS_TYPE_STRING;
  if (src[0]=='\'') return MF_JS_TYPE_STRING;
  if (src[0]=='`') return MF_JS_TYPE_STRING;
  if ((src[0]>='0')&&(src[0]<='9')) return MF_JS_TYPE_NUMBER;
  if (src[0]=='.') return MF_JS_TYPE_NUMBER;
  if (src[0]=='-') return MF_JS_TYPE_NUMBER;
  return MF_JS_TYPE_OBJECT;
}

/* Convert primitive to number, text to text.
 */
 
static int mf_js_to_number(char *dst,int dsta,const char *src,int srcc) {
  if (srcc<1) return -1;
  if (((src[0]>='0')&&(src[0]<='9'))||(src[0]=='-')||(src[0]=='.')) {
    if (srcc<=dsta) memcpy(dst,src,srcc);
    return srcc;
  }
  if ((srcc==3)&&!memcmp(src,"NaN",3)) {
    if (srcc<=dsta) memcpy(dst,src,srcc);
    return srcc;
  }
  if ((srcc==4)&&!memcmp(src,"null",4)) {
    if (dsta>=1) dst[0]='0';
    return 1;
  }
  if ((srcc==4)&&!memcmp(src,"true",4)) {
    if (dsta>=1) dst[0]='1';
    return 1;
  }
  if ((srcc==5)&&!memcmp(src,"false",5)) {
    if (dsta>=1) dst[0]='0';
    return 1;
  }
  if ((srcc>=9)&&!memcmp(src,"undefined",9)) {
    if (dsta>=3) memcpy(dst,"NaN",3); // why is null 0 but undefined NaN? whyyyyyyyy
    return 3;
  }
  if ((src[0]=='"')||(src[0]=='\'')||(src[0]=='`')) {
    if (srcc<=2) { // Empty string is 0. Other errors are NaN.
      if (dsta>=1) dst[0]='0';
      return 1;
    }
    char tmp[64];
    int tmpc=sr_string_eval(tmp,sizeof(tmp),src,srcc);
    if ((tmpc<0)||(tmpc>sizeof(tmp))) return -1;
    int v;
    if (sr_int_eval(&v,tmp,tmpc)>=2) {
      return sr_decsint_repr(dst,dsta,v);
    }
    return -1;
  }
  return -1;
}

static int mf_js_to_int(char *dst,int dsta,const char *src,int srcc) {
  int dstc=mf_js_to_number(dst,dsta,src,srcc);
  if ((dstc<0)||(dstc>dsta)) return dstc;
  if (dst[0]=='.') {
    if (dsta>=1) dst[0]='0';
    return 1;
  }
  int i=1; for (;i<dstc;i++) { // Lop off the fraction if present.
    if (dst[i]=='.') return i;
  }
  return dstc;
}

/* Primitive to string, ie quoted string token.
 */
 
static int mf_js_to_string(char *dst,int dsta,const char *src,int srcc) {
  if (!src||(srcc<1)) return -1;
  if ((src[0]=='"')||(src[0]=='\'')||(src[0]=='`')) {
    if (srcc<=dsta) memcpy(dst,src,srcc);
    return srcc;
  }
  int dstc=2+srcc;
  if (dstc<=dsta) {
    dst[0]='"';
    memcpy(dst+1,src,srcc);
    dst[srcc+1]='"';
  }
  return dstc;
}

/* Perform unary operator, text to text.
 */
 
static int mf_node_eval_unary(char *dst,int dsta,const char *op,int opc,const char *a,int ac) {

  /* LID and LNT are super easy.
   */
  int lid=-1;
  if ((opc==1)&&(op[0]=='!')) lid=0;
  else if ((opc==2)&&!memcmp(op,"!!",2)) lid=1;
  if (lid>=0) {
    int actual=mf_eval_lid(a,ac);
    if (lid==actual) {
      if (dsta>=4) memcpy(dst,"true",4);
      return 4;
    } else {
      if (dsta>=5) memcpy(dst,"false",5);
      return 5;
    }
  }
  
  /* "void" is a unary operator, and its output is "undefined" regardless of input.
   */
  if ((opc==4)&&!memcmp(op,"void",4)) {
    if (dsta>=9) memcpy(dst,"undefined",9);
    return 9;
  }
  
  /* "typeof" doesn't require much inspection.
   * I pretend it's a function when writing JS, but in truth it's an operator.
   */
  if ((opc==6)&&!memcmp(op,"typeof",6)) {
    if (ac<1) return -1;
    const char *src=0;
    if ((a[0]>='0')&&(a[0]<='9')) src="number";
    else if (a[0]=='.') src="number";
    else if ((ac==3)&&!memcmp(a,"NaN",3)) src="number"; // "not a number"? oh that's a "number".
    else if ((a[0]=='"')||(a[0]=='\'')||(a[0]=='`')) src="string";
    else if ((a[0]=='{')||(a[0]=='[')) src="object";
    else if ((ac==4)&&!memcmp(a,"null",4)) src="object"; // whyyyyyyy
    else if ((ac==9)&&!memcmp(a,"undefined",9)) src="undefined";
    else if ((ac==4)&&!memcmp(a,"true",4)) src="boolean";
    else if ((ac==5)&&!memcmp(a,"false",5)) src="boolean";
    if (!src) return -1;
    int srcc=0; while (src[srcc]) srcc++;
    if (2+srcc<=dsta) {
      dst[0]='"';
      memcpy(dst+1,src,srcc);
      dst[1+srcc]='"';
    }
    return 2+srcc;
  }
  
  /* "+" and "-" convert to number, then "-" flips the sign.
   */
  if ((opc==1)&&((op[0]=='+')||(op[0]=='-'))) {
    char tmp[64];
    int tmpc=mf_js_to_number(tmp,sizeof(tmp),a,ac);
    if ((tmpc<1)||(tmpc>sizeof(tmp))) return -1;
    if (op[0]=='+') {
      if (tmpc<=dsta) memcpy(dst,tmp,tmpc);
      return tmpc;
    } else if (op[0]=='-') {
      if (tmp[0]=='-') {
        int dstc=tmpc-1;
        if (dstc<=dsta) memcpy(dst,tmp+1,dstc);
        return dstc;
      } else {
        int dstc=tmpc+1;
        if (dstc<=dsta) {
          dst[0]='-';
          memcpy(dst+1,tmp,tmpc);
        }
        return dstc;
      }
    }
  }
  
  /* "~" converts to integer and inverts.
   */
  if ((opc==1)&&(op[0]=='~')) {
    char tmp[64];
    int tmpc=mf_js_to_int(tmp,sizeof(tmp),a,ac);
    if ((tmpc<1)||(tmpc>sizeof(tmp))) return -1;
    int v;
    if (sr_int_eval(&v,tmp,tmpc)>=1) {
      v=~v;
      return sr_decsint_repr(dst,dsta,v);
    }
    return -1;
  }

  return -1;
}

/* Binary operator on two strings.
 */
 
static int mf_node_eval_binary_string(char *dst,int dsta,const char *a,int ac,const char *op,int opc,const char *b,int bc) {
  // The only one I care about is "+".
  if ((opc==1)&&(op[0]=='+')) {
    // It's tempting to just strip quotes and glue the tokens together.
    // But that could mess up if we have say ("'" + '"' + "`"). Any way you slice it, the output needs to escape something that wasn't escaped initially.
    char tmp[1024];
    int tmpc=sr_string_eval(tmp,sizeof(tmp),a,ac);
    if ((tmpc<0)||(tmpc>sizeof(tmp))) return -1;
    int err=sr_string_eval(tmp+tmpc,sizeof(tmp)-tmpc,b,bc);
    if (err<0) return -1;
    tmpc+=err;
    if (tmpc>sizeof(tmp)) return -1;
    //TODO sr_string_repr always uses quote. Is it worth rewriting, to use apostrophe for strings containing a quote?
    // It has to be that way because it guarantees JSON-legal. But JS string repr isn't super complicated, we could do it separate.
    return sr_string_repr(dst,dsta,tmp,tmpc);
  }
  return -1;
}

/* Binary operator on numbers, no eval/repr.
 * Return:
 *  - -1 if can't compute.
 *  - 0 if result is the input type and we got it.
 *  - 1 if result is boolean (0=false, 1=true).
 *  - 2 if result is nan (from int only)
 */
 
static int mf_js_float_binop(double *dst,double a,const char *op,int opc,double b) {
  switch (opc) {
    case 1: switch (op[0]) {
        case '+': *dst=a+b; return 0;
        case '-': *dst=a-b; return 0;
        case '*': *dst=a*b; return 0;
        case '/': *dst=a/b; return 0;
        case '%': *dst=fmod(a,b); return 0;
        case '<': *dst=(a<b)?1.0:0.0; return 1;
        case '>': *dst=(a>b)?1.0:0.0; return 1;
      } break;
    case 2: if (op[0]==op[1]) switch (op[0]) {
        case '*': *dst=pow(a,b); return 0;
        case '=': *dst=(a==b)?1.0:0.0; return 1;
        case '&': { int acls=fpclassify(a),bcls=fpclassify(b); acls=(acls!=FP_ZERO)&&(acls!=FP_NAN); bcls=(bcls!=FP_ZERO)&&(bcls!=FP_NAN); *dst=(acls&&bcls)?1.0:0.0; } return 1;
        case '|': { int acls=fpclassify(a),bcls=fpclassify(b); acls=(acls!=FP_ZERO)&&(acls!=FP_NAN); bcls=(bcls!=FP_ZERO)&&(bcls!=FP_NAN); *dst=(acls||bcls)?1.0:0.0; } return 1;
      } else if (op[1]=='=') switch (op[0]) {
        case '!': *dst=(a!=b)?1.0:0.0; return 1;
        case '<': *dst=(a<=b)?1.0:0.0; return 1;
        case '>': *dst=(a>=b)?1.0:0.0; return 1;
      } break;
    case 3: {
        if (!memcmp(op,"===",3)) { *dst=(a==b)?1.0:0.0; return 1; }
        if (!memcmp(op,"!==",3)) { *dst=(a!=b)?1.0:0.0; return 1; }
      } break;
  }
  return -1;
}

static int mf_js_int_binop(int *dst,int a,const char *op,int opc,int b) {
  switch (opc) {
    case 1: switch (op[0]) {
        case '+': *dst=a+b; return 0;
        case '-': *dst=a-b; return 0;
        case '*': *dst=a*b; return 0;
        case '/': if (!b) return 2; *dst=a/b; return 0;
        case '%': if (!b) return 2; *dst=a%b; return 0;
        case '&': *dst=a&b; return 0;
        case '|': *dst=a|b; return 0;
        case '^': *dst=a^b; return 0;
        case '<': *dst=(a<b)?1:0; return 1;
        case '>': *dst=(a>b)?1:0; return 1;
      } break;
    case 2: if (op[0]==op[1]) switch (op[0]) {
        case '*': *dst=(int)pow(a,b); return 0;
        case '<': *dst=a<<b; return 0;
        case '>': *dst=a>>b; return 0;
        case '=': *dst=(a==b)?1:0; return 1;
        case '&': *dst=(a&&b)?1:0; return 1;
        case '|': *dst=(a||b)?1:0; return 1;
      } else if (op[1]=='=') switch (op[0]) {
        case '!': *dst=(a!=b)?1:0; return 1;
        case '<': *dst=(a<=b)?1:0; return 1;
        case '>': *dst=(a>=b)?1:0; return 1;
      } break;
    case 3: {
        if (!memcmp(op,">>>",3)) { *dst=((unsigned int)a)>>b; return 0; }
        if (!memcmp(op,"===",3)) { *dst=(a==b)?1:0; return 1; }
        if (!memcmp(op,"!==",3)) { *dst=(a!=b)?1:0; return 1; }
      } break;
  }
  return -1;
}

/* Binary operator on two numbers.
 */
 
static int boolrepr(char *dst,int dsta,int v) {
  if (v) {
    if (dsta>=4) memcpy(dst,"true",4);
    return 4;
  } else {
    if (dsta>=5) memcpy(dst,"false",5);
    return 5;
  }
}
 
static int mf_node_eval_binary_number(char *dst,int dsta,const char *a,int ac,const char *op,int opc,const char *b,int bc) {
  int i;
  int afloat=0,bfloat=0;
  for (i=ac;i-->0;) if (a[i]=='.') { afloat=1; break; }
  for (i=bc;i-->0;) if (b[i]=='.') { bfloat=1; break; }
  if (afloat||bfloat) {
    double av,bv,cv;
    if (sr_double_eval(&av,a,ac)<0) return -1;
    if (sr_double_eval(&bv,b,bc)<0) return -1;
    switch (mf_js_float_binop(&cv,av,op,opc,bv)) {
      case 0: return sr_double_repr(dst,dsta,cv);//TODO sr_double_repr might not be suitable re inf and nan.
      case 1: return boolrepr(dst,dsta,cv);
    }
  }
  int av,bv,cv;
  if (sr_int_eval(&av,a,ac)<1) return -1;
  if (sr_int_eval(&bv,b,bc)<1) return -1;
  switch (mf_js_int_binop(&cv,av,op,opc,bv)) {
    case 0: return sr_decsint_repr(dst,dsta,cv);
    case 1: return boolrepr(dst,dsta,cv);
    case 2: if (dsta>=3) memcpy(dst,"NaN",3); return 3;
  }
  return -1;
}

/* Perform binary operator, text to text.
 */
 
static int mf_node_eval_binary(char *dst,int dsta,const char *a,int ac,const char *op,int opc,const char *b,int bc) {
  if ((ac<1)||(bc<1)) return -1;
  
  int promotable=1;
  if (
    ((opc==3)&&!memcmp(op,"===",3))||
    ((opc==3)&&!memcmp(op,"!==",3))
  ) promotable=0;
  
  int atype=mf_js_expression_type(a,ac);
  int btype=mf_js_expression_type(b,bc);

  // string against string, promoting if needed.
  if ((atype==MF_JS_TYPE_STRING)||(btype==MF_JS_TYPE_STRING)) {
    char tmp[1024];
    if (promotable) {
      if (atype!=MF_JS_TYPE_STRING) {
        int tmpc=mf_js_to_string(tmp,sizeof(tmp),a,ac);
        if ((tmpc<1)||(tmpc>sizeof(tmp))) return -1;
        a=tmp;
        ac=tmpc;
      } else if (btype!=MF_JS_TYPE_STRING) {
        int tmpc=mf_js_to_string(tmp,sizeof(tmp),b,bc);
        if ((tmpc<1)||(tmpc>sizeof(tmp))) return -1;
        b=tmp;
        bc=tmpc;
      }
    } else {
      if ((atype!=MF_JS_TYPE_STRING)||(btype!=MF_JS_TYPE_STRING)) return -1;
    }
    return mf_node_eval_binary_string(dst,dsta,a,ac,op,opc,b,bc);
  }
  
  // number against number, promoting if needed.
  if ((atype==MF_JS_TYPE_NUMBER)||(btype==MF_JS_TYPE_NUMBER)) {
    char tmp[1024];
    if (promotable) {
      if (atype!=MF_JS_TYPE_NUMBER) {
        int tmpc=mf_js_to_number(tmp,sizeof(tmp),a,ac);
        if ((tmpc<1)||(tmpc>sizeof(tmp))) return -1;
        a=tmp;
        ac=tmpc;
      } else if (btype!=MF_JS_TYPE_NUMBER) {
        int tmpc=mf_js_to_number(tmp,sizeof(tmp),b,bc);
        if ((tmpc<1)||(tmpc>sizeof(tmp))) return -1;
        b=tmp;
        bc=tmpc;
      }
    } else {
      if ((atype!=MF_JS_TYPE_NUMBER)||(btype!=MF_JS_TYPE_NUMBER)) return -1;
    }
    return mf_node_eval_binary_number(dst,dsta,a,ac,op,opc,b,bc);
  }
    
  // There's plenty of other constant cases. eg "true/{}" => "NaN".
  // But I think it's string-string and number-number that are actually going to happen.
  return -1;
}

/* Evaluate node, main entry point.
 */
 
/* TODO
 * Here's a bit from Rom.decodeBase64, after minification:
 *   ch=ch-48+52;
 * Obviously that could turn into:
 *   ch+=4;
 * But it doesn't, because we have:
 *   +(-(ch,48),52)
 * Because the '-' is variable, we don't combine with the outer '+' operand.
 * ...then turning "ch=ch+4" into "ch+=4" is a different problem that we also should solve.
 */
 
int mf_node_eval(char *dst,int dsta,struct eggdev_minify_js *ctx,struct mf_node *node) {
  if (!node) return -1;

  /* VALUE might be constant by nature.
   * But we have to be careful about identifiers.
   */
  if (node->type==MF_NODE_TYPE_VALUE) {
    if (node->token.type==MF_TOKEN_TYPE_IDENTIFIER) {
           if ((node->token.c==4)&&!memcmp(node->token.v,"null",4)) ;
      else if ((node->token.c==9)&&!memcmp(node->token.v,"undefined",9)) ;
      else if ((node->token.c==4)&&!memcmp(node->token.v,"true",4)) ;
      else if ((node->token.c==5)&&!memcmp(node->token.v,"false",5)) ;
      else if ((node->token.c==3)&&!memcmp(node->token.v,"NaN",3)) ;
      else {
        struct mf_node *dfld=mf_node_lookup_symbol(node,node->token.v,node->token.c);
        struct mf_node *decl=mf_node_ancestor_of_type(dfld,MF_NODE_TYPE_DECL);
        if (decl&&(decl->token.c==5)&&!memcmp(decl->token.v,"const",5)) {
          // "let" declarations could also be constant, if we can prove they aren't reassigned before this reference.
          // But that's too complicated for me.
          struct mf_node *v=mf_node_get_symbol_initializer(dfld);
          if (!v) { // No initializer means undefined, which is constant.
            if (dsta>=9) memcpy(dst,"undefined",9);
            return 9;
          }
          return mf_node_eval(dst,dsta,ctx,v);
        }
        // Any other identifier, assume it's variable.
        return -1;
      }
    }
    // Any other VALUE eg string and number, assume it's constant.
    if (node->token.c<=dsta) memcpy(dst,node->token.v,node->token.c);
    return node->token.c;
  }
  
  /* OP are what we're mostly interested in.
   */
  if (node->type==MF_NODE_TYPE_OP) {
    switch (node->childc) {
      case 0: return -1;
      case 1: {
          char rvalue[1024];
          int rvaluec=mf_node_eval(rvalue,sizeof(rvalue),ctx,node->childv[0]);
          if ((rvaluec>0)&&(rvaluec<=sizeof(rvalue))) {
            return mf_node_eval_unary(dst,dsta,node->token.v,node->token.c,rvalue,rvaluec);
          }
        } break;
      case 2: {
          char lvalue[1024],rvalue[1024];
          int lvaluec=mf_node_eval(lvalue,sizeof(lvalue),ctx,node->childv[0]);
          if ((lvaluec>0)&&(lvaluec<=sizeof(lvalue))) {
            int rvaluec=mf_node_eval(rvalue,sizeof(rvalue),ctx,node->childv[1]);
            if ((rvaluec>0)&&(rvaluec<=sizeof(rvalue))) {
              return mf_node_eval_binary(dst,dsta,lvalue,lvaluec,node->token.v,node->token.c,rvalue,rvaluec);
            }
          }
        } break;
      default: {
          if ((node->token.c==1)&&(node->token.v[0]=='?')&&(node->childc==3)) {
            // Selection is simple, implement right here.
            char q[1024];
            int qc=mf_node_eval(q,sizeof(q),ctx,node->childv[0]);
            if ((qc>0)&&(qc<=sizeof(q))) {
              if (mf_eval_lid(q,qc)) {
                return mf_node_eval(dst,dsta,ctx,node->childv[1]);
              } else {
                return mf_node_eval(dst,dsta,ctx,node->childv[2]);
              }
            }
          } else {
            // All other >2-operand OP nodes are chained binary ops.
            int dstc=mf_node_eval(dst,dsta,ctx,node->childv[0]);
            if ((dstc>0)&&(dstc<=dsta)) {
              int i=1; for (;i<node->childc;i++) {
                struct mf_node *child=node->childv[i];
                char rvalue[1024];
                int rvaluec=mf_node_eval(rvalue,sizeof(rvalue),ctx,child);
                if ((rvaluec<1)||(rvaluec>sizeof(rvalue))) return -1;
                char tmp[1024];
                int tmpc=mf_node_eval_binary(tmp,sizeof(tmp),dst,dstc,node->token.v,node->token.c,rvalue,rvaluec);
                if ((tmpc<1)||(tmpc>sizeof(tmp))) return -1;
                memcpy(dst,tmp,tmpc);
                dstc=tmpc;
              }
              return dstc;
            }
          }
        }
    }
    return -1;
  }
  
  //TODO CALL
  //TODO INDEX
  //TODO ARRAY
  //TODO OBJECT
  return -1;
}
