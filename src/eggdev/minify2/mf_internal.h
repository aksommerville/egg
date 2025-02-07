#ifndef MF_INTERNAL_H
#define MF_INTERNAL_H

#include "eggdev/eggdev_internal.h"
#include "mf_node.h"

/* Generic entry point we can re-enter.
 */
int eggdev_minify_inner(struct sr_encoder *dst,const char *src,int srcc,const char *srcpath,int fmt);

/* CSS.
 ****************************************************************/
 
struct eggdev_minify_css {
  struct sr_encoder *dst; // WEAK
  const char *srcpath;
  const char *src;
  int srcc;
};

void eggdev_minify_css_cleanup(struct eggdev_minify_css *ctx);
int eggdev_minify_css(struct eggdev_minify_css *ctx);

/* HTML.
 ****************************************************************/
 
struct eggdev_minify_html {
  struct sr_encoder *dst; // WEAK
  const char *srcpath;
  const char *src;
  int srcc;
};

void eggdev_minify_html_cleanup(struct eggdev_minify_html *ctx);
int eggdev_minify_html(struct eggdev_minify_html *ctx);

/* Javascript.
 * An incomplete list of things we definitely don't support:
 *  - async/await
 *  - Generator functions.
 *  - Proper Unicode text (all >U+7f are identifier-legal here).
 *  - Semicolon insertion. We require them.
 *  - Named loops.
 *  - Imports with renaming, or default imports. All top-level namespaces are merged with their original names.
 ****************************************************************/
 
struct eggdev_minify_js {
  struct sr_encoder *dst; // WEAK
  const char *srcpath;
  
  /* We retain all of the original text during processing.
   * The AST points into these text dumps.
   */
  struct mf_file {
    char *path;
    char *src;
    int srcc;
    int fileid;
  } *filev;
  int filec,filea;
  
  // Additional scratch text. Once interned, an address is stable until the context deletes.
  struct mf_textcache {
    char *v;
    int c,a;
  } *textcachev;
  int textcachec,textcachea;
  
  struct mf_node *root;
  
  int nextident; // Used during identifier size reduction.
};

void eggdev_minify_js_cleanup(struct eggdev_minify_js *ctx);
int eggdev_minify_js(struct eggdev_minify_js *ctx,const char *src,int srcc);

/* Log an error in the context of (token) and return -2.
 * '_ifctx' returns -1 without logging, if (ctx) null.
 */
int mf_jserr(struct eggdev_minify_js *ctx,struct mf_token *token,const char *fmt,...);
int mf_jserr_ifctx(struct eggdev_minify_js *ctx,struct mf_token *token,const char *fmt,...);

/* For troubleshooting, log a message with file and line number.
 */
void mf_js_log(struct eggdev_minify_js *ctx,struct mf_token *token,const char *fmt,...);

/* Reads file and adds to list.
 * You should check first whether we already have it.
 * Does not parse any text.
 * If (src) is null we read the file, otherwise we copy what you provide.
 */
struct mf_file *mf_js_add_file(struct eggdev_minify_js *ctx,const char *path,const void *src,int srcc);

struct mf_file *mf_js_get_file_by_path(struct eggdev_minify_js *ctx,const char *path);
struct mf_file *mf_js_get_file_by_id(struct eggdev_minify_js *ctx,int fileid);

/* Returns a copy of (src) guaranteed to be free for the context's life, and you don't need to free it.
 * (srcc) is required.
 * The returned string is NOT terminated.
 */
char *mf_js_text_intern(struct eggdev_minify_js *ctx,const char *src,int srcc);

/* Advance the internal identifier counter and intern a new one.
 * NB the string is not terminated; you must receive (len).
 */
char *mf_next_identifier(struct eggdev_minify_js *ctx,int *len);

/* Read (file)'s text and append statements to (parent).
 * This is only appropriate at the top level of a file.
 * Gathering statements, and all the "compile" functions it involves, do only the minimum of validation.
 */
int mf_js_gather_statements(struct mf_node *parent,struct eggdev_minify_js *ctx,struct mf_file *file);

/* Append a node to (parent) for the next statement, starting with the next token off (reader).
 * EOF is an error.
 * Guaranteed to advance (reader) and append one node on success.
 */
int mf_js_compile_statement(struct mf_node *parent,struct eggdev_minify_js *ctx,struct mf_token_reader *reader);

/* Just mf_js_compile_statement, but first assert it begins with open-bracket.
 */
int mf_js_compile_bracketted_statement(struct mf_node *parent,struct eggdev_minify_js *ctx,struct mf_token_reader *reader);

/* Append one node to (parent), for the longest possible expression starting from (reader)'s next token.
 */
int mf_js_compile_expression_with_limit(struct mf_node *parent,struct eggdev_minify_js *ctx,struct mf_token_reader *reader,int opcls);
static inline int mf_js_compile_expression(struct mf_node *parent,struct eggdev_minify_js *ctx,struct mf_token_reader *reader) {
  return mf_js_compile_expression_with_limit(parent,ctx,reader,MF_OPCLS_OUTER);
}

/* Primary expression only, overwriting (node).
 * It is rare for consumers outside an expression to need this.
 * ("for" loops do).
 */
int mf_js_compile_primary_expression(struct mf_node *node,struct eggdev_minify_js *ctx,struct mf_token_reader *reader);

/* Exactly like mf_js_compile_expression() but assert that it begin with an open paren, and stop at the corresponding close paren.
 * eg for "if", "while", "switch"...
 */
int mf_js_compile_parenthesized_expression(struct mf_node *parent,struct eggdev_minify_js *ctx,struct mf_token_reader *reader);

/* Rewrite (node) with an expression limited by (opcls).
 * (node) should be freshly spawned and other uninitialized.
 * Next token off (reader) should be a primary expression.
 * If you're calling from statement context, use mf_js_compile_expression() instead.
 * This one is the recursive entry point.
 */
int mf_js_compile_limited_expression(struct mf_node *node,struct eggdev_minify_js *ctx,struct mf_token_reader *reader,int opcls);

/* Next token must be '('. We append a single PARAMLIST node to (parent), and consume thru the matching ')'.
 */
int mf_js_compile_paramlist(struct mf_node *parent,struct eggdev_minify_js *ctx,struct mf_token_reader *reader);

/* Validation, transformation, and optimization against the AST.
 * (ctx->root) must exist, and we modify it in place.
 */
int mf_js_digest(struct eggdev_minify_js *ctx);

/* Emit text.
 * This aims to be as dumb as possible.
 * Any adjustments we want to make to the text, prefer to make them semantically by changing nodes around.
 */
int mf_js_output(struct sr_encoder *dst,struct eggdev_minify_js *ctx,struct mf_node *node);

/* If (node) is constant, generate its value as Javascript text.
 * This will never be perfect because we're not a real JS runtime.
 * But it does get fancy, resolving identifiers and performing arithmetic and all that.
 */
int mf_node_eval(char *dst,int dsta,struct eggdev_minify_js *ctx,struct mf_node *node);

#endif
