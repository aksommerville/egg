/* jst.h
 * Javascript Tiny.
 */
 
#ifndef JST_H
#define JST_H

struct jst_context {
  int logged_error;
  const char *src;
  int srcc;
  const char *refname;
  struct sr_encoder *dst; // WEAK, provided by caller.
  int startp; // Start of output in (dst).
  const char *pvtoken;
  int pvtokenc;
  char **importedv; // Resolved path to all files imported so far.
  int importedc,importeda;
  struct jst_ident {
    char *src,*dst;
    int srcc,dstc;
  } *identv;
  int identc,identa;
  int ignore; // While nonzero, drop everything. /*IGNORE{*/ .. /*}IGNORE*/
};

void jst_context_cleanup(struct jst_context *ctx);

/* The whole operation happens here.
 * You must cleanup after, whether pass or fail.
 */
int jst_minify(struct sr_encoder *dst,struct jst_context *ctx,const char *src,int srcc,const char *refname);

/* Skip whitespace and comments.
 * May log errors, eg for unclosed block comment.
 */
int jst_measure_space(struct jst_context *ctx,const char *src,int srcc);

/* Measure one non-whitespace token.
 */
int jst_measure_token(struct jst_context *ctx,const char *src,int srcc);

/* Log an error and return -2.
 * If you call it again on the same context, we do not log twice.
 * (src) should be a pointer into the context's current source. If so, we determine and print the line number.
 * Can also be null to not print a line number.
 */
int jst_error(struct jst_context *ctx,const char *src,const char *fmt,...);

static inline int jst_isident(char ch) {
  if ((ch>='a')&&(ch<='z')) return 1;
  if ((ch>='A')&&(ch<='Z')) return 1;
  if ((ch>='0')&&(ch<='9')) return 1;
  if (ch=='_') return 1;
  if (ch=='$') return 1;
  return 0;
}

int jst_is_keyword(const char *src,int srcc);

/* Lop the basename off (pfx) then glue (sfx) onto it.
 * (sfx) may begin with dot and double-dot entries, but must not contain any in the middle.
 */
int jst_resolve_path(char *dst,int dsta,const char *pfx,int pfxc,const char *sfx,int sfxc);

/* Make up an identifier that will be unique for each (seq).
 */
int jst_identifier_from_sequence(char *dst,int dsta,int seq);

#endif
