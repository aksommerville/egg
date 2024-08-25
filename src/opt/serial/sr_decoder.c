#include "serial.h"
#include <string.h>
#include <limits.h>
#include <stdint.h>

#define DECV ((const uint8_t*)decoder->v)

/* Line of text.
 */
 
int sr_decode_line(const char **vpp,struct sr_decoder *decoder) {
  if (decoder->p>=decoder->c) return 0;
  *vpp=DECV+decoder->p;
  int linec=0;
  while (decoder->p<decoder->c) {
    if (DECV[decoder->p]==0x0d) {
      linec++;
      decoder->p++;
      if ((decoder->p<decoder->c)&&(DECV[decoder->p]==0x0a)) {
        decoder->p++;
        linec++;
      }
      return linec;
    }
    linec++;
    if (DECV[decoder->p++]==0x0a) return linec;
  }
  return linec;
}

/* Scalars.
 */

int sr_decode_u8(struct sr_decoder *decoder) {
  if (decoder->p>=decoder->c) return -1;
  return DECV[decoder->p++];
}

int sr_decode_intbe(int *v,struct sr_decoder *decoder,int len) {
  if (decoder->p>decoder->c-len) return -1;
  const uint8_t *SRC=DECV+decoder->p;
  switch (len) {
    case 1: *v=SRC[0]; break;
    case 2: *v=(SRC[0]<<8)|SRC[1]; break;
    case 3: *v=(SRC[0]<<16)|(SRC[1]<<8)|SRC[2]; break;
    case 4: *v=(SRC[0]<<24)|(SRC[1]<<16)|(SRC[2]<<8)|SRC[3]; break;
    default: return -1;
  }
  decoder->p+=len;
  return 0;
}

int sr_decode_intle(int *v,struct sr_decoder *decoder,int len) {
  if (decoder->p>decoder->c-len) return -1;
  const uint8_t *SRC=DECV+decoder->p;
  switch (len) {
    case 1: *v=SRC[0]; break;
    case 2: *v=SRC[0]|(SRC[1]<<8); break;
    case 3: *v=SRC[0]|(SRC[1]<<8)|(SRC[2]<<16); break;
    case 4: *v=SRC[0]|(SRC[1]<<8)|(SRC[2]<<16)|(SRC[3]<<24); break;
    default: return -1;
  }
  decoder->p+=len;
  return 0;
}

int sr_decode_vlq(int *v,struct sr_decoder *decoder) {
  int err=sr_vlq_decode(v,DECV+decoder->p,decoder->c-decoder->p);
  if (err<1) return -1;
  decoder->p+=err;
  return 0;
}

/* Chunks of raw data, optional leading length.
 */

int sr_decode_raw(void *dstpp,struct sr_decoder *decoder,int len) {
  if (len<0) return -1;
  if (decoder->p>decoder->c-len) return -1;
  *(const void**)dstpp=DECV+decoder->p;
  decoder->p+=len;
  return len;
}

int sr_decode_intbelen(void *dstpp,struct sr_decoder *decoder,int lenlen) {
  int len;
  if (sr_decode_intbe(&len,decoder,lenlen)<0) return -1;
  return sr_decode_raw(dstpp,decoder,len);
}

int sr_decode_intlelen(void *dstpp,struct sr_decoder *decoder,int lenlen) {
  int len;
  if (sr_decode_intle(&len,decoder,lenlen)<0) return -1;
  return sr_decode_raw(dstpp,decoder,len);
}

int sr_decode_vlqlen(void *dstpp,struct sr_decoder *decoder) {
  int len;
  if (sr_decode_vlq(&len,decoder)<0) return -1;
  return sr_decode_raw(dstpp,decoder,len);
}

/* JSON assert completion.
 */

int sr_decode_json_done(const struct sr_decoder *decoder) {
  if (decoder->jsonctx) return -1;
  return 0;
}

/* JSON structure fences.
 */
 
int sr_decode_json_object_start(struct sr_decoder *decoder) {
  if (decoder->jsonctx<0) return -1;
  while ((decoder->p<decoder->c)&&(DECV[decoder->p]<=0x20)) decoder->p++;
  if (decoder->p>=decoder->c) return decoder->jsonctx=-1;
  if (DECV[decoder->p]!='{') return decoder->jsonctx=-1;
  decoder->p++;
  int pvctx=decoder->jsonctx;
  decoder->jsonctx='{';
  return pvctx;
}

