#include "mf_internal.h"

/* Object lifecycle.
 */
 
void mf_node_del(struct mf_node *node) {
  if (!node) return;
  if (node->refc-->1) return;
  if (node->parent) fprintf(stderr,"Deleting node %p, parent=%p, should be null by this point.\n",node,node->parent);
  if (node->childv) {
    while (node->childc-->0) {
      struct mf_node *child=node->childv[node->childc];
      child->parent=0;
      mf_node_del(child);
    }
    free(node->childv);
  }
  free(node);
}

int mf_node_ref(struct mf_node *node) {
  if (!node) return -1;
  if (node->refc<1) return -1;
  if (node->refc>=INT_MAX) return -1;
  node->refc++;
  return 0;
}

struct mf_node *mf_node_new() {
  struct mf_node *node=calloc(1,sizeof(struct mf_node));
  if (!node) return 0;
  node->refc=1;
  return node;
}

struct mf_node *mf_node_spawn(struct mf_node *parent) {
  struct mf_node *child=mf_node_new();
  if (!child) return 0;
  if (mf_node_add_child(parent,child,-1)<0) {
    mf_node_del(child);
    return 0;
  }
  mf_node_del(child);
  return child;
}

struct mf_node *mf_node_spawn_at(struct mf_node *parent,int p) {
  struct mf_node *child=mf_node_new();
  if (!child) return 0;
  if (mf_node_add_child(parent,child,p)<0) {
    mf_node_del(child);
    return 0;
  }
  mf_node_del(child);
  return child;
}

struct mf_node *mf_node_spawn_token(struct mf_node *parent,const char *src,int srcc) {
  if (!src||(srcc<0)) return 0;
  struct mf_node *child=mf_node_spawn(parent);
  if (!child) return 0;
  child->token=parent->token;
  child->type=MF_NODE_TYPE_VALUE;
  child->token.v=src;
  child->token.c=srcc;
  return child;
}

/* Allocation.
 */
 
static int mf_node_childv_require(struct mf_node *node,int addc) {
  if (addc<1) return 0;
  if (node->childc>INT_MAX-addc) return -1;
  int na=node->childc+addc;
  if (na<=node->childa) return 0;
  if (na<INT_MAX-4) na=(na+4)&~3;
  if (na>INT_MAX/sizeof(void*)) return -1;
  void *nv=realloc(node->childv,sizeof(void*)*na);
  if (!nv) return -1;
  node->childv=nv;
  node->childa=na;
  return 0;
}

/* Test ancestry.
 */

int mf_node_is_descendant(const struct mf_node *ancestor,const struct mf_node *descendant) {
  if (!ancestor) return 0;
  for (;descendant;descendant=descendant->parent) {
    if (ancestor==descendant) return 1;
  }
  return 0;
}

int mf_node_find_child(const struct mf_node *parent,const struct mf_node *child) {
  if (!parent||!child) return -1;
  int i=parent->childc;
  while (i-->0) if (parent->childv[i]==child) return i;
  return -1;
}

/* Add child.
 */
 
int mf_node_add_child(struct mf_node *parent,struct mf_node *child,int p) {
  if (!parent||!child) return -1;
  if ((p<0)||(p>parent->childc)) p=parent->childc;
  
  // Same parent is noop for two (p). Otherwise it's just a shuffle.
  if (child->parent==parent) {
    int fromp=mf_node_find_child(parent,child);
    if (fromp<0) return -1;
    if (fromp<p) {
      memmove(parent->childv+fromp,parent->childv+fromp+1,sizeof(void*)*(p-fromp-1));
    } else if (fromp>p+1) {
      memmove(parent->childv+p+1,parent->childv+p,sizeof(void*)*(fromp-p-1));
    } else { // To same or next position, noop.
      return 0;
    }
    parent->childv[p]=child;
    return 0;
  }
  
  // Otherwise (child) must be an orphan.
  if (child->parent) return -1;
  if (mf_node_is_descendant(child,parent)) return -1;
  
  if (mf_node_childv_require(parent,1)<0) return -1;
  if (mf_node_ref(child)<0) return -1;
  memmove(parent->childv+p+1,parent->childv+p,sizeof(void*)*(parent->childc-p));
  parent->childc++;
  parent->childv[p]=child;
  child->parent=parent;
  return 0;
}

/* Remove child.
 */
 
int mf_node_remove_child(struct mf_node *parent,struct mf_node *child) {
  if (!parent||!child) return -1;
  int i=parent->childc;
  while (i-->0) {
    if (parent->childv[i]==child) return mf_node_remove_child_at(parent,i);
  }
  return -1;
}

int mf_node_remove_child_at(struct mf_node *parent,int p) {
  if (!parent) return -1;
  if ((p<0)||(p>=parent->childc)) return -1;
  struct mf_node *child=parent->childv[p];
  parent->childc--;
  memmove(parent->childv+p,parent->childv+p+1,sizeof(void*)*(parent->childc-p));
  child->parent=0;
  mf_node_del(child);
  return 0;
}

