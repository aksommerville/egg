#include "serial.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>

#define ENCV ((uint8_t*)encoder->v)

/* Cleanup.
 */

void sr_encoder_cleanup(struct sr_encoder *encoder) {
  if (encoder->v) free(encoder->v);
}

/* Grow buffer.
 */

int sr_encoder_require(struct sr_encoder *encoder,int addc) {
  if (addc<1) return 0;
  if (encoder->c<=encoder->a-addc) return 0;
  if (encoder->c>INT_MAX-addc) return -1;
  int na=encoder->c+addc;
  if (na<INT_MAX-256) na=(na+256)&~255;
  void *nv=realloc(encoder->v,na);
  if (!nv) return -1;
  encoder->v=nv;
  encoder->a=na;
  return 0;
}

/* Terminate.
 */
 
int sr_encoder_terminate(struct sr_encoder *encoder) {
  if (sr_encoder_require(encoder,1)<0) return -1;
  ENCV[encoder->c]=0;
  return 0;
}

/* Insert. Raw data only.
 */
 
int sr_encoder_insert(struct sr_encoder *encoder,int p,const void *src,int srcc) {
  if (!src) return 0;
  if (srcc<0) { srcc=0; while (((char*)src)[srcc]) srcc++; }
  if ((p<0)||(p>encoder->c)) return -1;
  if (sr_encoder_require(encoder,srcc)<0) return -1;
  char *v=encoder->v;
  memmove(v+p+srcc,v+p,encoder->c-p);
  memcpy(v+p,src,srcc);
  encoder->c+=srcc;
  return 0;
}

/* Append raw data.
 */

int sr_encode_raw(struct sr_encoder *encoder,const void *src,int srcc) {
  if (!src) return 0;
  if (srcc<0) { srcc=0; while (((char*)src)[srcc]) srcc++; }
  if (sr_encoder_require(encoder,srcc)<0) return -1;
  memcpy(ENCV+encoder->c,src,srcc);
  encoder->c+=srcc;
  return 0;
}

/* Append formatted string.
 */
 
int sr_encode_fmt(struct sr_encoder *encoder,const char *fmt,...) {
  if (!fmt||!fmt[0]) return 0;
  while (1) {
    va_list vargs;
    va_start(vargs,fmt);
    int err=vsnprintf(ENCV+encoder->c,encoder->a-encoder->c,fmt,vargs);
    if ((err<0)||(err>=INT_MAX)) return -1;
    if (encoder->c<encoder->a-err) { // sic < not <=
      encoder->c+=err;
      return 0;
    }
    if (sr_encoder_require(encoder,err+1)<0) return -1;
  }
}

/* Append zeroes.
 */

int sr_encode_zero(struct sr_encoder *encoder,int c) {
  if (c<1) return 0;
  if (sr_encoder_require(encoder,c)<0) return -1;
  memset(ENCV+encoder->c,0,c);
  encoder->c+=c;
  return 0;
}

/* Encode binary scalars.
 */

int sr_encode_u8(struct sr_encoder *encoder,int v) {
  if (sr_encoder_require(encoder,1)<0) return -1;
  ENCV[encoder->c++]=v;
  return 0;
}

int sr_encode_intbe(struct sr_encoder *encoder,int v,int size) {
  if ((size<1)||(size>4)) return -1;
  if (sr_encoder_require(encoder,size)<0) return -1;
  int i=size; while (i-->0) {
    ENCV[encoder->c+i]=v;
    v>>=8;
  }
  encoder->c+=size;
  return 0;
}

int sr_encode_intle(struct sr_encoder *encoder,int v,int size) {
  if ((size<1)||(size>4)) return -1;
  if (sr_encoder_require(encoder,size)<0) return -1;
  int i=size; while (i-->0) {
    ENCV[encoder->c++]=v;
    v>>=8;
  }
  return 0;
}

int sr_encode_vlq(struct sr_encoder *encoder,int v) {
  if (sr_encoder_require(encoder,4)<0) return -1;
  int err=sr_vlq_encode(ENCV+encoder->c,encoder->a-encoder->c,v);
  if ((err<0)||(encoder->c>encoder->a-err)) return -1;
  encoder->c+=err;
  return 0;
}

/* Raw chunk with length prefix.
 */

