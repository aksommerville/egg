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
