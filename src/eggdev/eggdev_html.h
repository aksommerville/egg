/* eggdev_html.h
 * Parses HTML text enough to extract the things interesting to Egg.
 * Not going to attempt a general compliant HTML parser, that would be ridiculous.
 */
 
#ifndef EGGDEV_HTML_H
#define EGGDEV_HTML_H

#define EGGDEV_HTML_DEPTH_LIMIT 32

struct eggdev_html_reader {
  const char *refname;
  const char *src;
  int srcc;
  int srcp;
  // Awaiting close tags:
  struct eggdev_html_tag {
    const char *name;
    int namec;
  } tagv[EGGDEV_HTML_DEPTH_LIMIT];
  int tagc;
};

/* Never fails, and no need for cleanup.
 * Caller must keep (src) live and unchanged while (reader) in use.
 * If (refname) not null, we will log errors.
 */
void eggdev_html_reader_init(struct eggdev_html_reader *reader,const void *src,int srcc,const char *refname);

/* Return the next expression and advance beyond it.
 * When we report OPEN, we have also pushed it on the context's stack.
 * You will only see CLOSE when they match the expected OPEN.
 * COMMENT includes DOCTYPE.
 * SPACE is lexically the same as TEXT, but only contains whitespace. Typically you'll ignore those.
 * eggdev_html_expression_type always returns one of the symbols below; empty string is SPACE.
 * Whitespace is not trimmed from TEXT.
 */
int eggdev_html_reader_next(void *dstpp,struct eggdev_html_reader *reader);
int eggdev_html_expression_type(const char *src,int srcc);
#define EGGDEV_HTML_EXPR_SPACE 1
#define EGGDEV_HTML_EXPR_TEXT 2
#define EGGDEV_HTML_EXPR_COMMENT 3
#define EGGDEV_HTML_EXPR_SINGLETON 4
#define EGGDEV_HTML_EXPR_OPEN 5
#define EGGDEV_HTML_EXPR_CLOSE 6

/* After receiving an OPEN, you can call this to consume all of the tag's inner text
 * and advance beyond its CLOSE tag.
 * The text chunk we return may contain nested tags and comments.
 * We fail if the inner HTML is malformed.
 */
int eggdev_html_close(void *dstpp,struct eggdev_html_reader *reader);

/* For SINGLETON, OPEN, and CLOSE.
 */
int eggdev_html_get_tag_name(void *dstpp,const char *src,int srcc);

/* Call (cb) for each attribute in this OPEN or SINGLETON tag.
 * Return the first nonzero result, or zero at the end.
 * We do not resolve escapes, but we do strip quotes from (v).
 */
int eggdev_html_for_each_attribute(
  const char *src,int srcc,
  int (*cb)(const char *k,int kc,const char *v,int vc,void *userdata),
  void *userdata
);

int eggdev_html_get_attribute(void *dstpp,const char *src,int srcc,const char *k,int kc);

/* Collapse whitespace and resolve ampersand escapes.
 */
int eggdev_html_text_eval(char *dst,int dsta,const char *src,int srcc);

#endif