int sr_decode_json_array_start(struct sr_decoder *decoder) {
  if (decoder->jsonctx<0) return -1;
  while ((decoder->p<decoder->c)&&(DECV[decoder->p]<=0x20)) decoder->p++;
  if (decoder->p>=decoder->c) return decoder->jsonctx=-1;
  if (DECV[decoder->p]!='[') return decoder->jsonctx=-1;
  decoder->p++;
  int pvctx=decoder->jsonctx;
  decoder->jsonctx='[';
  return pvctx;
}

int sr_decode_json_end(struct sr_decoder *decoder,int jsonctx) {
  if (decoder->jsonctx<0) return -1;
  while ((decoder->p<decoder->c)&&(DECV[decoder->p]<=0x20)) decoder->p++;
  if (decoder->p>=decoder->c) return decoder->jsonctx=-1;
  switch (decoder->jsonctx) {
    case '[': if (DECV[decoder->p]!=']') return decoder->jsonctx=-1; break;
    case '{': if (DECV[decoder->p]!='}') return decoder->jsonctx=-1; break;
    default: return decoder->jsonctx=-1;
  }
  decoder->p++;
  decoder->jsonctx=jsonctx;
  return 0;
}

/* JSON structure next.
 * We allow extraneous commas, because it's easier than enforcing the exact rules.
 */

int sr_decode_json_next(const char **kpp,struct sr_decoder *decoder) {
  switch (decoder->jsonctx) {
  
    case '{': {
        if (!kpp) return -1;
        while ((decoder->p<decoder->c)&&((DECV[decoder->p]<=0x20)||(DECV[decoder->p]==','))) decoder->p++;
        if (decoder->p>=decoder->c) return decoder->jsonctx=-1;
        if (DECV[decoder->p]=='}') return 0;
        int ksimple=1,retval;
        int ktokenc=sr_string_measure(DECV+decoder->p,decoder->c-decoder->p,&ksimple);
        if (ktokenc<1) return decoder->jsonctx=-1;
        if (ksimple&&(ktokenc>2)) { // normal cases -- return what's inside the key token
          *kpp=DECV+decoder->p+1;
          retval=ktokenc-2;
        } else { // empty or escape -- return the encoded key token
          *kpp=DECV+decoder->p;
          retval=ktokenc;
        }
        decoder->p+=ktokenc;
        while ((decoder->p<decoder->c)&&(DECV[decoder->p]<=0x20)) decoder->p++;
        if ((decoder->p>=decoder->c)||(DECV[decoder->p++]!=':')) return decoder->jsonctx=-1;
        return retval;
      }
      
    case '[': {
        if (kpp) return -1;
        while ((decoder->p<decoder->c)&&((DECV[decoder->p]<=0x20)||(DECV[decoder->p]==','))) decoder->p++;
        if (decoder->p>=decoder->c) return decoder->jsonctx=-1;
        if (DECV[decoder->p]==']') return 0;
        return 1;
      }
      
    default: return decoder->jsonctx=-1;
  }
}

/* Peek at the next JSON expression.
 */

char sr_decode_json_peek(const struct sr_decoder *decoder) {
  if (decoder->jsonctx<0) return 0;
  int p=decoder->p;
  while ((p<decoder->c)&&((DECV[p]<=0x20)||(DECV[p]==','))) p++;
  if (p>=decoder->c) return 0;
  char ch0=DECV[p];
  if ((ch0>='a')&&(ch0<='z')) {
    if ((p<=decoder->c-4)&&!memcmp(DECV+p,"null",4)) return 'n';
    if ((p<=decoder->c-4)&&!memcmp(DECV+p,"true",4)) return 't';
    if ((p<=decoder->c-5)&&!memcmp(DECV+p,"false",5)) return 'f';
    return 0;
  }
  if ((ch0>='0')&&(ch0<='9')) return '0';
  if (ch0=='"') return '"';
  if (ch0=='{') return '{';
  if (ch0=='[') return '[';
  return 0;
}