void mf_node_remove_all_children(struct mf_node *parent) {
  if (!parent) return;
  while (parent->childc>0) {
    struct mf_node *child=parent->childv[--(parent->childc)];
    child->parent=0;
    mf_node_del(child);
  }
}

/* Kidnap.
 */
 
int mf_node_kidnap(struct mf_node *toparent,struct mf_node *child,int p) {
  if (!toparent||!child) return -1;
  
  // From none or same is legal, but let 'add_child' do it.
  struct mf_node *fromparent=child->parent;
  if (!fromparent||(toparent==fromparent)) return mf_node_add_child(toparent,child,p);
  
  int fromp=mf_node_find_child(fromparent,child);
  if (fromp<0) return -1;
  if (mf_node_childv_require(toparent,1)<0) return -1;
  if ((p<0)||(p>toparent->childc)) p=toparent->childc;
  memmove(toparent->childv+p+1,toparent->childv+p,sizeof(void*)*(toparent->childc-p));
  toparent->childc++;
  toparent->childv[p]=child;
  fromparent->childc--;
  memmove(fromparent->childv+fromp,fromparent->childv+fromp+1,sizeof(void*)*(fromparent->childc-fromp));
  child->parent=toparent;
  return 0;
}

int mf_node_kidnap_all(struct mf_node *toparent,struct mf_node *fromparent,int p) {
  if (!toparent||!fromparent) return -1;
  if (toparent==fromparent) return 0;
  if (fromparent->childc<1) return 0;
  if (mf_node_childv_require(toparent,fromparent->childc)<0) return -1;
  if ((p<0)||(p>=toparent->childc)) p=toparent->childc;
  memmove(toparent->childv+p+fromparent->childc,toparent->childv+p,sizeof(void*)*(toparent->childc-p));
  toparent->childc+=fromparent->childc;
  memcpy(toparent->childv+p,fromparent->childv,sizeof(void*)*fromparent->childc);
  int i=fromparent->childc;
  while (i--) {
    struct mf_node *child=fromparent->childv[i];
    child->parent=toparent;
  }
  fromparent->childc=0;
  return 0;
}

/* Bodysnatch.
 */
 
int mf_node_transfer(struct mf_node *dst,struct mf_node *src) {
  if (!dst||!src) return -1;
  dst->type=src->type;
  memcpy(dst->argv,src->argv,sizeof(dst->argv));
  dst->token=src->token;
  return mf_node_kidnap_all(dst,src,-1);
}

/* Look up symbol.
 */
 
static struct mf_node *mf_node_lookup_symbol_in_decl(struct mf_node *ctx,const char *sym,int symc) {
  int i=0; for (;i<ctx->childc;i++) {
    struct mf_node *child=ctx->childv[i];
    
    if ((child->type==MF_NODE_TYPE_VALUE)&&(child->token.c==symc)&&!memcmp(child->token.v,sym,symc)) {
      // Found it, no initializer.
      return child;
      
    } else if ((child->type==MF_NODE_TYPE_OP)&&(child->token.c==1)&&(child->token.v[0]=='=')&&(child->childc==2)) {
      struct mf_node *lvalue=child->childv[0];
      
      if ((lvalue->type==MF_NODE_TYPE_VALUE)&&(lvalue->token.c==symc)&&!memcmp(lvalue->token.v,sym,symc)) {
        // Found it, with initializer.
        return lvalue;
        
      } else if ((lvalue->type==MF_NODE_TYPE_ARRAY)||(lvalue->type==MF_NODE_TYPE_OBJECT)) {
        int ai=0; for (;ai<lvalue->childc;ai++) {
          struct mf_node *a=lvalue->childv[i];
          if ((a->type==MF_NODE_TYPE_VALUE)&&(a->token.c==symc)&&!memcmp(a->token.v,sym,symc)) {
            // Found it, in destructured array or object.
            return a;
          }
        }
      }
    }
  }
  return 0;
}
 
static struct mf_node *mf_node_lookup_symbol_at_context(struct mf_node *ctx,const char *sym,int symc) {
  int i=0; for (;i<ctx->childc;i++) {
    struct mf_node *child=ctx->childv[i];
    if (child->type==MF_NODE_TYPE_DECL) {
      struct mf_node *found=mf_node_lookup_symbol_in_decl(child,sym,symc);
      if (found) return found;
    }
  }
  return 0;
}
 
struct mf_node *mf_node_lookup_symbol(struct mf_node *ctx,const char *sym,int symc) {
  if (!sym) return 0;
  if (symc<0) { symc=0; while (sym[symc]) symc++; }
  if (!symc) return 0;
  for (;ctx;ctx=ctx->parent) {
    struct mf_node *found=mf_node_lookup_symbol_at_context(ctx,sym,symc);
    if (found) return found;
  }
  return 0;
}

/* Locate initializer for symbol node.
 */
 
struct mf_node *mf_node_get_symbol_initializer(struct mf_node *sym) {
  if (!sym) return 0;
  struct mf_node *parent=sym->parent;
  if (!parent) return 0;
  
