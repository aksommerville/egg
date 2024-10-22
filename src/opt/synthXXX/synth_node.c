#include "synth_internal.h"

/* Delete node.
 */
 
void synth_node_del(struct synth_node *node) {
  if (!node) return;
  if (node->type->del) node->type->del(node);
  free(node);
}

/* New node.
 */

struct synth_node *synth_node_new(struct synth *synth,const struct synth_node_type *type,int chanc) {
  if (!synth||!type) return 0;
  if ((chanc<0)||(chanc>2)) return 0;
  struct synth_node *node=calloc(1,type->objlen);
  if (!node) return 0;
  node->synth=synth;
  node->type=type;
  if (chanc) node->chanc=chanc;
  else if (synth->chanc>2) node->chanc=2;
  else node->chanc=synth->chanc;
  if (type->init) {
    if (type->init(node)<0) {
      synth_node_del(node);
      return 0;
    }
  }
  return node;
}

/* Ready.
 */

int synth_node_ready(struct synth_node *node) {
  if (!node) return -1;
  if (node->ready) return 0;
  if (node->type->ready) {
    if (node->type->ready(node)<0) return -1;
  }
  if (!node->update) return -1;
  node->ready=1;
  return 0;
}
