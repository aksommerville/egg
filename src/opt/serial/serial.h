/* serial.h
 * Primitive evaluation and conversion of serial data.
 * Structured encoder and decoder.
 *
 * General rules:
 *  - Evaluation requires a measured token.
 *  - Measuring a token does not guarantee it will evaluate.
 *  - (srcc) may be negative, meaning (src) is NUL-terminated.
 *  - With an explicit length, embedded NULs are permitted.
 *  - Anything producing text appends an uncounted NUL *only if space is available*.
 *  - ^ Note that this is NOT how most libc functions works.
 *  - Text is generally presumed UTF-8 but we mostly tolerate encoding errors.
 */

#ifndef SERIAL_H
#define SERIAL_H

struct sr_encoder;
struct sr_decoder;

/* Text primitives.
 *****************************************************************************/

static inline int sr_digit_eval(char src) {
  if ((src>='0')&&(src<='9')) return src-'0';
  if ((src>='a')&&(src<='z')) return src-'a'+10;
  if ((src>='A')&&(src<='Z')) return src-'A'+10;
  return -1;
}

int sr_memcasecmp(const void *a,const void *b,int c);
int sr_space_measure(const char *src,int srcc);

/* Integers as string.
 * Tokens are [+-]?[0-9][0-9a-zA-Z]*
 * May lead with base: 0x 0X 0d 0D 0o 0O 0b 0B
 * Eval returns 2 on full success, 1 for sign-bit overflow, 0 for real overflow, or -1 if malformed.
 */
int sr_int_measure(const char *src,int srcc);
int sr_int_eval(int *dst,const char *src,int srcc);
int sr_decsint_repr(char *dst,int dsta,int src);
int sr_decuint_repr(char *dst,int dsta,int src,int mindigitc);
int sr_hexuint_repr(char *dst,int dsta,int src,int mindigitc);
int sr_hexuint_repr_prefixed(char *dst,int dsta,int src,int mindigitc);

/* Doubles as string.
 * The "json" format produces "null" for inf and nan, and no exponents, always JSON-legal.
 * Tokens are [+-][0-9]+(\.[0-9]+)?([eE][-+]?[0-9]+)?
 * Note that unlike C, a digit is required in each position if introduced. (eg ".0" and "0." are not legal).
 */
int sr_double_measure(const char *src,int srcc);
int sr_double_eval(double *dst,const char *src,int srcc);
int sr_double_repr(char *dst,int dsta,double src);
int sr_double_repr_json(char *dst,int dsta,double src);

/* String tokens.
 * Our format uses "\U" and "\x" escapes which are not valid in JSON.
 */
int sr_string_measure(const char *src,int srcc,int *simple);
int sr_string_eval(char *dst,int dsta,const char *src,int srcc);
int sr_string_repr(char *dst,int dsta,const char *src,int srcc);
int sr_string_repr_json(char *dst,int dsta,const char *src,int srcc);

/* Call for each (sep)-delimited string in (src).
 * We trim leading and trailing space, and do report empty items.
 * If (sep) is whitespace, splits on any whitespace and does not report empties.
 */
int sr_string_split(
  const char *src,int srcc,
  char sep,
  int (*cb)(const char *word,int wordc,void *userdata),
  void *userdata
);

/* Simple pattern matching.
 *   *    Any amount of anything.
 *   \N   Literal "N", case-sensitive.
 * Leading and trailing space are ignored in both pattern and query.
 * Any nonzero amount of whitespace matches any nonzero amount of whitespace.
 * ASCII letters are case-insensitive.
 * Returns zero for mismatch, >0 if matched, with higher numbers indicating more literal matches.
 * Never returns negative.
 *****************************************************************************/

int sr_pattern_match(const char *pat,int patc,const char *src,int srcc);

/* Binary primitives.
 *****************************************************************************/

/* Plain integers with a (size) in bytes: 1..4
 * Decode with negative (size) to extend the sign bit.
 */
int sr_intbe_encode(void *dst,int dsta,int src,int size);
int sr_intle_encode(void *dst,int dsta,int src,int size);
int sr_intbe_decode(int *v,const void *src,int srcc,int size);
int sr_intle_decode(int *v,const void *src,int srcc,int size);

/* Fixed-point numbers as floats.
 * Whole and fraction sizes are in bits.
 * (sign+wholesize+fractsize) must be 8, 16, 24, or 32; and (fractsize) can't be more than 24.
 * We fuzz that a little: You don't need to subtract one when (sign) in play, we figure it out.
 */
int sr_fixed_encode(void *dst,int dsta,double src,int sign,int wholesize,int fractsize);
int sr_fixed_decode(double *dst,const void *src,int srcc,int sign,int wholesize,int fractsize);

int sr_utf8_encode(void *dst,int dsta,int src);
int sr_utf8_decode(int *dst,const void *src,int srcc);

int sr_vlq_encode(void *dst,int dsta,int src);
int sr_vlq_decode(int *dst,const void *src,int srcc);

