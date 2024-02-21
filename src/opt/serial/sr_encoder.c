#include "serial.h"
#include <string.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

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

/* General replacement.
 */
 
int sr_encoder_replace(struct sr_encoder *encoder,int p,int c,const void *src,int srcc) {
  if (p<0) p=encoder->c;
  if (c<0) c=encoder->c-p;
  if (p<0) return -1;
  if (c<0) return -1;
  if (p>encoder->c-c) return -1;
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (((char*)src)[srcc]) srcc++; }
  if (c!=srcc) {
    if (sr_encoder_require(encoder,srcc-c)<0) return -1;
    memmove(encoder->v+p+srcc,encoder->v+p+c,encoder->c-c-p);
    encoder->c+=srcc-c;
  }
  memcpy(encoder->v+p,src,srcc);
  return 0;
}

/* Require an uncounted terminator.
 */
 
int sr_encoder_terminate(struct sr_encoder *encoder) {
  if (sr_encoder_require(encoder,1)<0) return -1;
  encoder->v[encoder->c]=0;
  return 0;
}

/* Encode binary scalars.
 */

int sr_encode_u8(struct sr_encoder *encoder,int src) {
  if (sr_encoder_require(encoder,1)<0) return -1;
  encoder->v[encoder->c++]=src;
  return 0;
}

int sr_encode_intbe(struct sr_encoder *encoder,int src,int size_bytes) {
  if (sr_encoder_require(encoder,4)<0) return -1;
  int err=sr_intbe_encode(encoder->v+encoder->c,encoder->a-encoder->c,src,size_bytes);
  if ((err<1)||(err>4)) return -1;
  encoder->c+=err;
  return 0;
}

int sr_encode_intle(struct sr_encoder *encoder,int src,int size_bytes) {
  if (sr_encoder_require(encoder,4)<0) return -1;
  int err=sr_intle_encode(encoder->v+encoder->c,encoder->a-encoder->c,src,size_bytes);
  if ((err<1)||(err>4)) return -1;
  encoder->c+=err;
  return 0;
}

int sr_encode_fixed(struct sr_encoder *encoder,double src,int sign,int wholesize_bits,int fractsize_bits) {
  if (sr_encoder_require(encoder,4)<0) return -1;
  int err=sr_fixed_encode(encoder->v+encoder->c,encoder->a-encoder->c,src,sign,wholesize_bits,fractsize_bits);
  if ((err<1)||(err>4)) return -1;
  encoder->c+=err;
  return 0;
}

int sr_encode_utf8(struct sr_encoder *encoder,int src) {
  if (sr_encoder_require(encoder,4)<0) return -1;
  int err=sr_utf8_encode(encoder->v+encoder->c,encoder->a-encoder->c,src);
  if ((err<1)||(err>4)) return -1;
  encoder->c+=err;
  return 0;
}

int sr_encode_vlq(struct sr_encoder *encoder,int src) {
  if (sr_encoder_require(encoder,4)<0) return -1;
  int err=sr_vlq_encode(encoder->v+encoder->c,encoder->a-encoder->c,src);
  if ((err<1)||(err>4)) return -1;
  encoder->c+=err;
  return 0;
}

/* Encode chunks with leading length.
 */

int sr_encode_raw(struct sr_encoder *encoder,const void *src,int srcc) {
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (((char*)src)[srcc]) srcc++; }
  if (sr_encoder_require(encoder,srcc)<0) return -1;
  memcpy(encoder->v+encoder->c,src,srcc);
  encoder->c+=srcc;
  return 0;
}

int sr_encode_intbelen(struct sr_encoder *encoder,const void *src,int srcc,int size_bytes) {
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (((char*)src)[srcc]) srcc++; }
  if (sr_encode_intbe(encoder,srcc,size_bytes)<0) return -1;
  if (sr_encoder_require(encoder,srcc)<0) return -1;
  memcpy(encoder->v+encoder->c,src,srcc);
  encoder->c+=srcc;
  return 0;
}

int sr_encode_intlelen(struct sr_encoder *encoder,const void *src,int srcc,int size_bytes) {
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (((char*)src)[srcc]) srcc++; }
  if (sr_encode_intle(encoder,srcc,size_bytes)<0) return -1;
  if (sr_encoder_require(encoder,srcc)<0) return -1;
  memcpy(encoder->v+encoder->c,src,srcc);
  encoder->c+=srcc;
  return 0;
}

int sr_encode_vlqlen(struct sr_encoder *encoder,const void *src,int srcc) {
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (((char*)src)[srcc]) srcc++; }
  if (sr_encode_vlq(encoder,srcc)<0) return -1;
  if (sr_encoder_require(encoder,srcc)<0) return -1;
  memcpy(encoder->v+encoder->c,src,srcc);
  encoder->c+=srcc;
  return 0;
}

/* Encode formatted text.
 */

int sr_encode_fmt(struct sr_encoder *encoder,const char *fmt,...) {
  if (!fmt||!fmt[0]) return 0;
  while (1) {
    va_list vargs;
    va_start(vargs,fmt);
    int err=vsnprintf(encoder->v+encoder->c,encoder->a-encoder->c,fmt,vargs);
    if ((err<0)||(err>=INT_MAX)) return -1;
    if (encoder->c<encoder->a-err) {
      encoder->c+=err;
      return 0;
    }
    if (sr_encoder_require(encoder,err+1)<0) return -1;
  }
}

/* JSON collections.
 */

int sr_encode_json_object_start(struct sr_encoder *encoder,const char *k,int kc) {
  if (sr_encode_json_setup(encoder,k,kc)<0) return -1;
  int jsonctx=encoder->jsonctx;
  if (sr_encode_u8(encoder,'{')<0) return -1;
  encoder->jsonctx='{';
  return jsonctx;
}

