/* serial.h
 */
 
#ifndef SERIAL_H
#define SERIAL_H

#include <stdint.h>

/* Text primitives.
 ***************************************************************/
 
static inline int sr_digit_eval(char ch) {
  if ((ch>='0')&&(ch<='9')) return ch-'0';
  if ((ch>='a')&&(ch<='z')) return ch-'a'+10;
  if ((ch>='A')&&(ch<='Z')) return ch-'A'+10;
  return -1;
}
 
int sr_memcasecmp(const void *a,const void *b,int c);

/* <0=malformed
 *  0=overflow
 *  1=overflow into sign bit
 *  2=full success
 */
int sr_int_eval(int *v,const char *src,int srcc);

int sr_decsint_repr(char *dst,int dsta,int v);
int sr_decuint_repr(char *dst,int dsta,int v,int mindigitc);
int sr_hexuint_repr(char *dst,int dsta,int v,int prefix,int mindigitc);
int sr_decsint64_repr(char *dst,int dsta,int64_t v);

int sr_double_eval(double *v,const char *src,int srcc);
int sr_double_repr(char *dst,int dsta,double v);

/* Length of an integer or float token.
 * Everything legal in JSON is handled properly, and some other constructions.
 * Optional (*flags) populated with features we discover.
 * If BASE, it can only be read as integer.
 * If FRACT or EXP, it can only be read as float.
 */
int sr_number_measure(const char *src,int srcc,int *flags);
#define SR_NUMBER_BASE        0x0001
#define SR_NUMBER_FRACT       0x0002
#define SR_NUMBER_EXP         0x0004

/* Evaluated side is expected to be UTF-8 and falls back to 8859-1 on a character-by-character basis as needed.
 * Represented side is JSON, but we accept some constructions at eval that are not legal JSON.
 * Optional (*simple) at measure is filled nonzero if the input had no escapes, ie you can read from the token directly.
 */
int sr_string_eval(char *dst,int dsta,const char *src,int srcc);
int sr_string_repr(char *dst,int dsta,const char *src,int srcc);
int sr_string_measure(const char *src,int srcc,int *simple);

/* "false","true" <=> 0,1
 * And lots of other aliases.
 * The empty string is false.
 */
int sr_bool_eval(int *v,const char *src,int srcc);
int sr_bool_repr(char *dst,int dsta,int v);

int sr_vlq_encode(void *dst,int dsta,int v);
int sr_vlq_decode(int *v,const void *src,int srcc);

int sr_utf8_encode(void *dst,int dsta,int v);
int sr_utf8_decode(int *v,const void *src,int srcc);

/* Opinionated pattern matcher.
 * Whitespace collapses, letters are case-insensitive, and '*' in (pat) matches any amount of anything.
 */
int sr_pattern_match(const char *pat,int patc,const char *src,int srcc);

/* Standard encodings and hashes.
 ****************************************************************/

/* Equivalent to a browser's encodeURIComponent/decodeURIComponent.
 */
int sr_url_encode(char *dst,int dsta,const char *src,int srcc);
int sr_url_decode(char *dst,int dsta,const char *src,int srcc);

/* Standard alphabet: A-Z a-z 0-9 + /
 */
int sr_base64_encode(char *dst,int dsta,const void *src,int srcc);
int sr_base64_decode(void *dst,int dsta,const char *src,int srcc);

/* Structured encoder.
 * Caller can yoink (encoder.v), otherwise you must cleanup.
 ****************************************************************/
 
struct sr_encoder {
  void *v;
  int c,a;
  int jsonctx; // 0,-1,'[','{'
};

void sr_encoder_cleanup(struct sr_encoder *encoder);

int sr_encoder_require(struct sr_encoder *encoder,int addc);
int sr_encoder_terminate(struct sr_encoder *encoder);
int sr_encoder_insert(struct sr_encoder *encoder,int p,const void *src,int srcc);

int sr_encode_raw(struct sr_encoder *encoder,const void *src,int srcc);
int sr_encode_fmt(struct sr_encoder *encoder,const char *fmt,...);
int sr_encode_zero(struct sr_encoder *encoder,int c);

int sr_encode_u8(struct sr_encoder *encoder,int v);
int sr_encode_intbe(struct sr_encoder *encoder,int v,int size);
int sr_encode_intle(struct sr_encoder *encoder,int v,int size);
int sr_encode_vlq(struct sr_encoder *encoder,int v);

int sr_encode_intbelen(struct sr_encoder *encoder,const void *src,int srcc,int lenlen);
int sr_encode_intlelen(struct sr_encoder *encoder,const void *src,int srcc,int lenlen);
int sr_encode_vlqlen(struct sr_encoder *encoder,const void *src,int srcc);

int sr_encode_base64(struct sr_encoder *encoder,const void *src,int srcc);

/* JSON structures and helpers.
 * All JSON-related errors are sticky: Once something fails, no further JSON calls will succeed.
 * You can safely defer error detection to the end, if that's more convenient.
 *
 * "done" asserts a clean context, ie no structure open and no sticky errors. Not required.
 *
 * "preamble" asserts that a key is present if required, and absent if forbidden, and encodes it.
 * In general, beginning any value inside an object requires that a key be provided.
 * But you can "preamble" ahead of time, and then provide the value with a null key.
 * This may be useful for nested encoders where the parent doesn't know or care what shape the children take.
 *
 * Both of the "start" return <0 or a token that you must return to us on the corresponding "end".
 * That's how we get away with a finite context.
 */
