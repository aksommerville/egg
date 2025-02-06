/* mf_token.h
 * Javascript tokens.
 */
 
#ifndef MF_TOKEN_H
#define MF_TOKEN_H

struct eggdev_minify_js;

struct mf_token {
  const char *v; // Starts with file's text but may change.
  int c;
  int type; // MF_TOKEN_TYPE_*, see below.
  int fileid;
  int srcp; // In original file.
};

struct mf_token_reader {
// Initialize manually:
  const char *v;
  int c;
  int fileid;
// Initialize to zero:
  int p;
  struct mf_token prev;
};

/* Quietly advance past any whitespace and comments.
 * You don't need to do this normally, but might if you need to detect EOF before popping the next token.
 * The only possible error is unclosed block comment, and in that case we stop at the start of the comment.
 */
void mf_token_reader_skip_space(struct mf_token_reader *reader);

/* Read and consume the next token.
 * Returns >0 if (token) was populated, 0 at EOF, or <0 for lexical errors.
 * If you provide a context, we'll log errors against it. Null is ok.
 */
int mf_token_reader_next(struct mf_token *token,struct mf_token_reader *reader,struct eggdev_minify_js *ctx);

/* You can only unread one step back.
 * Update: You can "warp" to before any token previously returned by this reader.
 */
void mf_token_reader_unread(struct mf_token_reader *reader);
void mf_token_reader_warp(struct mf_token_reader *reader,const struct mf_token *token);

/* Convenience to peek at the next token. Nonzero if it matches exactly.
 */
int mf_token_reader_next_is(struct mf_token_reader *reader,const char *src,int srcc);

/* Returns the length of the longest operator at (src).
 * Optionally return the operator's class for precedence purposes.
 * Anything we don't recognize returns as 1 with class 0.
 */
int mf_measure_js_operator(const char *src,int srcc,int *cls);

/* We're going to call all high code points identifier-legal.
 * The spec is much more complex.
 * But then if you're using >U+7f in source code, you're already asking for trouble. Here it is.
 */
#define JSIDENT(ch) ( \
  (((ch)>='a')&&((ch)<='z'))|| \
  (((ch)>='A')&&((ch)<='Z'))|| \
  (((ch)>='0')&&((ch)<='9'))|| \
  ((ch)=='_')|| \
  ((ch)=='$')|| \
  ((ch)&0x80) \
)

#define MF_TOKEN_TYPE_STRING      1 /* Simple string, ie quotes or apostrophes. */
#define MF_TOKEN_TYPE_GRAVESTRING 2 /* Must be digested at compile, shouldn't survive into AST ops. */
#define MF_TOKEN_TYPE_REGEX       3 /* Inline regex with optional flags. */
#define MF_TOKEN_TYPE_NUMBER      4 /* Int or float, no leading sign, no leading dot. */
#define MF_TOKEN_TYPE_IDENTIFIER  5 /* Includes keywords and identifier-like operators. */
#define MF_TOKEN_TYPE_OPERATOR    6 /* Everything else. */

/* Higher classes are executed first.
 */
#define MF_OPCLS_SPECIAL -10
#define MF_OPCLS_OUTER     0
#define MF_OPCLS_SEQ      10
#define MF_OPCLS_ASSIGN   20
#define MF_OPCLS_SELECT   30
#define MF_OPCLS_LOR      40
#define MF_OPCLS_LAN      50
#define MF_OPCLS_BOR      60
#define MF_OPCLS_BXR      70
#define MF_OPCLS_BAN      80
#define MF_OPCLS_EQ       90
#define MF_OPCLS_CMP     100
#define MF_OPCLS_SHIFT   110
#define MF_OPCLS_ADD     120
#define MF_OPCLS_MLT     130
#define MF_OPCLS_EXP     140
#define MF_OPCLS_UNARY   150
#define MF_OPCLS_FIX     160
#define MF_OPCLS_CALL    170
#define MF_OPCLS_NEW     180
#define MF_OPCLS_MEMBER  190
#define MF_OPCLS_LAMBDA  200

static inline int mf_opcls_rtl(int opcls) {
  switch (opcls) {
    case MF_OPCLS_ASSIGN:
    case MF_OPCLS_EXP:
    case MF_OPCLS_UNARY:
      return 1;
  }
  return 0;
}

#endif
