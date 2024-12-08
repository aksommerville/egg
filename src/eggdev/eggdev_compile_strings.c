#include "eggdev_internal.h"

/* Binary from text.
 */
 
int eggdev_strings_bin_from_text(struct sr_encoder *dst,const char *src,int srcc,const char *refname) {
  if (sr_encode_raw(dst,"\0ES\xff",4)<0) return -1;
  struct sr_decoder decoder={.v=src,.c=srcc};
  const char *line;
  int linec,lineno=1,index=0;
  for (;(linec=sr_decode_line(&line,&decoder))>0;lineno++) {
    while (linec&&((unsigned char)line[linec-1]<=0x20)) linec--;
    while (linec&&((unsigned char)line[0]<=0x20)) { linec--; line++; }
    if (!linec||(line[0]=='#')) continue;
    
    int linep=0;
    const char *ixtoken=line;
    int ixtokenc=0;
    while ((linep<linec)&&((unsigned char)line[linep++]>0x20)) ixtokenc++;
    while ((linep<linec)&&((unsigned char)line[linep]<=0x20)) linep++;
    int lindex;
    if (sr_int_eval(&lindex,ixtoken,ixtokenc)<2) {
      fprintf(stderr,"%s:%d: Expected index, found '%.*s'\n",refname,lineno,ixtokenc,ixtoken);
      return -2;
    }
    if (lindex<index) {
      fprintf(stderr,"%s:%d: Index %d out of order, expected at least %d\n",refname,lineno,lindex,index);
      return -2;
    }
    if (lindex>32767) {
      // This isn't a technical requirement, more of a sanity check. You don't need 32000 strings.
      // (or if you do, split them across multiple resources).
      fprintf(stderr,"%s:%d: String index must be 32767 or lower.\n",refname,lineno);
      return -2;
    }
    
    while (index<lindex) {
      if (sr_encode_u8(dst,0)<0) return -1;
      index++;
    }
    
    const char *v=line+linep;
    int vc=linec-linep;
    if ((vc>=1)&&(v[0]=='"')) {
      // Evaluate JSON string, then emit.
      // We will always use 2-byte length for quoted strings, to avoid the hassle of shuffling after evaluation.
      int lenp=dst->c;
      if (sr_encode_raw(dst,"\0\0",2)<0) return -1;
      for (;;) {
        int err=sr_string_eval((char*)dst->v+dst->c,dst->a-dst->c,v,vc);
        if (err<0) {
          fprintf(stderr,"%s:%d: Malformed string token.\n",refname,lineno);
          return -2;
        }
        if (err>0x7fff) {
          fprintf(stderr,"%s:%d: String length %d exceeds limit of 32767\n",refname,lineno,err);
          return -2;
        }
        if (dst->c<=dst->a-err) {
          dst->c+=err;
          ((uint8_t*)(dst->v))[lenp]=0x80|(err>>8);
          ((uint8_t*)(dst->v))[lenp+1]=err;
          break;
        }
        if (sr_encoder_require(dst,err)<0) return -1;
      }
      
    // Plain text is simpler:
    } else {
      if (vc>0x7fff) {
        fprintf(stderr,"%s:%d: String length %d exceeds limit of 32767\n",refname,lineno,vc);
        return -2;
      }
      if (vc>=0x80) {
        if (sr_encode_intbe(dst,0x8000|vc,2)<0) return -1;
      } else {
        if (sr_encode_u8(dst,vc)<0) return -1;
      }
      if (sr_encode_raw(dst,v,vc)<0) return -1;
    }
    
    index++;
  }
  return 0;
}

/* Do we need to quote it? Or can we just dump it raw?
 */
 
static int eggdev_string_requires_quotes(const char *src,int srcc) {
  // Empty is fine raw.
  if (srcc<1) return 0;
  // Leading or trailing whitespace must quote.
  if ((unsigned char)src[0]<=0x20) return 1;
  if ((unsigned char)src[srcc-1]<=0x20) return 1;
  // Leading quote must quote.
  if (src[0]=='"') return 1;
  // Any C0 byte must quote. Assume that high code points are kosher.
  for (;srcc-->0;src++) {
    if ((unsigned char)(*src)<0x20) return 1;
  }
  return 0;
}

/* Text from binary.
 */
 
int eggdev_strings_text_from_bin(struct sr_encoder *dst,const uint8_t *src,int srcc,const char *refname) {
  if ((srcc<4)||memcmp(src,"\0ES\xff",4)) return -1;
  int srcp=4,index=0;
  while (srcp<srcc) {
    int len=src[srcp++];
    if (len&0x80) {
      if (srcp>=srcc) return -1;
      len=(len&0x7f)<<8;
      len|=src[srcp++];
    }
    if (len) {
      if (srcp>srcc-len) return -1;
      if (sr_encode_fmt(dst,"%d ",index)<0) return -1;
      if (eggdev_string_requires_quotes((char*)(src+srcp),len)) {
        if (sr_encode_json_string(dst,0,0,(char*)(src+srcp),len)<0) return -1;
      } else {
        if (sr_encode_raw(dst,src+srcp,len)<0) return -1;
      }
      if (sr_encode_u8(dst,0x0a)<0) return -1;
      srcp+=len;
    }
    index++;
  }
  return 0;
}

/* Main entry points.
 */
 
int eggdev_compile_strings(struct eggdev_res *res) {
  if ((res->serialc>=4)&&!memcmp(res->serial,"\0ES\xff",4)) return 0;
  struct sr_encoder dst={0};
  int err=eggdev_strings_bin_from_text(&dst,res->serial,res->serialc,res->path);
  if (err<0) {
    sr_encoder_cleanup(&dst);
    return err;
  }
  eggdev_res_handoff_serial(res,dst.v,dst.c);
  return 0;
}

int eggdev_uncompile_strings(struct eggdev_res *res) {
  if ((res->serialc>=4)&&!memcmp(res->serial,"\0ES\xff",4)) {
    struct sr_encoder dst={0};
    int err=eggdev_strings_text_from_bin(&dst,res->serial,res->serialc,"");
    if (err<0) {
      sr_encoder_cleanup(&dst);
      return err;
    }
    eggdev_res_handoff_serial(res,dst.v,dst.c);
    return 0;
  }
  return 0;
}