  if ((parent->type==MF_NODE_TYPE_OP)&&(parent->token.c==1)&&(parent->token.v[0]=='=')&&(parent->childc>=2)) {
    // Simple initialized declaration.
    return parent->childv[1];
  }
  
  if (parent->type==MF_NODE_TYPE_DECL) {
    // No initializer.
    return 0;
  }
  
  if ((parent->type==MF_NODE_TYPE_ARRAY)||(parent->type==MF_NODE_TYPE_OBJECT)) {
    // Destructured assignment. Grandparent must be OP(=).
    struct mf_node *grampa=parent->parent;
    if (grampa&&(grampa->type==MF_NODE_TYPE_OP)&&(grampa->token.c==1)&&(grampa->token.v[0]=='=')&&(grampa->childc>=2)) {
      struct mf_node *rvalue=grampa->childv[1];
      if ((parent->type==MF_NODE_TYPE_ARRAY)&&(rvalue->type==MF_NODE_TYPE_ARRAY)) {
        int p=mf_node_find_child(parent,sym);
        if ((p>=0)&&(p<rvalue->childc)) return rvalue->childv[p];
        return 0;
      }
      //TODO There's a special case of destructuring an array from String.split(",") that we ought to accomodate.
      // We produce that construction. But probably only after constant expressions are resolved, so I don't think it will actually matter.
      if ((parent->type==MF_NODE_TYPE_OBJECT)&&(rvalue->type==MF_NODE_TYPE_OBJECT)) {
        // Destructuring an inline object is not realistic, why would you say "const {a}={a:123};" instead of "const a=123;"?
        return 0;
      }
      //TODO More complex destructuring. eg "const a={b:123}; const {b}=a;"
      // Is this ever going to come up? It sounds unusual.
      return 0;
    }
  }
  
  return 0;
}

/* Nearest ancestor of a given type.
 */
 
struct mf_node *mf_node_ancestor_of_type(struct mf_node *descendant,int type) {
  for (;descendant;descendant=descendant->parent) {
    if (descendant->type==type) return descendant;
  }
  return 0;
}

/* Helper nodelist.
 */
 
void mf_nodelist_del(struct mf_nodelist *nl) {
  if (!nl) return;
  if (nl->v) free(nl->v);
  free(nl);
}

struct mf_nodelist *mf_nodelist_new() {
  struct mf_nodelist *nl=calloc(1,sizeof(struct mf_nodelist));
  if (!nl) return 0;
  return nl;
}

int mf_nodelist_search(const struct mf_nodelist *nl,const struct mf_node *node) {
  int lo=0,hi=nl->c;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
    const struct mf_node *q=nl->v[ck];
         if (node<q) hi=ck;
    else if (node>q) lo=ck+1;
    else return ck;
  }
  return -lo-1;
}

int mf_nodelist_insert(struct mf_nodelist *nl,int p,struct mf_node *node) {
  if ((p<0)||(p>nl->c)) return -1;
  if (p&&(node<=nl->v[p-1])) return -1;
  if ((p<nl->c)&&(node>=nl->v[p])) return -1;
  if (nl->c>=nl->a) {
    int na=nl->a+16;
    if (na>INT_MAX/sizeof(void*)) return -1;
    void *nv=realloc(nl->v,sizeof(void*)*na);
    if (!nv) return -1;
    nl->v=nv;
    nl->a=na;
  }
  memmove(nl->v+p+1,nl->v+p,sizeof(void*)*(nl->c-p));
  nl->c++;
  nl->v[p]=node;
  return 0;
}

int mf_nodelist_add(struct mf_nodelist *nl,struct mf_node *node) {
  if (!nl||!node) return -1;
  int p=mf_nodelist_search(nl,node);
  if (p>=0) return 0;
  p=-p-1;
  return mf_nodelist_insert(nl,p,node);
}

void mf_nodelist_remove(struct mf_nodelist *nl,struct mf_node *node) {
  int p=mf_nodelist_search(nl,node);
  if (p<0) return;
  nl->c--;
  memmove(nl->v+p,nl->v+p+1,sizeof(void*)*(nl->c-p));
}

static int mf_nodelist_apply_filter(struct mf_nodelist *nl,struct mf_node *node,int (*filter)(struct mf_node *node,void *userdata),void *userdata) {
  int err,i=0;
  if ((err=filter(node,userdata))>0) {
    if (mf_nodelist_add(nl,node)<0) return -1;
  }
  if (err<0) return err;
  for (;i<node->childc;i++) {
    if ((err=mf_nodelist_apply_filter(nl,node->childv[i],filter,userdata))<0) return err;
  }
  return 0;
}

struct mf_nodelist *mf_find_nodes(struct mf_node *root,int (*filter)(struct mf_node *node,void *userdata),void *userdata) {
  struct mf_nodelist *nl=mf_nodelist_new();
  if (!nl) return 0;
  if (mf_nodelist_apply_filter(nl,root,filter,userdata)<0) {
    mf_nodelist_del(nl);
    return 0;
  }
  return nl;
}