int sr_encode_intbelen(struct sr_encoder *encoder,const void *src,int srcc,int lenlen) {
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (((char*)src)[srcc]) srcc++; }
  if (lenlen>4) return -1;
  if (lenlen==4) ;
  else if (srcc>=1<<(lenlen*8)) return -1;
  if (sr_encode_intbe(encoder,srcc,lenlen)<0) return -1;
  if (sr_encode_raw(encoder,src,srcc)<0) return -1;
  return 0;
}

int sr_encode_intlelen(struct sr_encoder *encoder,const void *src,int srcc,int lenlen) {
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (((char*)src)[srcc]) srcc++; }
  if (lenlen>4) return -1;
  if (lenlen==4) ;
  else if (srcc>=1<<(lenlen*8)) return -1;
  if (sr_encode_intle(encoder,srcc,lenlen)<0) return -1;
  if (sr_encode_raw(encoder,src,srcc)<0) return -1;
  return 0;
}

int sr_encode_vlqlen(struct sr_encoder *encoder,const void *src,int srcc) {
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (((char*)src)[srcc]) srcc++; }
  if (sr_encode_vlq(encoder,srcc)<0) return -1;
  if (sr_encode_raw(encoder,src,srcc)<0) return -1;
  return 0;
}

/* Base64.
 */
 
int sr_encode_base64(struct sr_encoder *encoder,const void *src,int srcc) {
  for (;;) {
    char *dst=((char*)encoder->v)+encoder->c;
    int dsta=encoder->a-encoder->c;
    int err=sr_base64_encode(dst,dsta,src,srcc);
    if (err<0) return -1;
    if (encoder->c<=encoder->a-err) {
      encoder->c+=err;
      return 0;
    }
    if (sr_encoder_require(encoder,err)<0) return -1;
  }
}

/* JSON assert completion.
 */
 
int sr_encode_json_done(struct sr_encoder *encoder) {
  if (encoder->jsonctx) return encoder->jsonctx=-1;
  return 0;
}

/* JSON string, just the token.
 */
 
static int sr_encode_string_token(struct sr_encoder *encoder,const char *src,int srcc) {
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  if (sr_encoder_require(encoder,srcc+2)<0) return -1;
  while (1) {
    int err=sr_string_repr(ENCV+encoder->c,encoder->a-encoder->c,src,srcc);
    if (err<0) return encoder->jsonctx=-1;
    if (encoder->c<=encoder->a-err) {
      encoder->c+=err;
      return 0;
    }
    if (sr_encoder_require(encoder,err)<0) return -1;
  }
}

/* Check the last character emitted, skipping whitespace.
 */
 
static char sr_encoder_last_non_space_char(const struct sr_encoder *encoder) {
  int p=encoder->c;
  while (p&&((unsigned char)ENCV[p-1]<=0x20)) p--;
  if (!p) return 0;
  return ENCV[p-1];
}

/* JSON preamble.
 */
 
int sr_encode_json_preamble(struct sr_encoder *encoder,const char *k,int kc) {
  if (encoder->jsonctx<0) return -1;
  if (encoder->jsonctx=='{') {
    char last=sr_encoder_last_non_space_char(encoder);
    if (last==':') { // key was already emitted, that's perfectly legal.
      if (k) return encoder->jsonctx=-1;
    } else {
      if (!k) return encoder->jsonctx=-1;
      if ((last=='{')||(last==',')) ;
      else if (sr_encode_u8(encoder,',')<0) return encoder->jsonctx=-1;
      if (sr_encode_string_token(encoder,k,kc)<0) return encoder->jsonctx=-1;
      if (sr_encode_u8(encoder,':')<0) return encoder->jsonctx=-1;
    }
  } else {
    if (k) return encoder->jsonctx=-1;
    if (encoder->jsonctx=='[') {
      char last=sr_encoder_last_non_space_char(encoder);
      if ((last=='[')||(last==',')) ;
      else if (sr_encode_u8(encoder,',')<0) return encoder->jsonctx=-1;
    }
  }
  return 0;
}

/* JSON structures.
 */

int sr_encode_json_object_start(struct sr_encoder *encoder,const char *k,int kc) {
  if (sr_encode_json_preamble(encoder,k,kc)<0) return -1;
  if (sr_encode_u8(encoder,'{')<0) return encoder->jsonctx=-1;
  int pvctx=encoder->jsonctx;
  encoder->jsonctx='{';
  return pvctx;
}

