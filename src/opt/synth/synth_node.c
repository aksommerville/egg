#include "synth_internal.h"

/* Delete.
 */

void synth_node_del(struct synth_node *node) {
  if (!node) return;
  if (node->refc-->1) return;
  if (node->type->del) node->type->del(node);
  if (node->childv) {
    while (node->childc-->0) {
      struct synth_node *child=node->childv[node->childc];
      child->parent=0;
      synth_node_del(child);
    }
    free(node->childv);
  }
  free(node);
}

/* Retain.
 */
 
int synth_node_ref(struct synth_node *node) {
  if (!node) return -1;
  if (node->refc<1) return -1;
  if (node->refc==INT_MAX) return -1;
  node->refc++;
  return 0;
}

/* New.
 */
 
static void synth_node_update_noop(float *v,int c,struct synth_node *node) {}

struct synth_node *synth_node_new(const struct synth_node_type *type,int chanc,struct synth *synth) {
  if (!type||!synth) return 0;
  struct synth_node *node=calloc(1,type->objlen);
  if (!node) return 0;
  node->type=type;
  node->synth=synth;
  node->chanc=chanc?chanc:synth->chanc;
  node->refc=1;
  node->update=synth_node_update_noop;
  if (type->init&&(type->init(node)<0)) {
    synth_node_del(node);
    return 0;
  }
  return node;
}

/* Spawn.
 */

struct synth_node *synth_node_spawn(struct synth_node *parent,const struct synth_node_type *type,int chanc) {
  if (!parent||!type) return 0;
  struct synth_node *child=synth_node_new(type,chanc?chanc:parent->chanc,parent->synth);
  if (!child) return 0;
  if (synth_node_add_child(parent,child)<0) {
    synth_node_del(child);
    return 0;
  }
  synth_node_del(child);
  return child;
}

/* Test ancestry.
 * True if they are the same node.
 */
 
static int synth_node_is_ancestor(const struct synth_node *ancestor,const struct synth_node *descendant) {
  if (!ancestor) return 0;
  for (;descendant;descendant=descendant->parent) if (ancestor==descendant) return 1;
  return 0;
}

/* Add child.
 */

int synth_node_add_child(struct synth_node *parent,struct synth_node *child) {
  if (!parent||!child) return -1;
  if (child->parent==parent) return 0;
  if (child->parent) return -1;
  if (synth_node_is_ancestor(child,parent)) return -1;
  if (parent->childc>=parent->childa) {
    int na=parent->childa+8;
    if (na>INT_MAX/sizeof(void*)) return -1;
    void *nv=realloc(parent->childv,sizeof(void*)*na);
    if (!nv) return -1;
    parent->childv=nv;
    parent->childa=na;
  }
  if (synth_node_ref(child)<0) return -1;
  child->parent=parent;
  parent->childv[parent->childc++]=child;
  return 0;
}

/* Remove child.
 */
 
int synth_node_remove_child(struct synth_node *parent,struct synth_node *child) {
  if (!parent||!child) return -1;
  int i=parent->childc;
  while (i-->0) {
    if (parent->childv[i]==child) {
      child->parent=0;
      parent->childc--;
      memmove(parent->childv+i,parent->childv+i+1,sizeof(void*)*(parent->childc-i));
      synth_node_del(child);
      return 0;
    }
  }
  return -1;
}

/* Ready.
 */
 
int synth_node_ready(struct synth_node *node) {
  if (!node) return -1;
  if (node->ready) return 1;
  if (!node->type->ready) {
    node->ready=1;
    return 0;
  }
  if (node->type->ready(node)<0) return -1;
  node->ready=1;
  return 0;
}
