#include "serial.h"
#include <stdint.h>
#include <limits.h>
#include <string.h>
#include <math.h>

#define SRC ((uint8_t*)(decoder->v)+decoder->p)

/* Decode one line of text.
 */

int sr_decode_line(void *dstpp,struct sr_decoder *decoder) {
  const char *src=(char*)SRC;
  int srcc=decoder->c-decoder->p;
  int srcp=0;
  while ((srcp<srcc)&&(src[srcp++]!=0x0a)) ;
  decoder->p+=srcp;
  if (dstpp) *(const void**)dstpp=src;
  return srcp;
}

/* Decode binary integers.
 */

int sr_decode_u8(struct sr_decoder *decoder) {
  if (decoder->p>=decoder->c) return -1;
  return ((uint8_t*)decoder->v)[decoder->p++];
}

int sr_decode_intbe(int *v,struct sr_decoder *decoder,int size_bytes) {
  int err=sr_intbe_decode(v,SRC,decoder->c-decoder->p,size_bytes);
  if (err<1) return -1;
  decoder->p+=err;
  return err;
}

int sr_decode_intle(int *v,struct sr_decoder *decoder,int size_bytes) {
  int err=sr_intle_decode(v,SRC,decoder->c-decoder->p,size_bytes);
  if (err<1) return -1;
  decoder->p+=err;
  return err;
}

int sr_decode_fixed(double *v,struct sr_decoder *decoder,int sign,int wholesize_bits,int fractsize_bits) {
  int err=sr_fixed_decode(v,SRC,decoder->c-decoder->p,sign,wholesize_bits,fractsize_bits);
  if (err<1) return -1;
  decoder->p+=err;
  return err;
}

int sr_decode_utf8(int *v,struct sr_decoder *decoder) {
  int err=sr_utf8_decode(v,SRC,decoder->c-decoder->p);
  if (err<1) return -1;
  decoder->p+=err;
  return err;
}

int sr_decode_vlq(int *v,struct sr_decoder *decoder) {
  int err=sr_vlq_decode(v,SRC,decoder->c-decoder->p);
  if (err<1) return -1;
  decoder->p+=err;
  return err;
}

/* Decode chunks of raw data.
 */

int sr_decode_raw(void *dstpp,struct sr_decoder *decoder,int len) {
  if (len<0) return -1;
  if (decoder->p>decoder->c-len) return -1;
  if (dstpp) *(void**)dstpp=SRC;
  decoder->p+=len;
  return len;
}

int sr_decode_intbelen(void *dstpp,struct sr_decoder *decoder,int lensize_bytes) {
  int p0=decoder->p;
  int len;
  if (sr_decode_intbe(&len,decoder,lensize_bytes)<0) return -1;
  if (sr_decode_raw(dstpp,decoder,len)<0) { decoder->p=p0; return -1; }
  return len;
}

int sr_decode_intlelen(void *dstpp,struct sr_decoder *decoder,int lensize_bytes) {
  int p0=decoder->p;
  int len;
  if (sr_decode_intle(&len,decoder,lensize_bytes)<0) return -1;
  if (sr_decode_raw(dstpp,decoder,len)<0) { decoder->p=p0; return -1; }
  return len;
}

int sr_decode_vlqlen(void *dstpp,struct sr_decoder *decoder) {
  int p0=decoder->p;
  int len;
  if (sr_decode_vlq(&len,decoder)<0) return -1;
  if (sr_decode_raw(dstpp,decoder,len)<0) { decoder->p=p0; return -1; }
  return len;
}

/* Measure JSON expression recursively.
 */
 