int sr_encode_json_array_start(struct sr_encoder *encoder,const char *k,int kc) {
  if (sr_encode_json_setup(encoder,k,kc)<0) return -1;
  int jsonctx=encoder->jsonctx;
  if (sr_encode_u8(encoder,'[')<0) return -1;
  encoder->jsonctx='[';
  return jsonctx;
}

int sr_encode_json_object_start_no_setup(struct sr_encoder *encoder) {
  int jsonctx=encoder->jsonctx;
  if (sr_encode_u8(encoder,'{')<0) return -1;
  encoder->jsonctx='{';
  return jsonctx;
}

int sr_encode_json_array_start_no_setup(struct sr_encoder *encoder) {
  int jsonctx=encoder->jsonctx;
  if (sr_encode_u8(encoder,'[')<0) return -1;
  encoder->jsonctx='[';
  return jsonctx;
}

int sr_encode_json_object_end(struct sr_encoder *encoder,int jsonctx) {
  if (encoder->jsonctx!='{') return encoder->jsonctx=-1;
  if (sr_encode_u8(encoder,'}')<0) return -1;
  encoder->jsonctx=jsonctx;
  return 0;
}

int sr_encode_json_array_end(struct sr_encoder *encoder,int jsonctx) {
  if (encoder->jsonctx!='[') return encoder->jsonctx=-1;
  if (sr_encode_u8(encoder,']')<0) return -1;
  encoder->jsonctx=jsonctx;
  return 0;
}

/* JSON primitives.
 */

int sr_encode_json_null(struct sr_encoder *encoder,const char *k,int kc) {
  if (sr_encode_json_setup(encoder,k,kc)<0) return -1;
  if (sr_encode_raw(encoder,"null",4)<0) return -1;
  return 0;
}

int sr_encode_json_boolean(struct sr_encoder *encoder,const char *k,int kc,int src) {
  if (sr_encode_json_setup(encoder,k,kc)<0) return -1;
  if (sr_encode_raw(encoder,src?"true":"false",-1)<0) return -1;
  return 0;
}

int sr_encode_json_int(struct sr_encoder *encoder,const char *k,int kc,int src) {
  if (sr_encode_json_setup(encoder,k,kc)<0) return -1;
  if (sr_encode_fmt(encoder,"%d",src)<0) return -1;
  return 0;
}

int sr_encode_json_double(struct sr_encoder *encoder,const char *k,int kc,double src) {
  if (sr_encode_json_setup(encoder,k,kc)<0) return -1;
  if (sr_encode_fmt(encoder,"%f",src)<0) return -1; // libc "%f" is not necessarily JSON compatible... is it a problem?
  return 0;
}

int sr_encode_json_string(struct sr_encoder *encoder,const char *k,int kc,const char *src,int srcc) {
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  if (sr_encode_json_setup(encoder,k,kc)<0) return -1;
  while (1) {
    int err=sr_string_repr_json(encoder->v+encoder->c,encoder->a-encoder->c,src,srcc);
    if (err<0) return -1;
    if (encoder->c<=encoder->a-err) {
      encoder->c+=err;
      return 0;
    }
    if (sr_encoder_require(encoder,err)<0) return -1;
  }
}

int sr_encode_json_base64(struct sr_encoder *encoder,const char *k,int kc,const void *src,int srcc) {
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (((char*)src)[srcc]) srcc++; }
  if (sr_encode_json_setup(encoder,k,kc)<0) return -1;
  while (1) {
    int err=sr_base64_encode(encoder->v+encoder->c,encoder->a-encoder->c,src,srcc);
    if (err<0) return -1;
    if (encoder->c<=encoder->a-err) {
      encoder->c+=err;
      return 0;
    }
    if (sr_encoder_require(encoder,err)<0) return -1;
  }
}

int sr_encode_json_preencoded(struct sr_encoder *encoder,const char *k,int kc,const char *src,int srcc) {
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  if (sr_encode_json_setup(encoder,k,kc)<0) return -1;
  if (sr_encode_raw(encoder,src,srcc)<0) return -1;
  return 0;
}

/* Prepare for a JSON expression.
 */

static int sr_encoder_json_comma_if_needed(struct sr_encoder *encoder) {
  int p=encoder->c;
  while (p-->0) {
    if (encoder->v[p]=='{') return 0;
    if (encoder->v[p]=='[') return 0;
    if (encoder->v[p]==',') return 0;
    if ((unsigned char)encoder->v[p]<=0x20) continue;
    if (sr_encode_u8(encoder,',')<0) return encoder->jsonctx=-1;
    return 0;
  }
  return 0;
}

int sr_encode_json_setup(struct sr_encoder *encoder,const char *k,int kc) {
  if (encoder->jsonctx<0) return -1;
  
  if (encoder->jsonctx=='{') {
    if (!k) return encoder->jsonctx=-1;
    if (sr_encoder_json_comma_if_needed(encoder)<0) return -1;
    while (1) {
      int err=sr_string_repr_json(encoder->v+encoder->c,encoder->a-encoder->c,k,kc);
      if (err<0) return encoder->jsonctx=-1;
      if (encoder->c<=encoder->a-err) {
        encoder->c+=err;
        break;
      }
      if (sr_encoder_require(encoder,err)<0) return encoder->jsonctx=-1;
    }
    if (sr_encode_u8(encoder,':')<0) return encoder->jsonctx=-1;
    return 0;
  }

  if (encoder->jsonctx=='[') {
    if (k) return encoder->jsonctx=-1;
    if (sr_encoder_json_comma_if_needed(encoder)<0) return -1;
    return 0;
  }

  if (k) return encoder->jsonctx=-1;
  return 0;
}
