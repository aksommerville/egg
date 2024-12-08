#include "eggdev_internal.h"
#include "opt/image/image.h"
#include "opt/synth/synth_formats.h"

/* metadata, single token from "required" or "optional".
 */
 
static int eggdev_validate_metadata_feature(
  const char *src,int srcc,
  const struct eggdev_res *res,const struct eggdev_rom *rom,const char *path
) {

  if ((srcc==2)&&!memcmp(src,"gl",2)) return 0;
  if ((srcc==8)&&!memcmp(src,"keyboard",8)) return 0;
  if ((srcc==5)&&!memcmp(src,"mouse",5)) return 0;
  if ((srcc==5)&&!memcmp(src,"touch",5)) return 0;
  if ((srcc==13)&&!memcmp(src,"accelerometer",13)) return 0;
  if ((srcc==7)&&!memcmp(src,"gamepad",7)) return 0;
  if ((srcc==5)&&!memcmp(src,"audio",5)) return 0;
  
  if ((srcc>8)&&!memcmp(src,"gamepad(",8)&&(src[srcc-1]==')')) {
    int bits;
    if (sr_int_eval(&bits,src+8,srcc-9)<2) {
      fprintf(stderr,"%s:metadata:%d: Expected integer in 'gamepad' tag: '%.*s'\n",path,res->rid,srcc,src);
      return -2;
    }
    if (bits&0xffc00001) {
      fprintf(stderr,"%s:metadata:%d:WARNING: Unexpected bits in 'gamepad' feature mask 0x%08x. See egg.h:EGG_BTN_*\n",path,res->rid,bits);
    }
    return 0;
  }
  
  fprintf(stderr,"%s:metadata:%d:WARNING: Unknown feature '%.*s'\n",path,res->rid,srcc,src);
  return 0;
}

/* metadata, single field.
 * Caller does the lexical stuff, duplicates, and anything that involves looking at other fields.
 * We manage per-key formatting rules and external resource references.
 */
 