int sr_json_measure(const char *src,int srcc) {
  if (!src) return 0;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  if (srcc<1) return 0;

  // Since we don't promise validation, we can cheat a little and treat objects and arrays the same.
  // All kinds of garbage could pass through here, but valid objects and arrays will definitely measure correctly and that's all we care about.
  if ((src[0]=='{')||(src[0]=='[')) {
    char closer=(src[0]=='{')?'}':']';
    int srcp=1;
    while (1) {
      if (srcp>=srcc) return -1;
      if (src[srcp]==closer) return srcp+1;
      if ((unsigned char)src[srcp]<=0x20) { srcp++; continue; }
      if (src[srcp]==',') { srcp++; continue; }
      if (src[srcp]==':') { srcp++; continue; }
      int err=sr_json_measure(src+srcp,srcc-srcp);
      if (err<1) return -1;
      srcp+=err;
    }
  }

  if (src[0]=='"') return sr_string_measure(src,srcc,0);

  // Numbers and identifiers.
  // We don't guarantee to measure a *valid* token. Just make sure there's no spaces or fences.
  int srcp=0;
  while (srcp<srcc) {
         if ((src[srcp]>='0')&&(src[srcp]<='9')) srcp++;
    else if ((src[srcp]>='a')&&(src[srcp]<='z')) srcp++;
    else if ((src[srcp]>='A')&&(src[srcp]<='Z')) srcp++;
    else if (src[srcp]=='_') srcp++;
    else if (src[srcp]=='-') srcp++;
    else if (src[srcp]=='+') srcp++;
    else if (src[srcp]=='.') srcp++;
    else break;
  }
  return srcp;
}

/* Peek at the next JSON expression.
 */

char sr_decode_json_peek(const struct sr_decoder *decoder) {
  if (decoder->jsonctx<0) return 0;
  const char *src=(char*)SRC;
  int srcc=decoder->c-decoder->p;
  while (srcc&&((unsigned char)(*src)<=0x20)) { srcc--; src++; }
  if (srcc<1) return 0;
  switch (*src) {
    case '{': case '[': case '"': case 'n': case 't': case 'f': return *src;
  }
  if ((*src>='0')&&(*src<='9')) return '#';
  if (*src=='-') return '#';
  if (*src=='+') return '#';
  return 0;
}

static void sr_decoder_skip_json_space(struct sr_decoder *decoder) {
  while (decoder->p<decoder->c) {
    if ((unsigned char)(*SRC)<=0x20) decoder->p++;
    else return;
  }
}

/* Read a full JSON expression (possibly a collection).
 */
 
int sr_decode_json_expression(void *dstpp,struct sr_decoder *decoder) {
  if (decoder->jsonctx<0) return -1;
  sr_decoder_skip_json_space(decoder);
  if (decoder->p>=decoder->c) return decoder->jsonctx=-1;
  const char *token=(char*)SRC;
  int tokenc=sr_json_measure(token,decoder->c-decoder->p);
  if (tokenc<1) return decoder->jsonctx=-1;
  decoder->p+=tokenc;
  if (dstpp) *(const void**)dstpp=token;
  return tokenc;
}
 
int sr_decode_json_skip(struct sr_decoder *decoder) {
  return sr_decode_json_expression(0,decoder);
}

/* Decode JSON expression as string.
 * Do not advance read head if the output buffer is short.
 */
 
int sr_decode_json_string(char *dst,int dsta,struct sr_decoder *decoder) {
  if (!dst||(dsta<0)) dsta=0;
  int p0=decoder->p;
  const char *src=0;
  int srcc=sr_decode_json_expression(&src,decoder);
  if (srcc<0) return -1;

  // If it is a string token, evaluate it.
  if ((srcc>=2)&&(src[0]=='"')) {
    int dstc=sr_string_eval(dst,dsta,src,srcc);
    if (dstc<0) return decoder->jsonctx=-1;
    if (dstc<=dsta) return dstc;
    decoder->p=p0;
    return dstc;
  }

  // If it is not a string token, copy it verbatim.
  if (srcc<=dsta) {
    memcpy(dst,src,srcc);
    if (srcc<dsta) dst[srcc]=0;
  } else {
    decoder->p=p0;
  }
  return srcc;
}

int sr_decode_json_string_to_encoder(struct sr_encoder *dst,struct sr_decoder *decoder) {
  while (1) {
    int err=sr_decode_json_string(dst->v+dst->c,dst->a-dst->c,decoder);
    if (err<0) return err;
    if (dst->c<=dst->a-err) {
      dst->c+=err;
      return err;
    }
    if (sr_encoder_require(dst,err)<0) return -1;
  }
}

/* Contextless JSON primitives.
 */
 
