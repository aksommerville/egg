#include "mf_internal.h"

/* Any complex expression with a constant result, replace it with the outcome.
 * This is probably the fanciest thing we do, since we have to implement a big chunk of the JS runtime.
 * It's also one of the most obvious: "1+1" should emit as "2".
 */
 
static int mf_resolve_expressions(struct eggdev_minify_js *ctx,struct mf_node *node) {
  int err,i;
  switch (node->type) {
    case MF_NODE_TYPE_OP:
    case MF_NODE_TYPE_CALL:
    case MF_NODE_TYPE_INDEX: {
        char tmp[1024];
        int tmpc=mf_node_eval(tmp,sizeof(tmp),ctx,node);
        if ((tmpc>0)&&(tmpc<=sizeof(tmp))) {
          char *nv=mf_js_text_intern(ctx,tmp,tmpc);
          if (!nv) return -1;
          node->type=MF_NODE_TYPE_VALUE;
          node->token.v=nv;
          node->token.c=tmpc;
          if ((nv[0]>='0')&&(nv[0]<='9')) node->token.type=MF_TOKEN_TYPE_NUMBER;
          else if (nv[0]=='-') node->token.type=MF_TOKEN_TYPE_NUMBER;
          else if ((nv[0]=='"')||(nv[0]=='\'')) node->token.type=MF_TOKEN_TYPE_STRING;
          mf_node_remove_all_children(node);
        }
      } break;
  }
  for (i=0;i<node->childc;i++) {
    if ((err=mf_resolve_expressions(ctx,node->childv[i]))<0) return err;
  }
  return 0;
}

/* Examine every constant expression, and if we can rephrase it shorter, do so.
 */
 
static int mf_reduce_constants(struct eggdev_minify_js *ctx,struct mf_node *node) {
  int err,i;
  if ((node->type==MF_NODE_TYPE_VALUE)&&(node->token.c>0)) {
    char ch0=node->token.v[0];
    
    if ((ch0>='0')&&(ch0<='9')) {
      int isfloat=0,i=node->token.c;
      while (i-->0) if (node->token.v[i]=='.') { isfloat=1; break; }
      if (isfloat) {
        // Eliminate leading and trailing zeroes. If we end up with a trailing dot, drop that too.
        // In JS it is not useful to ".0" a number to declare it floating-point; they're all implicitly floating-point.
        while (node->token.c&&(node->token.v[node->token.c-1]=='0')) node->token.c--;
        while (node->token.c&&(node->token.v[0]=='0')) { node->token.v++; node->token.c--; }
        if (node->token.c&&(node->token.v[node->token.c-1]=='.')) node->token.c--;
        if (!node->token.c) { // oops oops, put it back!
          node->token.v="0";
          node->token.c=1;
        }
      } else {
        // Evaluate integer, then rerepr it decimal.
        int v;
        if (sr_int_eval(&v,node->token.v,node->token.c)>=2) {
          char tmp[16];
          int tmpc=sr_decsint_repr(tmp,sizeof(tmp),v);
          if ((tmpc>0)&&(tmpc<=sizeof(tmp))&&(tmpc<node->token.c)) {
            char *nv=mf_js_text_intern(ctx,tmp,tmpc);
            if (nv) {
              node->token.v=nv;
              node->token.c=tmpc;
            }
          }
        }
      }
      
    } else if ((node->token.c==9)&&!memcmp(node->token.v,"undefined",9)) {
      node->token.v="{}._";
      node->token.c=4;
      
    } else if ((node->token.c==4)&&!memcmp(node->token.v,"true",4)) {
      node->token.v="!0";
      node->token.c=2;
      
    } else if ((node->token.c==5)&&!memcmp(node->token.v,"false",5)) {
      node->token.v="!1";
      node->token.c=2;
    
    }
    
    //TODO Are strings worth examining? There are things that can reduce, eg "\u00a0"=>"\n".
    // Or if it has escaped quote or apostrophe maybe we can change the quote character.
    // Also, we have fairly large strings of GLSL text. Is it worth rewriting those too?
  }
  for (i=0;i<node->childc;i++) {
    if ((err=mf_reduce_constants(ctx,node->childv[i]))<0) return err;
  }
  return 0;
}

/* Search for all member references "a.b" and if (b) is long enough, and appears more than once,
 * replace with "a[c]" and define (c) at the top, in a big destructured String.split().
 */
 