int sr_encode_json_array_start(struct sr_encoder *encoder,const char *k,int kc) {
  if (sr_encode_json_preamble(encoder,k,kc)<0) return -1;
  if (sr_encode_u8(encoder,'[')<0) return encoder->jsonctx=-1;
  int pvctx=encoder->jsonctx;
  encoder->jsonctx='[';
  return pvctx;
}

int sr_encode_json_end(struct sr_encoder *encoder,int jsonctx) {
  if (encoder->jsonctx=='[') {
    if (sr_encode_u8(encoder,']')<0) return -1;
  } else if (encoder->jsonctx=='{') {
    if (sr_encode_u8(encoder,'}')<0) return -1;
  } else {
    return encoder->jsonctx=-1;
  }
  encoder->jsonctx=jsonctx;
  return 0;
}

/* JSON primitives.
 */

int sr_encode_json_preencoded(struct sr_encoder *encoder,const char *k,int kc,const char *v,int vc) {
  if (sr_encode_json_preamble(encoder,k,kc)<0) return -1;
  if (sr_encode_raw(encoder,v,vc)<0) return encoder->jsonctx=-1;
  return 0;
}

int sr_encode_json_null(struct sr_encoder *encoder,const char *k,int kc) {
  return sr_encode_json_preencoded(encoder,k,kc,"null",4);
}

int sr_encode_json_bool(struct sr_encoder *encoder,const char *k,int kc,int v) {
  return sr_encode_json_preencoded(encoder,k,kc,v?"true":"false",-1);
}

int sr_encode_json_int(struct sr_encoder *encoder,const char *k,int kc,int v) {
  if (sr_encode_json_preamble(encoder,k,kc)<0) return -1;
  while (1) {
    int err=sr_decsint_repr(ENCV+encoder->c,encoder->a-encoder->c,v);
    if (encoder->c<=encoder->a-err) {
      encoder->c+=err;
      return 0;
    }
    if (sr_encoder_require(encoder,err)<0) return encoder->jsonctx=-1;
  }
}

int sr_encode_json_double(struct sr_encoder *encoder,const char *k,int kc,double v) {
  if (sr_encode_json_preamble(encoder,k,kc)<0) return -1;
  while (1) {
    int err=sr_double_repr(ENCV+encoder->c,encoder->a-encoder->c,v);
    if (encoder->c<=encoder->a-err) {
      encoder->c+=err;
      return 0;
    }
    if (sr_encoder_require(encoder,err)<0) return encoder->jsonctx=-1;
  }
}

int sr_encode_json_string(struct sr_encoder *encoder,const char *k,int kc,const char *v,int vc) {
  if (!v) vc=0; else if (vc<0) { vc=0; while (v[vc]) vc++; }
  if (sr_encode_json_preamble(encoder,k,kc)<0) return -1;
  while (1) {
    int err=sr_string_repr(ENCV+encoder->c,encoder->a-encoder->c,v,vc);
    if (err<0) return encoder->jsonctx=-1;
    if (encoder->c<=encoder->a-err) {
      encoder->c+=err;
      return 0;
    }
    if (sr_encoder_require(encoder,err)<0) return encoder->jsonctx=-1;
  }
}

int sr_encode_json_base64(struct sr_encoder *encoder,const char *k,int kc,const void *v,int vc) {
  if (!v) vc=0; else if (vc<0) { vc=0; while (((char*)v)[vc]) vc++; }
  if (sr_encode_json_preamble(encoder,k,kc)<0) return -1;
  // Nothing in the base64 alphabet needs escaped in a JSON string.
  // We happen to know this, so we cheat a little and encode right into the buffer.
  if (sr_encode_u8(encoder,'"')<0) return encoder->jsonctx=-1;
  while (1) {
    int err=sr_base64_encode(ENCV+encoder->c,encoder->a-encoder->c,v,vc);
    if (err<0) return encoder->jsonctx=-1;
    if (encoder->c<=encoder->a-err) {
      encoder->c+=err;
      if (sr_encode_u8(encoder,'"')<0) return encoder->jsonctx=-1;
      return 0;
    }
    if (sr_encoder_require(encoder,err)<0) return encoder->jsonctx=-1;
  }
}