int sr_encode_json_done(struct sr_encoder *encoder);
int sr_encode_json_preamble(struct sr_encoder *encoder,const char *k,int kc);
int sr_encode_json_object_start(struct sr_encoder *encoder,const char *k,int kc);
int sr_encode_json_array_start(struct sr_encoder *encoder,const char *k,int kc);
int sr_encode_json_end(struct sr_encoder *encoder,int jsonctx);

/* JSON primitives.
 * "preencoded", you supply the exact value and we don't validate it or anything.
 * "base64" is a convenience that produces a string token by base64-encoding the input.
 */
int sr_encode_json_preencoded(struct sr_encoder *encoder,const char *k,int kc,const char *v,int vc);
int sr_encode_json_null(struct sr_encoder *encoder,const char *k,int kc);
int sr_encode_json_bool(struct sr_encoder *encoder,const char *k,int kc,int v);
int sr_encode_json_int(struct sr_encoder *encoder,const char *k,int kc,int v);
int sr_encode_json_double(struct sr_encoder *encoder,const char *k,int kc,double v);
int sr_encode_json_string(struct sr_encoder *encoder,const char *k,int kc,const char *v,int vc);
int sr_encode_json_base64(struct sr_encoder *encoder,const char *k,int kc,const void *v,int vc);

/* Structured decoder.
 * Initialize (v,c), zero the rest, and no cleanup necessary.
 ***********************************************************************************/
 
struct sr_decoder {
  const void *v;
  int p,c;
  int jsonctx; // 0,-1,'{','['
};

/* Next line of text, including the newline.
 * Zero at EOF.
 */
int sr_decode_line(const char **vpp,struct sr_decoder *decoder);

int sr_decode_u8(struct sr_decoder *decoder);
int sr_decode_intbe(int *v,struct sr_decoder *decoder,int len);
int sr_decode_intle(int *v,struct sr_decoder *decoder,int len);
int sr_decode_vlq(int *v,struct sr_decoder *decoder);

int sr_decode_raw(void *dstpp,struct sr_decoder *decoder,int len);
int sr_decode_intbelen(void *dstpp,struct sr_decoder *decoder,int lenlen);
int sr_decode_intlelen(void *dstpp,struct sr_decoder *decoder,int lenlen);
int sr_decode_vlqlen(void *dstpp,struct sr_decoder *decoder);

/* Same as encoding, "start" returns an opaque token that you must return at "end".
 * "done" is optional, it only asserts that no JSON structure is open or failed.
 * Errors are sticky. Once one JSON call fails, no further one will succeed.
 */
int sr_decode_json_done(const struct sr_decoder *decoder);
int sr_decode_json_object_start(struct sr_decoder *decoder);
int sr_decode_json_array_start(struct sr_decoder *decoder);
int sr_decode_json_end(struct sr_decoder *decoder,int jsonctx);

/* Get the next member from a JSON object or array.
 * For arrays: (kpp) must be NULL. Returns >0 if another value is present.
 * For objects: (kpp) must NOT be NULL. Returns key length.
 * We don't decode keys (there's nowhere to decode them to).
 * You get the inside of the string token, or the entire token if not simple.
 */
int sr_decode_json_next(const char **kpp,struct sr_decoder *decoder);

/* Return something that describes the next token:
 *  n : null
 *  t : true
 *  f : false
 *  0 : number
 *  " : string
 *  { : object
 *  [ : array
 *  \0: error
 */
char sr_decode_json_peek(const struct sr_decoder *decoder);

/* Read one expression and return it verbatim.
 * You could use this eg to yoink content for later decoding, or for skipping a field.
 */
int sr_decode_json_expression(const char **dstpp,struct sr_decoder *decoder);
static inline int sr_decode_json_skip(struct sr_decoder *decoder) { return sr_decode_json_expression(0,decoder); }

/* Everything can decode as string. At the limit, we might return the verbatim text.
 * Similar to encode, we offer a convenience to automatically decode base64.
 * If a plain string or base64 decode returns >dsta, the decoder does not advance and you must try again or skip.
 */
int sr_decode_json_string(char *dst,int dsta,struct sr_decoder *decoder);
int sr_decode_json_string_to_encoder(struct sr_encoder *dst,struct sr_decoder *decoder);
int sr_decode_json_base64(char *dst,int dsta,struct sr_decoder *decoder);
int sr_decode_json_base64_to_encoder(struct sr_encoder *dst,struct sr_decoder *decoder);

/* Decoding to int or double, we will attempt to convert as needed.
 * Strings may contain JSON number tokens, they decode transparently.
 * Empty string is 0.
 * Floats truncate when read as int.
 * null and false are 0, true is 1.
 * Arrays and objects can not be read as numbers.
 */
int sr_decode_json_int(int *dst,struct sr_decoder *decoder);
int sr_decode_json_double(double *dst,struct sr_decoder *decoder);

/* Length of one JSON expression, including leading space.
 */
int sr_json_measure(const char *src,int srcc);

#endif