static int mf_reduce_member_names_declare(struct eggdev_minify_js *ctx,struct mf_node *array,const char *str,int strc) {
  struct mf_node *decl=mf_node_spawn_at(ctx->root,0);
  struct mf_node *field=mf_node_spawn(decl);
  if (mf_node_add_child(field,array,0)<0) return -1;
  struct mf_node *call=mf_node_spawn(field);
  struct mf_node *deref=mf_node_spawn(call);
  struct mf_node *comma=mf_node_spawn(call);
  struct mf_node *string=mf_node_spawn(deref);
  struct mf_node *split=mf_node_spawn(deref);
  if (!decl||!call||!deref||!comma||!string||!split) return -1;
  
  decl->type=MF_NODE_TYPE_DECL;
  decl->token.v="const";
  decl->token.c=5;
  decl->token.type=MF_TOKEN_TYPE_IDENTIFIER;
  
  field->type=MF_NODE_TYPE_OP;
  field->token.v="=";
  field->token.c=1;
  field->token.type=MF_TOKEN_TYPE_OPERATOR;
  
  call->type=MF_NODE_TYPE_CALL;
  call->token.v="(";
  call->token.c=1;
  call->token.type=MF_TOKEN_TYPE_OPERATOR;
  
  deref->type=MF_NODE_TYPE_OP;
  deref->token.v=".";
  deref->token.c=1;
  deref->token.type=MF_TOKEN_TYPE_OPERATOR;
  
  comma->type=MF_NODE_TYPE_VALUE;
  comma->token.v="','";
  comma->token.c=3;
  comma->token.type=MF_TOKEN_TYPE_STRING;
  
  string->type=MF_NODE_TYPE_VALUE;
  if (!(string->token.v=mf_js_text_intern(ctx,str,strc))) return -1;
  string->token.c=strc;
  string->token.type=MF_TOKEN_TYPE_STRING;
  
  split->type=MF_NODE_TYPE_VALUE;
  split->token.v="split";
  split->token.c=5;
  split->token.type=MF_TOKEN_TYPE_IDENTIFIER;

  return 0;
}
 
static int mf_reduce_member_names_filter(struct mf_node *node,void *userdata) {
  if (node->parent&&(node->parent->type==MF_NODE_TYPE_OP)&&(node->type==MF_NODE_TYPE_VALUE)&&(node->token.type==MF_TOKEN_TYPE_IDENTIFIER)) {
    if (node->token.c<4) return 0; // Too short to be worth changing.
    if ((node->parent->childc==2)&&(node->parent->childv[1]==node)) {
      if ((node->parent->token.c==1)&&(node->parent->token.v[0]=='.')) return 1;
      if ((node->parent->token.c==2)&&!memcmp(node->parent->token.v,"?.",2)) return 1;
    }
  }
  return 0;
}

static int mf_reduce_member_names_cmp(const void *A,const void *B) {
  const struct mf_node *a=*(void**)A,*b=*(void**)B;
  if (a->token.c<b->token.c) return -1;
  if (a->token.c>b->token.c) return 1;
  return memcmp(a->token.v,b->token.v,a->token.c);
}
 