/* Common encodings.
 *****************************************************************************/

int sr_base64_encode(char *dst,int dsta,const void *src,int srcc);
int sr_base64_decode(void *dst,int dsta,const char *src,int srcc);

/* Intended to behave just like standard encodeURIComponent/decodeURIComponent.
 * But I'll probably mess it up in small ways.
 * Decoding is simple: "%NN" becomes one byte, error if malformed, everything else is verbatim.
 * Encoding emits "%NN" for any byte in (src) outside G0, and these:
 *    "#$%&+,/:;<=>?@[\]^`{|}
 * (which list I acquired experimentally in Google Chrome).
 * Or if it helps, the ones that *don't* escape (along with letters and digits):
 *   !'()*-._~
 */
int sr_url_encode(char *dst,int dsta,const char *src,int srcc);
int sr_url_decode(char *dst,int dsta,const char *src,int srcc);

/* md5: 16 bytes out.
 * sha1: 20 bytes out.
 * These will tell you if it's insufficient, as usual, but the lengths are constant.
 */
int sr_md5(void *dst,int dsta,const void *src,int srcc);
int sr_sha1(void *dst,int dsta,const void *src,int srcc);

/* Structured decoder.
 * Decoders do not require cleanup and can safely be copied directly.
 *****************************************************************************/

struct sr_decoder {
  const void *v;
  int c;
  int p;
  int jsonctx;
};

static inline int sr_decoder_remaining(const struct sr_decoder *decoder) {
  return decoder->c-decoder->p;
}
static inline int sr_decoder_require(const struct sr_decoder *decoder,int reqc) {
  if (reqc<1) return 0;
  if (decoder->p>decoder->c-reqc) return -1;
  return 0;
}

/* Through the next newline. Zero at EOF, never negative.
 */
int sr_decode_line(void *dstpp,struct sr_decoder *decoder);

/* Binary primitives.
 */
int sr_decode_u8(struct sr_decoder *decoder);
int sr_decode_intbe(int *v,struct sr_decoder *decoder,int size_bytes);
int sr_decode_intle(int *v,struct sr_decoder *decoder,int size_bytes);
int sr_decode_fixed(double *v,struct sr_decoder *decoder,int sign,int wholesize_bits,int fractsize_bits);
int sr_decode_utf8(int *v,struct sr_decoder *decoder);
int sr_decode_vlq(int *v,struct sr_decoder *decoder);

/* Binary chunks.
 */
int sr_decode_raw(void *dstpp,struct sr_decoder *decoder,int len);
int sr_decode_intbelen(void *dstpp,struct sr_decoder *decoder,int lensize_bytes);
int sr_decode_intlelen(void *dstpp,struct sr_decoder *decoder,int lensize_bytes);
int sr_decode_vlqlen(void *dstpp,struct sr_decoder *decoder);

int sr_json_measure(const char *src,int srcc);

int sr_int_from_json_expression(int *dst,const char *src,int srcc);
int sr_double_from_json_expression(double *dst,const char *src,int srcc);
int sr_boolean_from_json_expression(const char *src,int srcc);

/* JSON primitives.
 * Everything can be read as expression or primitive.
 * Expression gives you verbatim encoded text.
 * "skip" is the same as "expression(0)", just a clearer name for documentation.
 * "peek" does not change any state and guesses the next expression type:
 *   0   Invalid.
 *   '{' Object.
 *   '[' Array.
 *   '"' String.
 *   '#' Number.
 *   'n' Null.
 *   't' True.
 *   'f' False.
 * Reading as string: String tokens evaluate, and everything else returns the raw encoded text.
 * As (int,double,boolean): Empty strings are (0,0.0,0), strings containing a JSON token are evaluated recursively, and everything else is (-1,NAN,1).
 * Note that empty objects and empty arrays are True, but empty strings are False.
 */
char sr_decode_json_peek(const struct sr_decoder *decoder);
int sr_decode_json_skip(struct sr_decoder *decoder);
int sr_decode_json_expression(void *dstpp,struct sr_decoder *decoder);
int sr_decode_json_int(int *dst,struct sr_decoder *decoder);
int sr_decode_json_double(double *dst,struct sr_decoder *decoder);
int sr_decode_json_boolean(struct sr_decoder *decoder); // => 0,1, or <0 for errors

/* Reading strings is special.
 * If the result >dsta, the decoder's read head did not advance and you must call again (or skip).
 * All other JSON decode calls advance the read head no matter what.
 */
int sr_decode_json_string(char *dst,int dsta,struct sr_decoder *decoder);

/* An alternative to sr_decode_json_string() with no length problems.
 */
int sr_decode_json_string_to_encoder(struct sr_encoder *dst,struct sr_decoder *decoder);

