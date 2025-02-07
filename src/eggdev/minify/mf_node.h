/* mf_node.h
 * One statement or expression in a Javascript AST.
 */
 
#ifndef MF_NODE_H
#define MF_NODE_H

#include "mf_token.h"

#define MF_NODE_ARGV_SIZE 4

struct mf_node {
  struct mf_node *parent; // WEAK
  struct mf_node **childv;
  int childc,childa;
  int refc;
  int type; // MF_NODE_TYPE_*, see below.
  int argv[MF_NODE_ARGV_SIZE];
  struct mf_token token;
};

void mf_node_del(struct mf_node *node);
int mf_node_ref(struct mf_node *node);
struct mf_node *mf_node_new();

struct mf_node *mf_node_spawn(struct mf_node *parent); // => WEAK
struct mf_node *mf_node_spawn_at(struct mf_node *parent,int p);

/* Convenience to spawn a VALUE node with static text (typically "0", "undefined", etc).
 */
struct mf_node *mf_node_spawn_token(struct mf_node *parent,const char *src,int srcc);

/* Add and kidnap take an insert position (p) which can be <0 to append.
 * Add or kidnap to existing parent to only change position.
 */
int mf_node_is_descendant(const struct mf_node *ancestor,const struct mf_node *descendant);
int mf_node_find_child(const struct mf_node *parent,const struct mf_node *child);
int mf_node_add_child(struct mf_node *parent,struct mf_node *child,int p);
int mf_node_remove_child(struct mf_node *parent,struct mf_node *child);
int mf_node_remove_child_at(struct mf_node *parent,int p);
int mf_node_kidnap(struct mf_node *toparent,struct mf_node *child,int p);
int mf_node_kidnap_all(struct mf_node *toparent,struct mf_node *fromparent,int p);
void mf_node_remove_all_children(struct mf_node *parent);

/* Copy (type,argv,token) from (src) to (dst), and kidnap the children.
 */
int mf_node_transfer(struct mf_node *dst,struct mf_node *src);

/* Given an identifier (sym) and context node (ctx), walk up the tree until we find its declaration.
 * If found, we return a WEAK node of type VALUE whose token is (sym).
 */
struct mf_node *mf_node_lookup_symbol(struct mf_node *ctx,const char *sym,int symc);

/* For a node returned by mf_node_lookup_symbol(), locate its initializer.
 * It doesn't necessarily exist; you can declare symbols without initializing them.
 */
struct mf_node *mf_node_get_symbol_initializer(struct mf_node *sym);

/* Nearest ancestor of the given type, including (descendant) itself, or null if not found.
 */
struct mf_node *mf_node_ancestor_of_type(struct mf_node *descendant,int type);

#define MF_NODE_TYPE_ROOT            1 /* Like BLOCK but reserved for the root node. */
#define MF_NODE_TYPE_BLOCK           2 /* Sequential statements. */
#define MF_NODE_TYPE_VALUE           3 /* Expression formed of this node's token. Grave strings should be treated as regular strings. */
#define MF_NODE_TYPE_EXPWRAP         4 /* Single expression occupying the space of a statement. */
#define MF_NODE_TYPE_CLASS           5 /* Token is name. Children are functions. */
#define MF_NODE_TYPE_METHOD          6 /* Token is name or "constructor". argv[0]=static, [0]=paramlist, [1]=body */
#define MF_NODE_TYPE_PARAMLIST       7 /* [PARAM...] */
#define MF_NODE_TYPE_PARAM           8 /* Token is name. argv[0]=rest, [0]?=initializer */
#define MF_NODE_TYPE_DSPARAM         9 /* Token is "{" or "[". [0]=PARAMLIST, [1]?=initializer. TODO Are destructuring params allowed to have an initializer? We allow it. */
#define MF_NODE_TYPE_IF             10 /* [0]=condition, [1]=true, [2]?=false */
#define MF_NODE_TYPE_OP             11 /* Token=operator, argv[0]=opcls, []=operands. Binary operators may contain more than 2 operands. */
#define MF_NODE_TYPE_RETURN         12 /* [0]?=value */
#define MF_NODE_TYPE_LAMBDA         13 /* [0]=PARAMLIST, [1]=body */
#define MF_NODE_TYPE_CALL           14 /* [0]=function, [1...]=args */
#define MF_NODE_TYPE_INDEX          15 /* token="[" or "?.[", [0]=context, [1]=index */
#define MF_NODE_TYPE_DECL           16 /* token=const|var|let, [...]=VALUE|OP(=). */
#define MF_NODE_TYPE_THROW          17 /* [0]?=value */
#define MF_NODE_TYPE_ARRAY          18 /* [...]=members. */
#define MF_NODE_TYPE_OBJECT         19 /* [...]=members (FIELD). */
#define MF_NODE_TYPE_SWITCH         20 /* [0]=context, [1...]=statement|CASE */
#define MF_NODE_TYPE_CASE           21 /* [0]?=match. "default" if no children. Token is "case" or "default". Does not contain its statements. */
#define MF_NODE_TYPE_TRY            22 /* [0]=body, [1]?=identifier, [2]?=catch body, [3|1]?=finally, argv[0]=catch present, argv[1]=finally present */
#define MF_NODE_TYPE_FOR1           23 /* argv[0]=mode: (1,2)=(in,of). argv[1]=decl: (1,2,3)=(const,let,var). [0]=lcv, [1]=context, [2]=body. */
#define MF_NODE_TYPE_FOR3           24 /* [0]=init, [1]=test, [2]=post, [3]=body */
#define MF_NODE_TYPE_WHILE          25 /* [0]=condition [1]=body */
#define MF_NODE_TYPE_DO             26 /* [0]=body [1]=condition */
#define MF_NODE_TYPE_LOOPCTL        27 /* Token is "break" or "continue". We're not supporting named loops. */
#define MF_NODE_TYPE_POSTFIX        28 /* [0]=expression, token is "++" or "--". Prefix are OP, like normal unary operators. */
#define MF_NODE_TYPE_FUNCTION       29 /* Token is name or "function" if anonymous. [0]=paramlist, [1]=body */
#define MF_NODE_TYPE_FIELD          30 /* [0]=key [1]?=value */

struct mf_nodelist {
  struct mf_node **v; // WEAK
  int c,a;
};

void mf_nodelist_del(struct mf_nodelist *nl);
struct mf_nodelist *mf_nodelist_new();

int mf_nodelist_search(const struct mf_nodelist *nl,const struct mf_node *node);
int mf_nodelist_insert(struct mf_nodelist *nl,int p,struct mf_node *node); // must be sorted, we check
int mf_nodelist_add(struct mf_nodelist *nl,struct mf_node *node);
void mf_nodelist_remove(struct mf_nodelist *nl,struct mf_node *node);

struct mf_nodelist *mf_find_nodes(struct mf_node *root,int (*filter)(struct mf_node *node,void *userdata),void *userdata);

#endif