int sr_int_from_json_expression(int *dst,const char *src,int srcc) {
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  if (!srcc) { *dst=0; return 0; }
  if (sr_int_eval(dst,src,srcc)>=0) return 0;
  double d;
  if (sr_double_eval(&d,src,srcc)>=0) {
    if (d<=INT_MIN) { *dst=INT_MIN; return 0; }
    if (d>=INT_MAX) { *dst=INT_MAX; return 0; }
    *dst=d;
    return 0;
  }
  if ((srcc>=2)&&(src[0]=='"')) {
    char tmp[32];
    int tmpc=sr_string_eval(tmp,sizeof(tmp),src,srcc);
    if ((tmpc>=0)&&(tmpc<=sizeof(tmp))) {
      return sr_int_from_json_expression(dst,tmp,tmpc);
    }
  }
  if ((srcc==4)&&!memcmp(src,"null",4)) { *dst=0; return 0; }
  if ((srcc==5)&&!memcmp(src,"false",5)) { *dst=0; return 0; }
  *dst=1;
  return 0;
}

int sr_double_from_json_expression(double *dst,const char *src,int srcc) {
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  if (sr_double_eval(dst,src,srcc)>=0) return 0;
  int i;
  if (sr_int_eval(&i,src,srcc)>=0) { *dst=i; return 0; }
  if ((srcc>=2)&&(src[0]=='"')) {
    char tmp[32];
    int tmpc=sr_string_eval(tmp,sizeof(tmp),src,srcc);
    if ((tmpc>=0)&&(tmpc<=sizeof(tmp))) {
      return sr_double_from_json_expression(dst,tmp,tmpc);
    }
  }
  if ((srcc==4)&&!memcmp(src,"true",4)) { *dst=1.0; return 0; }
  if ((srcc==5)&&!memcmp(src,"false",5)) { *dst=0.0; return 0; }
  *dst=NAN;
  return 0;
}

int sr_boolean_from_json_expression(const char *src,int srcc) {
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  if (!srcc) return 0;
  if ((srcc==4)&&!memcmp(src,"true",4)) return 1;
  if ((srcc==5)&&!memcmp(src,"false",5)) return 0;
  if ((srcc==4)&&!memcmp(src,"null",4)) return 0;
  int i;
  if (sr_int_eval(&i,src,srcc)>=0) return i?1:0;
  double d;
  if (sr_double_eval(&d,src,srcc)>=0) switch (fpclassify(d)) {
    case FP_ZERO: case FP_NAN: return 0;
    default: return 1;
  }
  if ((srcc>=2)&&(src[0]=='"')) {
    char tmp[32];
    int tmpc=sr_string_eval(tmp,sizeof(tmp),src,srcc);
    if ((tmpc>=0)&&(tmpc<=sizeof(tmp))) {
      return sr_boolean_from_json_expression(tmp,tmpc);
    }
  }
  return 1;
}

/* Decode JSON expression as number.
 */
 
int sr_decode_json_int(int *dst,struct sr_decoder *decoder) {
  const char *src=0;
  int srcc=sr_decode_json_expression(&src,decoder);
  if (srcc<0) return -1;
  return sr_int_from_json_expression(dst,src,srcc);
}

int sr_decode_json_double(double *dst,struct sr_decoder *decoder) {
  const char *src=0;
  int srcc=sr_decode_json_expression(&src,decoder);
  if (srcc<0) return -1;
  return sr_double_from_json_expression(dst,src,srcc);
}

int sr_decode_json_boolean(struct sr_decoder *decoder) {
  const char *src=0;
  int srcc=sr_decode_json_expression(&src,decoder);
  if (srcc<0) return -1;
  return sr_boolean_from_json_expression(src,srcc);
}

/* JSON collection fences.
 */

int sr_decode_json_object_start(struct sr_decoder *decoder) {
  if (decoder->jsonctx<0) return -1;
  sr_decoder_skip_json_space(decoder);
  if (decoder->p>=decoder->c) return decoder->jsonctx=-1;
  if (*SRC!='{') return decoder->jsonctx=-1;
  decoder->p++;
  int jsonctx=decoder->jsonctx;
  decoder->jsonctx='{';
  return jsonctx;
}