/* To start reading an array or object, call one of the "start" and record what it returns.
 * Then call sr_decode_json_next() until it returns <=0.
 * If you are reading an object, you *must* provide (kpp) to hold the key.
 * If you are reading an array, (kpp) *must* be null.
 * Simple keys will have their quotes stripped.
 * If a key is empty or contains an escape, you'll get the token as encoded.
 * At the end of a structure, after 'next' returns zero, you must "end" it with the "jsonctx" returned at start.
 * You can't 'end' early; if you need to, you have to 'skip' each member in turn, until the real end.
 * Errors are sticky, it's fine to defer error processing to "end".
 * We tolerate a few things not allowed by the spec. (so everything works, but you can't trust us as a validator).
 */
int sr_decode_json_object_start(struct sr_decoder *decoder); // => jsonctx
int sr_decode_json_array_start(struct sr_decoder *decoder); // => jsonctx
int sr_decode_json_next(void *kpp,struct sr_decoder *decoder);
int sr_decode_json_end(struct sr_decoder *decoder,int jsonctx);

/* Iterate a JSON array or object.
 * Stops if you return nonzero and returns the same.
 * (v,vc) are the member value in JSON (pointing directly into (src)).
 * (k,kc) are plain text. But see notes above, we actually don't decode keys fully.
 * If (src) contains trailing text after the end of the structure, we quietly ignore it.
 */
int sr_for_each_of_json_array(
  const char *src,int srcc,
  int (*cb)(const char *v,int vc,void *userdata),
  void *userdata
);
int sr_for_each_of_json_object(
  const char *src,int srcc,
  int (*cb)(const char *k,int kc,const char *v,int vc,void *userdata),
  void *userdata
);

/* Structured encoder.
 * These do require cleanup and must not be copied directly.
 * You may yoink (v) after encoding, and then do not clean up the encoder.
 *****************************************************************************/

struct sr_encoder {
  char *v;
  int c,a;
  int jsonctx;
};

void sr_encoder_cleanup(struct sr_encoder *encoder);
int sr_encoder_require(struct sr_encoder *encoder,int addc);
int sr_encoder_replace(struct sr_encoder *encoder,int p,int c,const void *src,int srcc);
int sr_encoder_terminate(struct sr_encoder *encoder);

int sr_encode_u8(struct sr_encoder *encoder,int src);
int sr_encode_intbe(struct sr_encoder *encoder,int src,int size_bytes);
int sr_encode_intle(struct sr_encoder *encoder,int src,int size_bytes);
int sr_encode_fixed(struct sr_encoder *encoder,double src,int sign,int wholesize_bits,int fractsize_bits);
int sr_encode_utf8(struct sr_encoder *encoder,int src);
int sr_encode_vlq(struct sr_encoder *encoder,int src);

int sr_encode_raw(struct sr_encoder *encoder,const void *src,int srcc);
int sr_encode_intbelen(struct sr_encoder *encoder,const void *src,int srcc,int size_bytes);
int sr_encode_intlelen(struct sr_encoder *encoder,const void *src,int srcc,int size_bytes);
int sr_encode_vlqlen(struct sr_encoder *encoder,const void *src,int srcc);

int sr_encode_fmt(struct sr_encoder *encoder,const char *fmt,...);

/* Open and close JSON structures.
 * Within an array, you *must* supply (0,0) for all keys.
 * Within an object, (k) must never be null.
 * As with decoding, opening the structure returns a context token that you must give back at close.
 */
int sr_encode_json_object_start(struct sr_encoder *encoder,const char *k,int kc); // => jsonctx
int sr_encode_json_array_start(struct sr_encoder *encoder,const char *k,int kc); // => jsonctx
int sr_encode_json_object_start_no_setup(struct sr_encoder *encoder); // => jsonctx
int sr_encode_json_array_start_no_setup(struct sr_encoder *encoder); // => jsonctx
int sr_encode_json_object_end(struct sr_encoder *encoder,int jsonctx);
int sr_encode_json_array_end(struct sr_encoder *encoder,int jsonctx);

/* Encode JSON primitives.
 * Use "preencoded" if you have a valid JSON expression ready to use. We don't validate it or anything.
 * We provide a "base64" convenience which base64-encodes the input and emits that as a JSON string.
 */
int sr_encode_json_null(struct sr_encoder *encoder,const char *k,int kc);
int sr_encode_json_boolean(struct sr_encoder *encoder,const char *k,int kc,int src);
int sr_encode_json_int(struct sr_encoder *encoder,const char *k,int kc,int src);
int sr_encode_json_double(struct sr_encoder *encoder,const char *k,int kc,double src);
int sr_encode_json_string(struct sr_encoder *encoder,const char *k,int kc,const char *src,int srcc);
int sr_encode_json_base64(struct sr_encoder *encoder,const char *k,int kc,const void *src,int srcc);
int sr_encode_json_preencoded(struct sr_encoder *encoder,const char *k,int kc,const char *src,int srcc);

// Emit key, object/array framing, context validation.
int sr_encode_json_setup(struct sr_encoder *encoder,const char *k,int kc);

#endif