static int mf_reduce_member_names(struct eggdev_minify_js *ctx) {
  struct mf_nodelist *nl=mf_find_nodes(ctx->root,mf_reduce_member_names_filter,0);
  if (!nl) return -1;
  
  qsort(nl->v,nl->c,sizeof(void*),mf_reduce_member_names_cmp);
  struct mf_node *array=mf_node_new();
  if (!array) {
    mf_nodelist_del(nl);
    return -1;
  }
  array->type=MF_NODE_TYPE_ARRAY;
  array->token.v="[";
  array->token.c=1;
  array->token.type=MF_TOKEN_TYPE_OPERATOR;
  struct sr_encoder bigstr={0};
  if (sr_encode_u8(&bigstr,'"')<0) {
    mf_nodelist_del(nl);
    mf_node_del(array);
    return -1;
  }
  int p=0;
  while (p<nl->c) {
    int c=1;
    while ((p+c<nl->c)&&(nl->v[p]->token.c==nl->v[p+1]->token.c)&&!memcmp(nl->v[p]->token.v,nl->v[p+c]->token.v,nl->v[p]->token.c)) c++;
    if (c>1) { // It's not worth moving one symbol. But at 2 or more, we may benefit from moving. (not worth figuring out the exact formula).
      int namec=0;
      char *nname=mf_next_identifier(ctx,&namec);
      if (!nname||(sr_encode_fmt(&bigstr,"%.*s,",nl->v[p]->token.c,nl->v[p]->token.v)<0)) {
        mf_nodelist_del(nl);
        sr_encoder_cleanup(&bigstr);
        mf_node_del(array);
        return -1;
      }
      struct mf_node *sym=mf_node_spawn(array);
      if (sym) {
        sym->type=MF_NODE_TYPE_VALUE;
        sym->token.v=nname;
        sym->token.c=namec;
        sym->token.type=MF_TOKEN_TYPE_IDENTIFIER;
      }
      int ii=c; while (ii-->0) {
        struct mf_node *node=nl->v[p+ii];
        node->token.v=nname;
        node->token.c=namec;
        node->parent->type=MF_NODE_TYPE_INDEX;
        if (node->parent->token.c==1) {
          node->parent->token.v="[";
        } else {
          node->parent->token.v="?.[";
          node->parent->token.c=3;
        }
      }
    }
    p+=c;
  }
  mf_nodelist_del(nl);
  if (bigstr.c<2) {
    sr_encoder_cleanup(&bigstr);
    mf_node_del(array);
    return 0;
  }
  bigstr.c--; // We'll have produced an unnecessary trailing comma.
  if (sr_encode_u8(&bigstr,'"')<0) {
    sr_encoder_cleanup(&bigstr);
    mf_node_del(array);
    return -1;
  }
  int err=mf_reduce_member_names_declare(ctx,array,bigstr.v,bigstr.c);
  sr_encoder_cleanup(&bigstr);
  mf_node_del(array);
  return err;
}

/* Everything declared by the script (pretty much) can have its name replaced.
 * Replace them with the smallest possible identifiers.
 * We'll replace eligible symbols even if they are already single characters, since they would collide with some other replacement.
 */
 
static int mf_rename_all_within(struct eggdev_minify_js *ctx,struct mf_node *root,const char *to,int toc,const char *from,int fromc) {
  if ((root->type==MF_NODE_TYPE_VALUE)&&(root->token.type==MF_TOKEN_TYPE_IDENTIFIER)) {
    if ((root->token.c==fromc)&&!memcmp(root->token.v,from,fromc)) {
  
      if (root->parent) {
        struct mf_node *mom=root->parent;
        if ((mom->type==MF_NODE_TYPE_OP)&&(mom->childc>=2)&&(mom->childv[1]==root)) {
          // I'm the rvalue of some op...
          if ((mom->token.c==1)&&(mom->token.v[0]=='.')) return 0;
          if ((mom->token.c==2)&&!memcmp(mom->token.v,"?.",2)) return 0;
        }
        if ((mom->type==MF_NODE_TYPE_OP)&&(mom->childc>=2)&&(mom->childv[0]==root)) {
          // I'm the lvalue. If the op is ":", we're an inline object field.
          if ((mom->token.c==1)&&(mom->token.v[0]==':')) return 0;
        }
        if (mom->type==MF_NODE_TYPE_FIELD) {
          if (mom->parent&&mom->parent->parent&&(mom->parent->parent->type==MF_NODE_TYPE_FOR1)) {
            // We're a destructuring "for" lcv. Don't change.
            return 0;
          }
          if ((mom->childc>=2)&&(mom->childv[1]==root)) {
            // rvalue of field. Replace as usual.
          } else if ((mom->childc>=1)&&(mom->childv[0]==root)) {
            if (mom->childc==1) {
              // I'm a self-referencing inline object member. Need to expand to "fromname:toname"
              mf_node_remove_all_children(root);
              root->type=MF_NODE_TYPE_OP;
              root->token.v=":";
              root->token.c=1;
              root->token.type=MF_TOKEN_TYPE_OPERATOR;
              struct mf_node *lvalue=mf_node_spawn(root);
              struct mf_node *rvalue=mf_node_spawn(root);
              if (!lvalue||!rvalue) return -1;
              lvalue->token=rvalue->token=root->token;
              lvalue->type=MF_NODE_TYPE_VALUE;
              lvalue->token.v=from;
              lvalue->token.c=fromc;
              lvalue->token.type=MF_TOKEN_TYPE_IDENTIFIER;
              rvalue->type=MF_NODE_TYPE_VALUE;
              rvalue->token.v=to;
              rvalue->token.c=toc;
              rvalue->token.type=MF_TOKEN_TYPE_IDENTIFIER;
              return 0;
            } else {
              // I'm the lvalue of an inline object. Don't change.
              return 0;
            }
          }
        }
      }
  
      root->token.v=to;
      root->token.c=toc;
    }
  }
  int i=0,err; for (;i<root->childc;i++) {
    if ((err=mf_rename_all_within(ctx,root->childv[i],to,toc,from,fromc))<0) return err;
  }
  return 0;
}
 