/* Extract JSON expression verbatim.
 */

int sr_decode_json_expression(const char **dstpp,struct sr_decoder *decoder) {
  if (decoder->jsonctx<0) return -1;
  while ((decoder->p<decoder->c)&&(DECV[decoder->p]<=0x20)) decoder->p++;
  if (decoder->p>=decoder->c) return decoder->jsonctx=-1;
  const char *expr=DECV+decoder->p;
  int exprc=sr_json_measure(expr,decoder->c-decoder->p);
  if (exprc<1) return decoder->jsonctx=-1;
  if (dstpp) *dstpp=expr;
  return exprc;
}

/* Decode JSON string.
 */

int sr_decode_json_string(char *dst,int dsta,struct sr_decoder *decoder) {
  int p0=decoder->p;
  const char *expr;
  int exprc=sr_decode_json_expression(&expr,decoder);
  if (exprc<1) return -1;
  // Evaluate strings:
  if (expr[0]=='"') {
    int err=sr_string_eval(dst,dsta,expr,exprc);
    if (err<0) return decoder->jsonctx=-1;
    if (err>dsta) decoder->p=p0;
    return err;
  }
  // Everything else, return the verbatim text:
  if (exprc>dsta) decoder->p=p0;
  else {
    memcpy(dst,expr,exprc);
    if (exprc<dsta) dst[exprc]=0;
  }
  return exprc;
}

int sr_decode_json_string_to_encoder(struct sr_encoder *dst,struct sr_decoder *decoder) {
  while (1) {
    int err=sr_decode_json_string((char*)dst->v+dst->c,dst->a-dst->c,decoder);
    if (err<0) return err;
    if (dst->c<=dst->a-err) {
      dst->c+=err;
      return 0;
    }
    if (sr_encoder_require(dst,err)<0) return -1;
  }
}

int sr_decode_json_base64(char *dst,int dsta,struct sr_decoder *decoder) {
  if (decoder->jsonctx<0) return -1;
  while ((decoder->p<decoder->c)&&(DECV[decoder->p]<=0x20)) decoder->p++;
  if (decoder->p>=decoder->c) return decoder->jsonctx=-1;
  if (DECV[decoder->p]!='"') return -1;
  int tokenc=sr_string_measure(DECV+decoder->p,decoder->c-decoder->p,0);
  if (tokenc<2) return decoder->jsonctx=-1;
  int dstc=sr_base64_decode(dst,dsta,DECV+decoder->p+1,tokenc-2);
  if (dstc<0) return decoder->jsonctx=-1;
  if (dstc<=dsta) {
    decoder->p+=tokenc;
    if (dstc<dsta) dst[dstc]=0;
  }
  return dstc;
}

int sr_decode_json_base64_to_encoder(struct sr_encoder *dst,struct sr_decoder *decoder) {
  if (decoder->jsonctx<0) return -1;
  while ((decoder->p<decoder->c)&&(DECV[decoder->p]<=0x20)) decoder->p++;
  if (decoder->p>=decoder->c) return decoder->jsonctx=-1;
  if (DECV[decoder->p]!='"') return -1;
  int tokenc=sr_string_measure(DECV+decoder->p,decoder->c-decoder->p,0);
  if (tokenc<2) return decoder->jsonctx=-1;
  while (1) {
    int err=sr_base64_decode((char*)dst->v+dst->c,dst->a-dst->c,DECV+decoder->p+1,tokenc-2);
    if (err<0) return decoder->jsonctx=-1;
    if (dst->c<=dst->a-err) {
      dst->c+=err;
      decoder->p+=tokenc;
      return 0;
    }
    if (sr_encoder_require(dst,err)<0) return -1;
  }
}

/* JSON numbers.
 */