static int eggdev_validate_metadata_field(
  const char *k,int kc,const char *v,int vc,
  const struct eggdev_res *res,const struct eggdev_rom *rom,const char *path
) {
  #define PLAININT(key,lo,hi) if ((kc==sizeof(key)-1)&&!memcmp(k,key,kc)) { \
    int vn; \
    if ((sr_int_eval(&vn,v,vc)<2)||(vn<lo)||(vn>hi)) { \
      fprintf(stderr,"%s:metadata:%d: Malformed field '%.*s' = '%.*s', expected integer in %d..%d\n",path,res->rid,kc,k,vc,v,lo,hi); \
      return -2; \
    } \
    return 0; \
  }
  #define FK(typetag) \
    int frid=0; \
    if ((sr_int_eval(&frid,v,vc)<2)||(frid<1)||(frid>0xffff)) { \
      fprintf(stderr,"%s:metadata:%d: Expected %s id in 1..65535 for '%.*s', found '%.*s'\n",path,res->rid,#typetag,kc,k,vc,v); \
      return -2; \
    } \
    int fresp=eggdev_rom_search(rom,EGG_TID_##typetag,frid); \
    if (fresp<0) { \
      fprintf(stderr,"%s:metadata:%d: Resource %s:%d not found, referred by field '%.*s'\n",path,res->rid,#typetag,frid,kc,k); \
      return -2; \
    } \
    const struct eggdev_res *fres=rom->resv+fresp;

  // fb: "WIDTHxHEIGHT"
  if ((kc==2)&&!memcmp(k,"fb",2)) {
    int w=0,h=0,i=0;
    for (;i<vc;i++) {
      if (v[i]=='x') {
        if (sr_int_eval(&w,v,i)<2) w=0;
        if (sr_int_eval(&h,v+i+1,vc-i-1)<2) h=0;
        break;
      }
    }
    if ((w<1)||(h<1)||(w>EGG_TEXTURE_SIZE_LIMIT)||(h>EGG_TEXTURE_SIZE_LIMIT)) {
      fprintf(stderr,"%s:metadata:%d: 'fb' must be 'WIDTHxHEIGHT' in 1..%d.\n",path,res->rid,EGG_TEXTURE_SIZE_LIMIT);
      return -2;
    }
    return 0;
  }
  
  // lang: Comma-delimited ISO 639 codes.
  if ((kc==4)&&!memcmp(k,"lang",4)) {
    int i=0; while (i<vc) {
      if ((unsigned char)v[i]<=0x20) { i++; continue; }
      if (v[i]==',') { i++; continue; }
      const char *token=v+i;
      int tokenc=0;
      while ((i<vc)&&(v[i++]!=',')) tokenc++;
      while (tokenc&&((unsigned char)token[tokenc-1]<=0x20)) tokenc--;
      if (!tokenc) continue;
      if ((tokenc!=2)||(token[0]<'a')||(token[0]>'z')||(token[1]<'a')||(token[1]>'z')) {
        fprintf(stderr,"%s:metadata:%d: 'lang' must be comma-delimited ISO 639 language codes, ie 2 lowercase letters. Found '%.*s'.\n",path,res->rid,tokenc,token);
        return -2;
      }
    }
    return 0;
  }
  
  // freedom: enum
  if ((kc==7)&&!memcmp(k,"freedom",7)) {
    if ((vc==10)&&!memcmp(v,"restricted",10)) return 0;
    if ((vc==7)&&!memcmp(v,"limited",7)) return 0;
    if ((vc==6)&&!memcmp(v,"intact",6)) return 0;
    if ((vc==4)&&!memcmp(v,"free",4)) return 0;
    fprintf(stderr,"%s:metadata:%d: 'freedom' must be one of 'restricted', 'limited', 'intact', 'free'. Found '%.*s'.\n",path,res->rid,vc,v);
    return -2;
  }
  
  // required, optional: Comma-delimited list of features.
  if ((kc==8)&&(!memcmp(k,"required",8)||!memcmp(k,"optional",8))) {
    int i=0; while (i<vc) {
      if ((unsigned char)v[i]<=0x20) { i++; continue; }
      if (v[i]==',') { i++; continue; }
      const char *token=v+i;
      int tokenc=0;
      while ((i<vc)&&(v[i++]!=',')) tokenc++;
      while (tokenc&&((unsigned char)token[tokenc-1]<=0x20)) tokenc--;
      if (!tokenc) continue;
      int err=eggdev_validate_metadata_feature(token,tokenc,res,rom,path);
      if (err<0) return err;
    }
    return 0;
  }
  
  // players: "N" or "[N]..[N]"
  if ((kc==7)&&!memcmp(k,"players",7)) {
    int i=0; for (;i<vc;i++) {
      if (v[i]=='.') {
        if ((i>vc-2)||(v[i+1]!='.')) {
          fprintf(stderr,"%s:metadata:%d: 'players' must be 'N' or 'N..N', found '%.*s'\n",path,res->rid,vc,v);
          return -2;
        }
        int lo=0,hi=INT_MAX;
        if (i>0) {
          if (sr_int_eval(&lo,v,i)<2) {
            fprintf(stderr,"%s:metadata:%d: Expected integer for 'players' low, found '%.*s'\n",path,res->rid,i,v);
            return -2;
          }
        }
        if (i+2<vc) {
          if (sr_int_eval(&hi,v+i+2,vc-i-2)<2) {
            fprintf(stderr,"%s:metadata:%d: Expected integer for 'players' high, found '%.*s'\n",path,res->rid,vc-i-2,v+i+2);
            return -2;
          }
        }
        if ((lo<0)||(lo>hi)) {
          fprintf(stderr,"%s:metadata:%d: Invalid range %d..%d for 'players'\n",path,res->rid,lo,hi);
          return -2;
        }
        return 0;
      }
    }
    if ((sr_int_eval(&i,v,vc)<2)||(i<0)) {
      fprintf(stderr,"%s:metadata:%d: 'players' must be 'N' or 'N..N', found '%.*s'\n",path,res->rid,vc,v);
      return -2;
    }
    return 0;
  }
  
  // rating: Format not yet defined, but will be machine-readable.
  if ((kc==6)&&!memcmp(k,"rating",6)) {
    //TODO Validate rating
    return 0;
  }
  
  // time: ISO 8601.
  if ((kc==4)&&!memcmp(k,"time",4)) {
    int vp=0;
    #define DIGITS(digitc) { \
      if (vp>=vc) return 0; \
      if (vp>vc-digitc) { \
        fprintf(stderr,"%s:metadata:%d: Invalid 'time', EOF expecting %d digits\n",path,res->rid,digitc); \
        return -2; \
      } \
      int i=digitc; while (i-->0) { \
        if ((v[vp]<'0')||(v[vp]>'9')) { \
          fprintf(stderr,"%s:metadata:%d: Invalid 'time', expected digit but found '%c'\n",path,res->rid,v[vp]); \
          return -2; \
        } \
        vp++; \
      } \
    }
    DIGITS(4) // year
    if ((vp<vc)&&(v[vp]=='-')) vp++;
    DIGITS(2) // month
    if ((vp<vc)&&(v[vp]=='-')) vp++;
    DIGITS(2) // day
    if ((vp<vc)&&(v[vp]=='T')) vp++;
    DIGITS(2) // hour
    if ((vp<vc)&&(v[vp]==':')) vp++;
    DIGITS(2) // minute
    if ((vp<vc)&&(v[vp]==':')) vp++;
    DIGITS(2) // second
    if ((vp<vc)&&(v[vp]=='.')) vp++;
    DIGITS(vc-vp)
    return 0;
  }
  
  // iconImage: image rid, 16x16
  if ((kc==9)&&!memcmp(k,"iconImage",9)) {
    FK(image)
    struct image *image=image_decode(fres->serial,fres->serialc);
    if (!image) return 0; // Don't report the error here. We will validate image resources on their own.
    if ((image->w!=16)||(image->h!=16)) {
      fprintf(stderr,"%s:metadata:%d:WARNING: image:%d for 'iconImage' has dimensions %dx%d. Recommend 16x16 instead.\n",path,res->rid,frid,image->w,image->h);
    }
    image_del(image);
    return 0;
  }
  
  // posterImage: image rid, 2:1
  if ((kc==11)&&!memcmp(k,"posterImage",11)) {
    FK(image)
    struct image *image=image_decode(fres->serial,fres->serialc);
    if (!image) return 0; // Don't report the error here. We will validate image resources on their own.
    if (image->w!=image->h<<1) {
      fprintf(stderr,"%s:metadata:%d:WARNING: image:%d for 'posterImage' has dimensions %dx%d. 2:1 aspect is recommended.\n",path,res->rid,frid,image->w,image->h);
    }
    if ((fres->serialc<8)||memcmp(fres->serial,"\x89PNG\r\n\x1a\n",8)) {
      fprintf(stderr,"%s:metadata:%d:WARNING: image:%d for 'posterImage' should be PNG format, external tools might read it.\n",path,res->rid,frid);
    }
    image_del(image);
    return 0;
  }
  
  PLAININT("audioRate",200,200000)
  PLAININT("audioChanc",1,8)

  #undef PLAININT
  #undef FK
  return 0;
}

/* Validate metadata.
 * Caller does the special rid=1 assertion.
 */
 
static int eggdev_validate_metadata(const struct eggdev_res *res,const struct eggdev_rom *rom,const char *path) {
  if ((res->serialc<4)||memcmp(res->serial,"\0EM\xff",4)) {
    fprintf(stderr,"%s:metadata:%d: Signature mismatch\n",path,res->rid);
    return -2;
  }
  
  // There is no field limit in the spec.
  // We're creating one, just to avoid dynamic allocation.
  // But either way, 128 is a crazy size for metadata, if there's that many there's something wrong for sure.
  #define FIELD_LIMIT 128
  struct field {
    const char *k,*v;
    int kc,vc;
  } fieldv[FIELD_LIMIT];
  int fieldc=0;
  
  const uint8_t *src=res->serial;
  int srcp=4;
  int result=0; // Formatting errors don't stop us, don't have to at least.
  int lang=0;
  for (;;) {
  
    // Framing.
    if (srcp>=res->serialc) {
      fprintf(stderr,"%s:metadata:%d: Required terminator missing.\n",path,res->rid);
      return -2;
    }
    int kc=src[srcp++];
    if (!kc) break;
    if (srcp>=res->serialc) {
      fprintf(stderr,"%s:metadata:%d: Unexpected EOF\n",path,res->rid);
      return -2;
    }
    int vc=src[srcp++];
    if (srcp>res->serialc-vc-kc) {
      fprintf(stderr,"%s:metadata:%d: Unexpected EOF\n",path,res->rid);
      return -2;
    }
    const char *k=(char*)(src+srcp); srcp+=kc;
    const char *v=(char*)(src+srcp); srcp+=vc;
    
    // All text, both key and value, must be ASCII G0.
    {
      const unsigned char *q;
      int i;
      for (q=(unsigned char*)k,i=kc;i-->0;q++) {
        if ((*q<0x20)||(*q>0x7e)) {
          fprintf(stderr,"%s:metadata:%d: Illegal byte 0x%02x in key.\n",path,res->rid,*q);
          return -2;
        }
      }
      for (q=(unsigned char*)v,i=vc;i-->0;q++) {
        if ((*q<0x20)||(*q>0x7e)) {
          fprintf(stderr,"%s:metadata:%d: Illegal byte 0x%02x in value for '%.*s'.\n",path,res->rid,*q,kc,k);
          return -2;
        }
      }
    }
    
    // Confirm we haven't seen this key already.
    {
      const struct field *q=fieldv;
      int i=fieldc;
      for (;i-->0;q++) {
        if (q->kc!=kc) continue;
        if (memcmp(q->k,k,kc)) continue;
        fprintf(stderr,"%s:metadata:%d: Duplicate field '%.*s', values '%.*s' and '%.*s'\n",path,res->rid,kc,k,q->vc,q->v,vc,v);
        result=-2;
      }
    }
    
    // Add to field list.
    if (fieldc>=FIELD_LIMIT) {
      fprintf(stderr,"%s:metadata:%d: Too many fields, limit %d. Artificial limit imposed around %s:%d.\n",path,res->rid,FIELD_LIMIT,__FILE__,__LINE__);
      return -2;
    }
    struct field *field=fieldv+fieldc++;
    field->k=k;
    field->kc=kc;
    field->v=v;
    field->vc=vc;
    
    // Single-field validation.
    int err=eggdev_validate_metadata_field(k,kc,v,vc,res,rom,path);
    if (err<0) {
      if (err!=-2) fprintf(stderr,"%s:metadata:%d: Unspecified validation error for field '%.*s' = '%.*s'\n",path,res->rid,kc,k,vc,v);
      result=-2;
    }
    
    // lang: Yoink the default language.
    if ((kc==4)&&!memcmp(k,"lang",4)) {
      while (vc&&(((unsigned char)v[0]<=0x20)||(v[0]==','))) { v++; vc++; }
      if ((vc>=2)&&(v[0]>='a')&&(v[0]<='z')&&(v[1]>='a')&&(v[1]<='z')) {
        lang=EGG_LANG_FROM_STRING(v);
      }
    }
  }
  
  // Required fields. Overkilling this with a neat list, you only have to add required fields once.
  // Of course there is only one of them and will probably never be another.
  #define FOR_EACH_REQUIRED \
    _(fb)
  #define _(tag) int have_##tag=0;
  FOR_EACH_REQUIRED
  #undef _
  const struct field *field=fieldv;
  int i=fieldc;
  for (;i-->0;field++) {
    #define _(tag) if ((field->kc==sizeof(#tag)-1)&&!memcmp(field->k,#tag,field->kc)) have_##tag=1;
    FOR_EACH_REQUIRED
    #undef _
  }
  int missing=0;
  #define _(tag) if (!have_##tag) { \
    fprintf(stderr,"%s:metadata:%d: Missing required field '%s'\n",path,res->rid,#tag); \
    missing=1; \
  }
  FOR_EACH_REQUIRED
  #undef _
  if (missing) return -2;
  #undef FOR_EACH_REQUIRED
  
  // '$' keys must have a non-$ equivalent. If its value in the ROM's default language is metadata-legal, it must be the same as the non-$ value.
  int stringsid=(lang<<6)|1;
  for (field=fieldv,i=fieldc;i-->0;field++) {
    if (field->k[field->kc-1]!='$') continue;
    const struct field *plain=0;
    int ii=fieldc;
    while (ii-->0) {
      if (fieldv[ii].kc!=field->kc-1) continue;
      if (memcmp(fieldv[ii].k,field->k,field->kc-1)) continue;
      plain=fieldv+ii;
      break;
    }
    if (!plain) {
      fprintf(stderr,"%s:metadata:%d: '%.*s' must have a default non-$ version too.\n",path,res->rid,field->kc,field->k);
      result=-2;
      continue;
    }
    int index;
    if ((sr_int_eval(&index,field->v,field->vc)<2)||(index<0)) {
      fprintf(stderr,"%s:metadata:%d: Expected integer (string index) for '%.*s', found '%.*s'.\n",path,res->rid,field->kc,field->k,field->vc,field->v);
      result=-2;
      continue;
    }
    const char *string=0;
    int stringc=eggdev_strings_get(&string,stringsid,index);
    if (stringc<0) stringc=0;
    if ((stringc!=plain->vc)||memcmp(string,plain->v,stringc)) {
      fprintf(stderr,
        "%s:metadata:%d: Default value for '%.*s' ('%.*s') does not match strings:%d:%d ('%.*s')\n",
        path,res->rid,plain->kc,plain->k,plain->vc,plain->v,stringsid,index,stringc,string
      );
      result=-2;
    }
  }
  
  #undef FIELD_LIMIT
  return result;
}

/* Validate code.
 * Caller does the special rid=1 assertion.
 */
 
static int eggdev_validate_code(const struct eggdev_res *res,const struct eggdev_rom *rom,const char *path) {
  // Lots of validation is possible against a WebAssembly module, but we're not going deep.
  // Just check the signature.
  if ((res->serialc<4)||memcmp(res->serial,"\0asm",4)) {
    fprintf(stderr,"%s:code:%d: Not a WebAssembly module, signature mismatch.\n",path,res->rid);
    return -2;
  }
  return 0;
}

/* Validate strings, individual resource.
 */
 
static int eggdev_validate_strings(const struct eggdev_res *res,const struct eggdev_rom *rom,const char *path) {
  int status=0;
  if ((res->serialc<4)||memcmp(res->serial,"\0ES\xff",4)) {
    fprintf(stderr,"%s:strings:%d: Signature mismatch\n",path,res->rid);
    return -2;
  }
  
  // Expect a language code in the high bits.
  int rid6=res->rid&0x3f;
  int lang=res->rid>>6;
  int hi=(lang>>5)&0x1f;
  int lo=lang&0x1f;
  if ((hi>=26)||(lo>=26)) {
    hi='?'-'a';
    lo='?'-'a';
    rid6=res->rid;
  }
  char rrepr[32];
  int rreprc=snprintf(rrepr,sizeof(rrepr),"strings:%c%c:%d",'a'+hi,'a'+lo,rid6);
  if ((rreprc<0)||(rreprc>=sizeof(rrepr))) rrepr[0]=0;
  
  /* Find any other strings resource of the same 6-bit rid (ie a different language).
   * If there's none, just take a copy of myself to compare against, it will pass of course.
   * Any other weirdness at the TOC level is not our problem, we only care about content.
   */
  char peerlang[3]={'a'+hi,'a'+lo,0};
  const uint8_t *peer=res->serial;
  int peerc=res->serialc;
  int resp=eggdev_rom_search(rom,EGG_TID_strings,0);
  if (resp<0) resp=-resp-1;
  for (;(resp<rom->resc)&&(rom->resv[resp].tid==EGG_TID_strings);resp++) {
    const struct eggdev_res *q=rom->resv+resp;
    if (q->rid==res->rid) continue;
    if ((q->rid&0x3f)!=rid6) continue;
    peer=q->serial;
    peerc=q->serialc;
    peerlang[0]='a'+((q->rid>>11)&0x1f);
    peerlang[1]='a'+((q->rid>>6)&0x1f);
    break;
  }
  
  const uint8_t *src=res->serial;
  int srcp=4,peerp=4,index=0;
  for (;;) {
    if ((srcp>=res->serialc)&&(peerp>=peerc)) break;
    
    // Measure in both (src) and (peer). Ignore framing errors in (peer).
    int srclen=0,peerlen=0;
    if (srcp<res->serialc) {
      srclen=src[srcp++];
      if (srclen&0x80) {
        if (srcp>=res->serialc) {
          fprintf(stderr,"%s:%s: Unexpected EOF\n",path,rrepr);
          return -2;
        }
        srclen=((srclen&0x7f)<<8)|src[srcp++];
      }
      if (srcp>res->serialc-srclen) {
        fprintf(stderr,"%s:%s: Unexpected EOF\n",path,rrepr);
        return -2;
      }
    }
    if (peerp<peerc) {
      peerlen=peer[peerp++];
      if (peerlen&0x80) {
        if (peerp>=peerc) {
          peerlen=0;
        }
        peerlen=((peerlen&0x7f)<<8)|peer[peerp++];
      }
      if (peerp>peerc-peerlen) {
        peerlen=0;
        peerp=peerc;
      }
      peerp+=peerlen; // No need to linger here; all we want is the length.
    }
    
    // Validate UTF-8 encoding. All codepoints are legal, even NUL, all we care about is the encoding.
    const char *v=(char*)(src+srcp);
    srcp+=srclen;
    int vp=0;
    while (vp<srclen) {
      int seqlen,codepoint;
      if ((seqlen=sr_utf8_decode(&codepoint,v+vp,srclen-vp))<1) {
        fprintf(stderr,"%s:%s: UTF-8 misencode in index %d around byte %d/%d 0x%02x\n",path,rrepr,index,vp,srclen,v[vp]);
        status=-2;
        break;
      } else {
        vp+=seqlen;
      }
    }
    
    // Compare my length to peer length. They must both be zero or both nonzero.
    // This is an important check, one of the main reasons `validate` exists. It's easy to forget to add translations for new strings.
    if (srclen&&!peerlen) {
      fprintf(stderr,"%s:%s: Index %d is present here but absent from '%s'\n",path,rrepr,index,peerlang);
      status=-2;
    } else if (!srclen&&peerlen) {
      fprintf(stderr,"%s:%s: Index %d is absent here but present in '%s'\n",path,rrepr,index,peerlang);
      status=-2;
    }
    
    index++;
  }
  return status;
}

/* Validate the set of strings resources.
 * We don't look at their content.
 * Assert that each sub-rid that exists, exists for each language named in metadata.
 */
 
static int eggdev_validate_multi_strings(const struct eggdev_rom *rom,const char *path) {
  int status=0;

  // Pull "lang" from metadata:1 if present.
  const char *langs=0;
  int langsc=0;
  if ((rom->resc>=1)&&(rom->resv[0].tid==EGG_TID_metadata)&&(rom->resv[0].rid==1)) {
    langsc=eggdev_metadata_get(&langs,rom->resv[0].serial,rom->resv[0].serialc,"lang",4);
  }
  
  /* Examine the resources present and note all the languages and sub-rids.
   * Log any fishy rids after splitting.
   */
  int stringsc=0;
  uint8_t count_by_lang[1024]={0};
  uint8_t count_by_rid[64]={0};
  int langlo=1024,langhi=0,langcmax=0,ridlo=63,ridhi=0,ridcmax=0;
  int resp=eggdev_rom_search(rom,EGG_TID_strings,0);
  if (resp<0) resp=-resp-1;
  for (;(resp<rom->resc)&&(rom->resv[resp].tid==EGG_TID_strings);resp++) {
    stringsc++;
    const struct eggdev_res *res=rom->resv+resp;
    int lang=(res->rid>>6)&0x3ff;
    int rid=res->rid&0x3f;
    count_by_lang[lang]++;
    count_by_rid[rid]++;
    if (lang<langlo) langlo=lang;
    if (lang>langhi) langhi=lang;
    if (count_by_lang[lang]>langcmax) langcmax=count_by_lang[lang];
    if (rid<ridlo) ridlo=rid;
    if (rid>ridhi) ridhi=rid;
    if (count_by_rid[rid]>ridcmax) ridcmax=count_by_rid[rid];
    if (!rid) {
      fprintf(stderr,"%s:strings:%d: Lower 6 bits of strings rid must not be zero.\n",path,res->rid);
      status=-2;
    }
    int hi=lang>>5;
    int lo=lang&0x1f;
    if ((hi>=26)||(lo>=26)) {
      fprintf(stderr,"%s:strings:%d: Invalid language code in strings rid, will read as '%c%c'\n",path,res->rid,'a'+hi,'a'+lo);
      status=-2;
    } else if (!lang) {
      fprintf(stderr,"%s:strings:%d: strings rid should contain a language code in the high 10 bits. (zero is 'aa' which is not a real language)\n",path,res->rid);
      status=-2;
    }
  }
  
  /* If there weren't any strings resources, great, we're done.
   * Even if the metadata is set, that might just refer to the metadata's static text, or text in images, or something.
   */
  if (!stringsc) return 0;
  
  /* Every language named in metadata must have at least one strings resource.
   * (it must have all of them, but it's enough here to check for just one).
   * We'll also assert that metadata lang is well formed.
   */
  int langsp=0,declared_lang_count=0;
  while (langsp<langsc) {
    if (((unsigned char)langs[langsp]<=0x20)||(langs[langsp]==',')) {
      langsp++;
      continue;
    }
    const char *token=langs+langsp;
    int tokenc=0;
    while ((langsp<langsc)&&((unsigned char)langs[langsp]>0x20)&&(langs[langsp]!=',')) { langsp++; tokenc++; }
    if ((tokenc!=2)||(token[0]<'a')||(token[0]>'z')||(token[1]<'a')||(token[1]>'z')) {
      fprintf(stderr,"%s:metadata:1: Invalid language '%.*s'. Expected 2 lowercase letters.\n",path,tokenc,token);
      status=-2;
    } else {
      declared_lang_count++;
      int lang=((token[0]-'a')<<5)|(token[1]-'a');
      if (!count_by_lang[lang]) {
        fprintf(stderr,"%s:metadata:1: Language '%.*s' declared in metadata but no strings present.\n",path,tokenc,token);
        status=-2;
      }
    }
  }
  
  /* The nonzero members of count_by_lang and count_by_rid must all be equal.
   * Ideally I guess we should tell the user which ones are missing, but "something missing for language/rid X" i think will be enough.
   */
  int i,strings_lang_count=0;
  for (i=langlo;i<=langhi;i++) {
    if (!count_by_lang[i]) continue;
    strings_lang_count++;
    int missingc=langcmax-count_by_lang[i];
    if (missingc<=0) continue;
    char name[3]={
      'a'+((i>>5)&0x1f),
      'a'+(i&0x1f),
      0,
    };
    fprintf(stderr,"%s: Language '%s' is missing %d strings resource%s\n",path,name,missingc,(missingc==1)?"":"s");
    status=-2;
  }
  for (i=ridlo;i<=ridhi;i++) {
    if (!count_by_rid[i]) continue;
    int missingc=ridcmax-count_by_rid[i];
    if (missingc<=0) continue;
    fprintf(stderr,"%s: strings:%d is missing for %d language%s\n",path,i,missingc,(missingc==1)?"":"s");
    status=-2;
  }
  if (strings_lang_count!=declared_lang_count) {
    fprintf(stderr,
      "%s: metadata:1 declares %d language%s but strings resources were found for %d.\n",
      path,declared_lang_count,(declared_lang_count==1)?"":"s",strings_lang_count
    );
    status=-2;
  }
  
  return status;
}

/* Validate image.
 */
 
static int eggdev_validate_image(const struct eggdev_res *res,const struct eggdev_rom *rom,const char *path) {
  struct image *image=image_decode(res->serial,res->serialc);
  if (!image) {
    fprintf(stderr,"%s:image:%d: Failed to decode %d-byte image\n",path,res->rid,res->serialc);
    return -2;
  }
  if ((image->w<1)||(image->w>0x7fff)||(image->h<1)||(image->h>0x7fff)) {
    fprintf(stderr,"%s:image:%d: Size %dx%d outside legal range 1..32767\n",path,res->rid,image->w,image->h);
    image_del(image);
    return -2;
  }
  // Plenty of opportunity here to look for other odd conditions. Is there wasted space? Nonzero chroma on transparent pixels? Intermediate alpha?
  // Maybe return here if we establish firm conventions about what's acceptable for an image.
  image_del(image);
  return 0;
}

/* Validate EGS.
 */
 
static int eggdev_validate_egs(const struct eggdev_res *res,const struct eggdev_rom *rom,const char *path,int sndid) {
  const uint8_t *src=res->serial;
  int srcc=res->serialc;
  int status=0;
  #define ERROR(fmt,...) { \
    fprintf(stderr,"%s:%s:%d:%d: "fmt"\n",path,eggdev_tid_repr(res->tid),res->rid,sndid,##__VA_ARGS__); \
    return -2; \
  }
  #define SOFTERROR(fmt,...) { \
    fprintf(stderr,"%s:%s:%d:%d: "fmt"\n",path,eggdev_tid_repr(res->tid),res->rid,sndid,##__VA_ARGS__); \
    status=-2; \
  }
  
  if ((srcc<4)||memcmp(src,"\0EGS",4)) ERROR("EGS signature mismatch.")
  int srcp=4;
  
  int chanv[16]={0};
  int chid=0;
  for (;;) {
    if (srcp>=srcc) break;
    if (srcp>srcc-2) ERROR("Unexpected EOF reading Channel Header length.")
    int chlen=(src[srcp]<<8)|src[srcp+1];
    srcp+=2;
    if (!chlen) break;
    if (srcp>srcc-chlen) ERROR("Unexpected EOF reading Channel Header.")
    const uint8_t *chsrc=src+srcp;
    srcp+=chlen;
    while ((chlen>=2)&&(chsrc[0]==0x00)) { // trim leading noops
      chsrc+=2;
      chlen-=2;
    }
    if (chid>=16) SOFTERROR("Channel Header for chid %d will be ignored. Limit 16.",chid)
    else if (chlen) {
      chanv[chid]=1;
      //TODO Validate channel header.
    }
    chid++;
  }
  
  int duration=0;
  int final_release_time=0;
  while (srcp<srcc) {
    uint8_t lead=src[srcp++];
    if (!lead) break;
    if (!(lead&0x80)) {
      int delay=lead&0x3f;
      if (lead&0x40) delay=(delay+1)<<6;
      duration+=delay;
      continue;
    }
    switch (lead&0xf0) {
      case 0x80:
      case 0x90:
      case 0xa0: {
          if (srcp>srcc-2) ERROR("Unexpected EOF in note event.")
          uint8_t a=src[srcp++];
          uint8_t b=src[srcp++];
          uint8_t chid=lead&0x0f;
          if (!chanv[chid]) SOFTERROR("Note event on unconfigured channel %d.",chid)
          int endtime=b&0x1f;
          switch (lead&0xf0) {
            case 0x80: endtime=0; break;
            case 0x90: endtime=(endtime+1)<<4; break;
            case 0xa0: endtime=(endtime+1)<<7; break;
          }
          endtime+=duration;
          if (endtime>final_release_time) final_release_time=endtime;
        } break;
      case 0xb0: {
          if (srcp>srcc-1) ERROR("Unexpected EOF in wheel event.")
          uint8_t v=src[srcp++];
          uint8_t chid=lead&0x0f;
          if (!chanv[chid]) SOFTERROR("Wheel event on unconfigured channel %d.",chid)
          // We could warn about redundant wheel changes... meh
        } break;
      default: ERROR("Unexpected leading byte 0x%02x for event around %d/%d.",lead,srcp-1,srcc)
    }
  }
  
  if (sndid>=0) {
    if (duration>5000) SOFTERROR("Duration %d exceeds limit 5000 for sound effects.",duration)
  }
  
  if (final_release_time>duration) {
    SOFTERROR("Final release time %d > duration %d. Player's behavior is undefined.",final_release_time,duration)
  }
  
  if (!duration) SOFTERROR("Expected at least one delay event.")
  
  #undef ERROR
  #undef SOFTERROR
  return status;
}

/* Validate WAV.
 */
 
static int eggdev_validate_wav(const uint8_t *src,int srcc,const char *path,int rid,int index) {
  if ((srcc<12)||memcmp(src,"RIFF",4)||memcmp(src+8,"WAVE",4)) {
    fprintf(stderr,"%s:sounds:%d:%d: Invalid WAV signature.\n",path,rid,index);
    return -2;
  }
  int srcp=12,stopp=srcc-8,status=0,fmtc=0;
  while (srcp<=stopp) {
    const uint8_t *chunkid=src+srcp; srcp+=4;
    int chunklen=src[srcp]|(src[srcp+1]<<8)|(src[srcp+2]<<16)|(src[srcp+3]<<24); srcp+=4;
    if ((chunklen<0)||(srcp>srcc-chunklen)) {
      fprintf(stderr,"%s:sounds:%d:%d: Chunk overflows EOF\n",path,rid,index);
      return -2;
    }
    const uint8_t *chunk=src+srcp;
    srcp+=chunklen;
    if (chunklen&1) srcp++; // Chunks must round up to 2 bytes. The hell, Microsoft? Just what the hell?
    if (!memcmp(chunkid,"fmt ",4)) {
      if (fmtc++) {
        fprintf(stderr,"%s:sounds:%d:%d: Multiple WAV 'fmt ' chunks\n",path,rid,index);
        return -2;
      }
      if (chunklen<16) {
        fprintf(stderr,"%s:sounds:%d:%d: Expected at least 16 bytes for WAV 'fmt ' chunk, found %d\n",path,rid,index,chunklen);
        return -2;
      }
      int fmt=chunk[0]|(chunk[1]<<8);
      int chanc=chunk[2]|(chunk[3]<<8);
      int rate=chunk[4]|(chunk[5]<<8)|(chunk[6]<<16)|(chunk[7]<<24);
      int samplesize=chunk[14]|(chunk[15]<<8);
      if (fmt!=1) {
        fprintf(stderr,"%s:sounds:%d:%d: WAV sample format %d not supported. Must be 1 (Linear PCM).\n",path,rid,index,fmt);
        status=-2;
      }
      if (chanc!=1) {
        fprintf(stderr,"%s:sounds:%d:%d: WAV channel count %d, should be 1.\n",path,rid,index,chanc);
        status=-2;
      }
      if ((rate<200)||(rate>200000)) {
        fprintf(stderr,"%s:sounds:%d:%d: Unrealistic sample rate %d hz. May fail to load at runtime.\n",path,rid,index,rate);
        status=-2;
      }
      if (samplesize!=16) {
        fprintf(stderr,"%s:sounds:%d:%d: Sample size %d, must be 16.\n",path,rid,index,samplesize);
        status=-2;
      }
    }
  }
  if (!fmtc) {
    fprintf(stderr,"%s:sounds:%d:%d: WAV missing 'fmt ' chunk\n",path,rid,index);
    return -2;
  }
  return status;
}

/* Validate sound.
 */
 
static int eggdev_validate_sound(const struct eggdev_res *res,const struct eggdev_rom *rom,const char *path) {
  const uint8_t *src=res->serial;
  int srcc=res->serialc,err,status=0;
  if ((srcc<4)||memcmp(src,"\0MSF",4)) {
    fprintf(stderr,"%s:sounds:%d: MSF signature mismatch.\n",path,res->rid);
    return -2;
  }
  int srcp=4,index=0;
  while (srcp<srcc) {
    if (srcp>srcc-4) {
      fprintf(stderr,"%s:sounds:%d: Unexpected EOF reading MSF entry header.\n",path,res->rid);
      return -2;
    }
    int d=src[srcp++];
    int len=(src[srcp]<<16)|(src[srcp+1]<<8)|src[srcp+2];
    srcp+=3;
    if (srcp>srcc-len) {
      fprintf(stderr,"%s:sounds:%d: Unexpected EOF\n",path,res->rid);
      return -2;
    }
    index+=d+1;
    const uint8_t *subsrc=src+srcp;
    srcp+=len;
    struct eggdev_res fakeres=*res;
    fakeres.serial=(void*)subsrc;
    fakeres.serialc=len;
    
    if (!len) {
      // Empty is ok.
    } else if ((len>=4)&&!memcmp(subsrc,"\0EGS",4)) {
      if ((err=eggdev_validate_egs(&fakeres,rom,path,index))<status) status=err;
    } else if ((len>=4)&&!memcmp(subsrc,"RIFF",4)) {
      if ((err=eggdev_validate_wav(subsrc,len,path,res->rid,index))<status) status=err;
    } else {
      fprintf(stderr,"%s:sounds:%d:%d: Unrecognized format.\n",path,res->rid,index);
      status=-2;
    }
  }
  return status;
}

/* Validate song.
 */
 
static int eggdev_validate_song(const struct eggdev_res *res,const struct eggdev_rom *rom,const char *path) {
  return eggdev_validate_egs(res,rom,path,-1);
}

/* Validate ROM.
 * Since we managed to acquire a ROM object, most of the validation is already done.
 * We'll be looking at finer-grained things, formatting of individual resources etc.
 */
 
static int eggdev_validate_rom(struct eggdev_rom *rom,const char *path) {
  if (rom->resc<1) {
    fprintf(stderr,"%s: Empty ROM. Must contain at least metadata:1.\n",path);
    return -2;
  }
  int status=0,err,have_meta1=0,have_code1=0;
  int i=rom->resc;
  const struct eggdev_res *res=rom->resv;
  #define CALLOUT(fncall) if ((err=(fncall))<0) { \
    if (err==-2) status=-2; \
    else if (status>=0) status=-1; \
  }
  for (;i-->0;res++) {
    switch (res->tid) {
    
      case EGG_TID_metadata: {
          if (res->rid==1) {
            have_meta1=1;
          } else {
            fprintf(stderr,"%s: Unexpected %d-byte resource metadata:%d. metadata id may only be 1.\n",path,res->serialc,res->rid);
            status=-2;
          }
          CALLOUT(eggdev_validate_metadata(res,rom,path))
        } break;
        
      case EGG_TID_code: {
          if (res->rid==1) {
            have_code1=1;
          } else {
            fprintf(stderr,"%s: Unexpected %d-byte resource code:%d. code id may only be 1.\n",path,res->serialc,res->rid);
            status=-2;
          }
          CALLOUT(eggdev_validate_code(res,rom,path))
        } break;
        
      case EGG_TID_strings: CALLOUT(eggdev_validate_strings(res,rom,path)) break;
      case EGG_TID_image: CALLOUT(eggdev_validate_image(res,rom,path)) break;
      case EGG_TID_sound: CALLOUT(eggdev_validate_sound(res,rom,path)) break;
      case EGG_TID_song: CALLOUT(eggdev_validate_song(res,rom,path)) break;
      
    }
  }
  CALLOUT(eggdev_validate_multi_strings(rom,path))
  if (!have_meta1) {
    fprintf(stderr,"%s: Required resource metadata:1 was not found.\n",path);
    status=-2;
  }
  if (!have_code1) {
    fprintf(stderr,"%s:WARNING: code:1 not found. For 'true' or 'recom' executables, this is normal.\n",path);
  }
  #undef CALLOUT
  return status;
}

/* React to a failure from eggdev_rom_add_path().
 * If not (logged_already), we guarantee to log something.
 */
 
static void eggdev_validate_why_did_rom_fail(const char *path,int logged_already) {
  //TODO
  if (!logged_already) {
    fprintf(stderr,"%s: Unspecified error reading ROM\n",path);
  }
}

/* validate, main entry point.
 */
 
int eggdev_main_validate() {
  if (eggdev.srcpathc!=1) {
    fprintf(stderr,"%s: 'validate' requires exactly one input source.\n",eggdev.exename);
    return -2;
  }
  int err=eggdev_require_rom(eggdev.srcpathv[0]);
  if (err<0) {
    eggdev_validate_why_did_rom_fail(eggdev.srcpathv[0],err==-2);
    return -2;
  }
  err=eggdev_validate_rom(eggdev.rom,eggdev.srcpathv[0]);
  if (err>=0) return 0;
  if (err!=-2) fprintf(stderr,"%s: Unspecified error\n",eggdev.srcpathv[0]);
  return -2;
}
