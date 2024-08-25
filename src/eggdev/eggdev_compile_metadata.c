#include "eggdev_internal.h"

/* Detect non kosher text.
 */
 
static int eggdev_string_contains_nong0(const char *src,int srcc,int allow_space) {
  char lo=allow_space?0x20:0x21;
  for (;srcc-->0;src++) {
    if ((*src)<lo) return 1;
    if ((*src)>0x7e) return 1;
  }
  return 0;
}

/* Binary from text.
 */
 
static int eggdev_metadata_bin_from_text(struct sr_encoder *dst,const char *src,int srcc,const char *refname) {
  if (sr_encode_raw(dst,"\0EM\xff",4)<0) return -1;
  struct sr_decoder decoder={.v=src,.c=srcc};
  const char *line;
  int linec,lineno=1;
  for (;(linec=sr_decode_line(&line,&decoder))>0;lineno++) {
    while (linec&&((unsigned char)line[linec-1]<=0x20)) linec--;
    while (linec&&((unsigned char)line[0]<=0x20)) { linec--; line++; }
    if (!linec||(line[0]=='#')) continue;
    
    const char *k=line;
    int kc=0,linep=0;
    while ((linep<linec)&&((unsigned char)line[linep++]!='=')) kc++;
    while (kc&&((unsigned char)k[kc-1]<=0x20)) kc--;
    while ((linep<linec)&&((unsigned char)line[linep]<=0x20)) linep++;
    const char *v=line+linep;
    int vc=linec-linep;
    
    if (!kc) {
      fprintf(stderr,"%s:%d: Invalid empty key\n",refname,lineno);
      return -2;
    }
    if (kc>0xff) {
      fprintf(stderr,"%s:%d: Key too long (%d, limit 255)\n",refname,lineno,kc);
      return -2;
    }
    if (eggdev_string_contains_nong0(k,kc,0)) {
      fprintf(stderr,"%s:%d: Illegal byte in key. Must be ASCII G0 only, and no spaces.\n",refname,lineno);
      return -2;
    }
    if (vc>0xff) {
      fprintf(stderr,"%s:%d: Value too long (%d, limit 255)\n",refname,lineno,kc);
      return -2;
    }
    if (eggdev_string_contains_nong0(v,vc,1)) {
      fprintf(stderr,"%s:%d: Illegal byte in value. Must be ASCII G0 only.\n",refname,lineno);
      return -2;
    }
    
    if (sr_encode_u8(dst,kc)<0) return -1;
    if (sr_encode_u8(dst,vc)<0) return -1;
    if (sr_encode_raw(dst,k,kc)<0) return -1;
    if (sr_encode_raw(dst,v,vc)<0) return -1;
  }
  if (sr_encode_u8(dst,0)<0) return -1;
  return 0;
}

/* Text from binary.
 */
 
static int eggdev_metadata_text_from_bin(struct sr_encoder *dst,const uint8_t *src,int srcc,const char *refname) {
  if ((srcc<4)||memcmp(src,"\0EM\xff",4)) return -1;
  int srcp=4;
  while (srcp<srcc) {
    int kc=src[srcp++];
    if (!kc) break;
    if (srcp>=srcc) return -1;
    int vc=src[srcp++];
    if (srcp>srcc-vc-kc) return -1;
    const char *k=(char*)(src+srcp); srcp+=kc;
    const char *v=(char*)(src+srcp); srcp+=vc;
    if (sr_encode_fmt(dst,"%.*s=%.*s\n",kc,k,vc,v)<0) return -1;
  }
  return 0;
}

/* Main entry points.
 */
 
int eggdev_compile_metadata(struct eggdev_res *res) {

  // Already in binary format? Perfect, keep it.
  if ((res->serialc>=4)&&!memcmp(res->serial,"\0EM\xff",4)) return 0;
  
  // Assume anything else is the "k=v" text format.
  struct sr_encoder dst={0};
  int err=eggdev_metadata_bin_from_text(&dst,res->serial,res->serialc,res->path);
  if (err<0) {
    sr_encoder_cleanup(&dst);
    return err;
  }
  eggdev_res_handoff_serial(res,dst.v,dst.c);
  return 0;
}

int eggdev_uncompile_metadata(struct eggdev_res *res) {

  // Binary format? Uncompile.
  if ((res->serialc>=4)&&!memcmp(res->serial,"\0EM\xff",4)) {
    struct sr_encoder dst={0};
    int err=eggdev_metadata_text_from_bin(&dst,res->serial,res->serialc,"");
    if (err<0) {
      sr_encoder_cleanup(&dst);
      return err;
    }
    eggdev_res_handoff_serial(res,dst.v,dst.c);
    return 0;
  }
  
  // Assume anything else is already in the desired text format.
  return 0;
}

/* Extra helper: Get field from binary-format metadata.
 */
 
int eggdev_metadata_get(void *dstpp,const void *src,int srcc,const char *k,int kc) {
  if (!src||(srcc<4)||memcmp(src,"\0EM\xff",4)) return 0;
  if (!k) return 0;
  if (kc<0) { kc=0; while (k[kc]) kc++; }
  const uint8_t *SRC=src;
  int srcp=4;
  while (srcp<srcc) {
    int qkc=SRC[srcp++];
    if (!qkc) break;
    if (srcp>=srcc) break;
    int qvc=SRC[srcp++];
    if (srcp>srcc-qvc-qkc) break;
    if ((qkc==kc)&&!memcmp(SRC+srcp,k,kc)) {
      srcp+=kc;
      *(const void**)dstpp=SRC+srcp;
      return qvc;
    }
    srcp+=qkc;
    srcp+=qvc;
  }
  return 0;
}