int sr_decode_json_int(int *dst,struct sr_decoder *decoder) {
  const char *expr;
  int exprc=sr_decode_json_expression(&expr,decoder);
  if (exprc<1) return -1;
  
  char tmp[32];
  if (expr[0]=='"') {
    int tmpc=sr_string_eval(tmp,sizeof(tmp),expr,exprc);
    if ((tmpc<0)||(tmpc>sizeof(tmp))) return decoder->jsonctx=-1;
    expr=tmp;
    exprc=tmpc;
  }
  
  if (!exprc) { *dst=0; return 0; }
  if ((exprc==4)&&!memcmp(expr,"null",4)) { *dst=0; return 0; }
  if ((exprc==4)&&!memcmp(expr,"true",4)) { *dst=1; return 0; }
  if ((exprc==5)&&!memcmp(expr,"false",5)) { *dst=0; return 0; }
  
  int err=sr_int_eval(dst,expr,exprc);
  if (err>=0) return err;
  
  double d;
  if (sr_double_eval(&d,expr,exprc)>=0) {
    if (d>INT_MAX) { *dst=INT_MAX; return 0; }
    if (d<INT_MIN) { *dst=INT_MIN; return 0; }
    *dst=(int)d;
    return 2;
  }
  
  return decoder->jsonctx=-1;
}

int sr_decode_json_double(double *dst,struct sr_decoder *decoder) {
  const char *expr;
  int exprc=sr_decode_json_expression(&expr,decoder);
  if (exprc<1) return -1;
  
  char tmp[32];
  if (expr[0]=='"') {
    int tmpc=sr_string_eval(tmp,sizeof(tmp),expr,exprc);
    if ((tmpc<0)||(tmpc>sizeof(tmp))) return decoder->jsonctx=-1;
    expr=tmp;
    exprc=tmpc;
  }
  
  if (!exprc) { *dst=0.0; return 0; }
  if ((exprc==4)&&!memcmp(expr,"null",4)) { *dst=0.0; return 0; }
  if ((exprc==4)&&!memcmp(expr,"true",4)) { *dst=1.0; return 0; }
  if ((exprc==5)&&!memcmp(expr,"false",5)) { *dst=0.0; return 0; }
  
  if (sr_double_eval(dst,expr,exprc)>=0) return 0;
  
  int i;
  if (sr_int_eval(&i,expr,exprc)>=2) {
    *dst=(double)i;
    return 0;
  }
  
  return decoder->jsonctx=-1;
}

/* Measure any JSON expression.
 */

int sr_json_measure(const char *src,int srcc) {
  if (!src) return 0;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  int srcp=0;
  while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
  if (srcp>=srcc) return 0;
  
  if (src[srcp]=='"') {
    int err=sr_string_measure(src+srcp,srcc-srcp,0);
    if (err<=0) return 0;
    return srcp+err;
  }
  
  if ((src[srcp]=='+')||(src[srcp]=='-')||((src[srcp]>='0')&&(src[srcp]<='9'))) {
    int err=sr_number_measure(src+srcp,srcc-srcp,0);
    if (err<=0) return 0;
    return srcp+err;
  }
  
  // There are only 3 legal identifers, all composed of lowercase letters.
  if ((src[srcp]>='a')&&(src[srcp]<='z')) {
    srcp++;
    while ((srcp<srcc)&&(src[srcp]>='a')&&(src[srcp]<='z')) srcp++;
    return srcp;
  }
  
  if (src[srcp]=='{') {
    srcp++;
    while (1) {
      if (srcp>=srcc) return 0;
      if (src[srcp]=='}') return srcp+1;
      if ((unsigned char)src[srcp]<=0x20) { srcp++; continue; }
      if (src[srcp]==',') { srcp++; continue; }
      
      int err=sr_string_measure(src+srcp,srcc-srcp,0);
      if (err<1) return 0;
      srcp+=err;
      while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
      if ((srcp>=srcc)||(src[srcp++]!=':')) return 0;
      while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
      
      err=sr_json_measure(src+srcp,srcc-srcp);
      if (err<1) return 0;
      srcp+=err;
    }
  }
  
  if (src[srcp]=='[') {
    srcp++;
    while (1) {
      if (srcp>=srcc) return 0;
      if (src[srcp]==']') return srcp+1;
      if ((unsigned char)src[srcp]<=0x20) { srcp++; continue; }
      if (src[srcp]==',') { srcp++; continue; }
      int err=sr_json_measure(src+srcp,srcc-srcp);
      if (err<1) return 0;
      srcp+=err;
    }
  }
  
  return 0;
}