static int mf_rename_variable(struct eggdev_minify_js *ctx,struct mf_node *node) {
  if (!ctx||!node) return -1;
  
  if (node->token.type!=MF_TOKEN_TYPE_IDENTIFIER) return 0;

  int nnamec=0;
  char *nname=mf_next_identifier(ctx,&nnamec);
  if (!nname) return -1;
  const char *prev=node->token.v;
  int prevc=node->token.c;
  node->token.v=nname;
  node->token.c=nnamec;
  
  // Step up to the containing scope.
  if (node->parent) node=node->parent; // We might be FUNCTION. Don't stop here!
  while (node->parent) {
    if (node->type==MF_NODE_TYPE_FUNCTION) break;
    if (node->type==MF_NODE_TYPE_LAMBDA) break;
    if (node->type==MF_NODE_TYPE_METHOD) break;
    if (node->type==MF_NODE_TYPE_BLOCK) break; // I'm not sure that {} creates a namespace.
    node=node->parent;
  }
  return mf_rename_all_within(ctx,node,nname,nnamec,prev,prevc);
}
 
static int mf_rename_local_symbols(struct eggdev_minify_js *ctx,struct mf_node *node) {
  int err,i;
  switch (node->type) {
  
    case MF_NODE_TYPE_DECL: {
        for (i=0;i<node->childc;i++) {
          struct mf_node *child=node->childv[i];
          if (child->type==MF_NODE_TYPE_VALUE) {
            if ((err=mf_rename_variable(ctx,child))<0) return err;
          } else if ((child->type==MF_NODE_TYPE_OP)&&(child->token.c==1)&&(child->token.v[0]=='=')&&(child->childc>=1)) {
            struct mf_node *lvalue=child->childv[0];
            if (lvalue->type==MF_NODE_TYPE_VALUE) {
              if ((err=mf_rename_variable(ctx,lvalue))<0) return err;
            } else if (lvalue->type==MF_NODE_TYPE_ARRAY) {
              int ii=0; for (;ii<lvalue->childc;ii++) {
                struct mf_node *k=lvalue->childv[ii];
                if (k->type==MF_NODE_TYPE_VALUE) {
                  if ((err=mf_rename_variable(ctx,k))<0) return err;
                }
              }
            }
          } else {
            mf_js_log(ctx,&child->token,"Unexpected node %d under DECL '%.*s' childc=%d",child->type,child->token.c,child->token.v,child->childc);
          }
        }
      } break;
      
    case MF_NODE_TYPE_CLASS: {
        if ((err=mf_rename_variable(ctx,node))<0) return err;
      } break;
      
    case MF_NODE_TYPE_FUNCTION: {
        if ((node->token.c==8)&&!memcmp(node->token.v,"function",8)) {
          // anonymous, skip it
        } else {
          if ((err=mf_rename_variable(ctx,node))<0) return err;
        }
      } break;
    
    case MF_NODE_TYPE_PARAMLIST: {
        for (i=0;i<node->childc;i++) {
          struct mf_node *child=node->childv[i];
          if (child->type==MF_NODE_TYPE_PARAM) {
            if ((err=mf_rename_variable(ctx,child))<0) return err;
          }
        }
      } break;
  }
  for (i=0;i<node->childc;i++) {
    if ((err=mf_rename_local_symbols(ctx,node->childv[i]))<0) return err;
  }
  return 0;
}

/* Digest, main entry point.
 */
 
int mf_js_digest(struct eggdev_minify_js *ctx) {
  if (!ctx||!ctx->root) return -1;
  int err;
  //TODO Rephrase "let" as "const" if the symbol is never reassigned.
  if ((err=mf_resolve_expressions(ctx,ctx->root))<0) return err;
  //TODO Eliminate unreachable code.
  //TODO Drop unnecessary return at end of function.
  if ((err=mf_rename_local_symbols(ctx,ctx->root))<0) return err;
  if ((err=mf_reduce_member_names(ctx))<0) return err;
  //TODO Hoist and combine declarations.
  if ((err=mf_reduce_constants(ctx,ctx->root))<0) return err;
  //TODO Validate.
  return 0;
}