int sr_decode_json_array_start(struct sr_decoder *decoder) {
  if (decoder->jsonctx<0) return -1;
  sr_decoder_skip_json_space(decoder);
  if (decoder->p>=decoder->c) return decoder->jsonctx=-1;
  if (*SRC!='[') return decoder->jsonctx=-1;
  decoder->p++;
  int jsonctx=decoder->jsonctx;
  decoder->jsonctx='[';
  return jsonctx;
}

int sr_decode_json_end(struct sr_decoder *decoder,int jsonctx) {
  switch (decoder->jsonctx) {
    case '[': {
        sr_decoder_skip_json_space(decoder);
        if (decoder->p>=decoder->c) return decoder->jsonctx=-1;
        if (*SRC!=']') return decoder->jsonctx=-1;
        decoder->p++;
        decoder->jsonctx=jsonctx;
        return 0;
      }
    case '{': {
        sr_decoder_skip_json_space(decoder);
        if (decoder->p>=decoder->c) return decoder->jsonctx=-1;
        if (*SRC!='}') return decoder->jsonctx=-1;
        decoder->p++;
        decoder->jsonctx=jsonctx;
        return 0;
      }
  }
  return -1;
}

/* Next member of JSON collection.
 */
 
int sr_decode_json_next(void *kpp,struct sr_decoder *decoder) {
  switch (decoder->jsonctx) {
    case '[': {
        if (kpp) return decoder->jsonctx=-1;
        while (1) {
          sr_decoder_skip_json_space(decoder);
          if (decoder->p>=decoder->c) return decoder->jsonctx=-1;
          if (*SRC==',') { decoder->p++; continue; }
          if (*SRC==']') return 0;
          return 1;
        }
      }
    case '{': {
        if (!kpp) return decoder->jsonctx=-1;
        while (1) {
          sr_decoder_skip_json_space(decoder);
          if (decoder->p>=decoder->c) return decoder->jsonctx=-1;
          if (*SRC==',') { decoder->p++; continue; }
          if (*SRC=='}') return 0;
          break;
        }
        int simple=0,kc;
        int tokenc=sr_string_measure((char*)SRC,decoder->c-decoder->p,&simple);
        if (tokenc<1) return decoder->jsonctx=-1;
        if (simple&&(tokenc>2)) {
          *(const void**)kpp=SRC+1;
          kc=tokenc-2;
        } else {
          *(const void**)kpp=SRC;
          kc=tokenc;
        }
        decoder->p+=tokenc;
        sr_decoder_skip_json_space(decoder);
        if (decoder->p>=decoder->c) return decoder->jsonctx=-1;
        if (*SRC!=':') return decoder->jsonctx=-1;
        decoder->p++;
        return kc;
      }
  }
  return -1;
}

/* JSON array and object, high-level conveniences.
 */
 
int sr_for_each_of_json_array(
  const char *src,int srcc,
  int (*cb)(const char *v,int vc,void *userdata),
  void *userdata
) {
  if (!src) return -1;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  struct sr_decoder decoder={.v=src,.c=srcc};
  if (sr_decode_json_array_start(&decoder)<0) return -1;
  const char *v;
  int vc,err;
  while (sr_decode_json_next(0,&decoder)>0) {
    if ((vc=sr_decode_json_expression(&v,&decoder))<0) return -1;
    if (err=cb(v,vc,userdata)) return err;
  }
  return sr_decode_json_end(&decoder,0);
}

int sr_for_each_of_json_object(
  const char *src,int srcc,
  int (*cb)(const char *k,int kc,const char *v,int vc,void *userdata),
  void *userdata
) {
  if (!src) return -1;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  struct sr_decoder decoder={.v=src,.c=srcc};
  if (sr_decode_json_object_start(&decoder)<0) return -1;
  const char *k,*v;
  int kc,vc,err;
  while ((kc=sr_decode_json_next(&k,&decoder))>0) {
    if ((vc=sr_decode_json_expression(&v,&decoder))<0) return -1;
    if (err=cb(k,kc,v,vc,userdata)) return err;
  }
  return sr_decode_json_end(&decoder,0);
}
