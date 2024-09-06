#include "sfgc_internal.h"

#define FAIL(fmt,...) { \
  if (refname) { \
    fprintf(stderr,"%s:%d: "fmt"\n",refname,lineno,##__VA_ARGS__); \
    return -2; \
  } \
  return -1; \
}

#define STATE_ANYTHING 1
#define STATE_shape 2
#define STATE_harm 4
#define STATE_fm 8
#define STATE_fmlfo 16
#define STATE_freq 32
#define STATE_freqlfo 64
#define STATE_PIPELINE 128
#define STATE_level 256

/* Compile envelope.
 */
 
static int sfg_compile_env(struct sr_encoder *dst,const char *src,int srcc,double vhi,double scale,const char *refname,int lineno) {

  // Must begin with initial (or only) value.
  int srcp=0;
  while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
  const char *token=src+srcp;
  int tokenc=0;
  while ((srcp<srcc)&&((unsigned char)src[srcp++]>0x20)) tokenc++;
  double v;
  if ((sr_double_eval(&v,token,tokenc)<0)||(v<0.0)||(v>vhi)) {
    FAIL("Expected envelope value in 0..%f, found '%.*s'",vhi,tokenc,token)
  }
  v*=scale;
  int iv=(v*65535.0)/vhi;
  if (iv<0) iv=0; else if (iv>0xffff) iv=0xffff;
  if (sr_encode_intbe(dst,iv,2)<0) return -1;
  
  // Insert a placeholder for additional step count.
  int cp=dst->c,c=0;
  if (sr_encode_u8(dst,0)<0) return -1;
  
  // Followed by up to 255 "MS VALUE", which each emit in 16 bits.
  while (srcp<srcc) {
    if ((unsigned char)src[srcp]<=0x20) { srcp++; continue; }
    
    token=src+srcp;
    tokenc=0;
    while ((srcp<srcc)&&((unsigned char)src[srcp++]>0x20)) tokenc++;
    if ((sr_int_eval(&iv,token,tokenc)<2)||(iv<0)||(iv>0xffff)) FAIL("Expected ms in 0..65535, found '%.*s'",tokenc,token)
    if (sr_encode_intbe(dst,iv,2)<0) return -1;
    
    while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
    token=src+srcp;
    tokenc=0;
    while ((srcp<srcc)&&((unsigned char)src[srcp++]>0x20)) tokenc++;
    while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
    if ((sr_double_eval(&v,token,tokenc)<0)||(v<0.0)||(v>vhi)) {
      FAIL("Expected envelope value in 0..%f, found '%.*s'",vhi,tokenc,token)
    }
    v*=scale;
    iv=(v*65535.0)/vhi;
    if (iv<0) iv=0; else if (iv>0xffff) iv=0xffff;
    if (sr_encode_intbe(dst,iv,2)<0) return -1;
    
    c++;
  }
  if (c>255) FAIL("Too many envelope points (%d, limit 255)",c)
  ((uint8_t*)dst->v)[cp]=c;
    
  return 0;
}

/* Compile one line within sound block.
 */
 
static int sfg_compile_line(
  struct sr_encoder *dst,
  const char *src,int srcc,
  int *state,double *master,
  int index,const char *name,int namec,const char *refname,int lineno
) {
  int srcp=0;
  while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
  if (srcp>=srcc) return 0;
  
  const char *kw=src+srcp;
  int kwc=0;
  while ((srcp<srcc)&&((unsigned char)src[srcp++]>0x20)) kwc++;
  while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
  
  #define SCALAR(name,lo,hi) double name; { \
    const char *token=src+srcp; \
    int tokenc=0; \
    while ((srcp<srcc)&&((unsigned char)src[srcp++]>0x20)) tokenc++; \
    while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++; \
    if ((sr_double_eval(&name,token,tokenc)<0)||(name<lo)||(name>hi)) { \
      FAIL("Expected %s in %f..%f, found '%.*s'",#name,lo,hi,tokenc,token) \
    } \
  }
  
  if ((kwc==6)&&!memcmp(kw,"master",6)) {
    if ((*state)&STATE_ANYTHING) FAIL("'master' may only be the first command of a sound")
    SCALAR(level,0.0,1.0)
    *master=level;
    (*state)|=STATE_ANYTHING;
    return 0;
  }
  
  if ((kwc==8)&&!memcmp(kw,"endvoice",8)) {
    if ((*state)&~STATE_ANYTHING) {
      if (!((*state)&STATE_level)) FAIL("Voice must contain a 'level' command")
      if (sr_encode_u8(dst,0x00)<0) return -1;
    }
    (*state)=STATE_ANYTHING;
    return 0;
  }
  
  if ((kwc==5)&&!memcmp(kw,"shape",5)) {
    if ((*state)&(STATE_shape|STATE_harm|STATE_fm|STATE_fmlfo|STATE_freq|STATE_freqlfo|STATE_PIPELINE)) FAIL("Invalid position for '%.*s'",kwc,kw)
    if (sr_encode_u8(dst,0x01)<0) return -1;
    const char *name=src+srcp;
    int namec=srcc-srcp;
    uint8_t shapeid;
         if ((namec==4)&&!memcmp(name,"sine",4)) shapeid=0;
    else if ((namec==6)&&!memcmp(name,"square",6)) shapeid=1;
    else if ((namec==3)&&!memcmp(name,"saw",3)) shapeid=2;
    else if ((namec==8)&&!memcmp(name,"triangle",8)) shapeid=3;
    else if ((namec==5)&&!memcmp(name,"noise",5)) shapeid=4;
    else FAIL("Unknown shape '%.*s'. Expected one of: sine square saw triangle noise",namec,name)
    if (sr_encode_u8(dst,shapeid)<0) return -1;
    (*state)|=STATE_shape|STATE_ANYTHING;
    return 0;
  }
  
  if ((kwc==4)&&!memcmp(kw,"harm",4)) {
    if ((*state)&(STATE_harm|STATE_fm|STATE_fmlfo|STATE_freq|STATE_freqlfo|STATE_PIPELINE)) FAIL("Invalid position for '%.*s'",kwc,kw)
    if (sr_encode_u8(dst,0x02)<0) return -1;
    int cp=dst->c,c=0;
    if (sr_encode_u8(dst,0)<0) return -1;
    while (srcp<srcc) {
      SCALAR(coef,0.0,1.0)
      coef*=65536.0;
      int v;
      if (coef<=0.0) v=0;
      else if (coef>=0xffff) v=0xffff;
      else v=(int)coef;
      if (sr_encode_intbe(dst,v,2)<0) return -1;
      c++;
      if (c>=256) break;
    }
    if (!c) {
      if (sr_encode_intbe(dst,0xffff,2)<0) return -1;
      c=1;
    }
    ((uint8_t*)dst->v)[cp]=c;
    (*state)|=STATE_harm|STATE_ANYTHING;
    return 0;
  }
  
  if ((kwc==2)&&!memcmp(kw,"fm",2)) {
    if ((*state)&(STATE_fm|STATE_fmlfo|STATE_freq|STATE_freqlfo|STATE_PIPELINE)) FAIL("Invalid position for '%.*s'",kwc,kw)
    if (sr_encode_u8(dst,0x03)<0) return -1;
    SCALAR(rate,0.0,256.0)
    int v=(int)(rate*256.0);
    if (v<0) v=0;
    else if (v>0xffff) v=0xffff;
    if (sr_encode_intbe(dst,v,2)<0) return -1;
    if (sfg_compile_env(dst,src+srcp,srcc-srcp,16.0,1.0,refname,lineno)<0) return -1;
    (*state)|=STATE_fm|STATE_ANYTHING;
    return 0;
  }
  
  if ((kwc==5)&&!memcmp(kw,"fmlfo",5)) {
    if ((*state)&(STATE_fmlfo|STATE_freq|STATE_freqlfo|STATE_PIPELINE)) FAIL("Invalid position for '%.*s'",kwc,kw)
    if (sr_encode_u8(dst,0x04)<0) return -1;
    SCALAR(rate,0.0,256.0)
    int v=(int)(rate*256.0);
    if (v<0) v=0;
    else if (v>0xffff) v=0xffff;
    if (sr_encode_intbe(dst,v,2)<0) return -1;
    SCALAR(depth,0.0,16.0)
    v=(int)(depth*16.0);
    if (v<0) v=0;
    else if (v>0xff) v=0xff;
    if (sr_encode_u8(dst,v)<0) return -1;
    (*state)|=STATE_fmlfo|STATE_ANYTHING;
    return 0;
  }
  
  if ((kwc==4)&&!memcmp(kw,"freq",4)) {
    if ((*state)&(STATE_freq|STATE_freqlfo|STATE_PIPELINE)) FAIL("Invalid position for '%.*s'",kwc,kw)
    if (sr_encode_u8(dst,0x05)<0) return -1;
    if (sfg_compile_env(dst,src+srcp,srcc-srcp,65536.0,1.0,refname,lineno)<0) return -1;
    (*state)|=STATE_freq|STATE_ANYTHING;
    return 0;
  }
  
  if ((kwc==7)&&!memcmp(kw,"freqlfo",7)) {
    if ((*state)&(STATE_freqlfo|STATE_PIPELINE)) FAIL("Invalid position for '%.*s'",kwc,kw)
    if (sr_encode_u8(dst,0x06)<0) return -1;
    SCALAR(rate,0.0,256.0)
    int v=(int)(rate*256.0);
    if (v<0) v=0;
    else if (v>0xffff) v=0xffff;
    if (sr_encode_intbe(dst,v,2)<0) return -1;
    SCALAR(depth,0.0,65535.0)
    v=(int)depth;
    if (sr_encode_intbe(dst,v,2)<0) return -1;
    (*state)|=STATE_freqlfo|STATE_ANYTHING;
    return 0;
  }
  
  if ((kwc==5)&&!memcmp(kw,"level",5)) {
    if (sr_encode_u8(dst,0x07)<0) return -1;
    if (sfg_compile_env(dst,src+srcp,srcc-srcp,1.0,*master,refname,lineno)<0) return -1;
    (*state)|=STATE_PIPELINE|STATE_ANYTHING|STATE_level;
    return 0;
  }
  
  if ((kwc==10)&&!memcmp(kw,"waveshaper",10)) {
    if (sr_encode_u8(dst,0x08)<0) return -1;
    int cp=dst->c,c=0;
    if (sr_encode_u8(dst,0)<0) return -1;
    while (srcp<srcc) {
      SCALAR(coef,-1.0,1.0)
      int v=(int)(coef*128.0);
      if (v<-128) v=-128;
      else if (v>127) v=127;
      if (sr_encode_u8(dst,v)<0) return -1;
      c++;
      if (c>257) FAIL("Limit 257 coefficients for waveshaper")
    }
    if (c==0) {
      if (sr_encode_u8(dst,0x80)<0) return -1;
      if (sr_encode_u8(dst,0x7f)<0) return -1;
      c=2;
    } else if (c==1) {
      if (sr_encode_u8(dst,((uint8_t*)dst->v)[dst->c-1])<0) return -1;
      c=2;
    }
    ((uint8_t*)dst->v)[cp]=c-2;
    (*state)|=STATE_PIPELINE|STATE_ANYTHING;
    return 0;
  }
  
  if ((kwc==4)&&!memcmp(kw,"gain",4)) {
    if (sr_encode_u8(dst,0x09)<0) return -1;
    SCALAR(gain,0.0,256.0)
    int v=(int)(gain*256.0);
    if (v<0) v=0;
    else if (v>0xffff) v=0xffff;
    if (sr_encode_intbe(dst,v,2)<0) return -1;
    if (srcp>=srcc) {
      if (sr_encode_u8(dst,0xff)<0) return -1;
    } else {
      SCALAR(clip,0.0,1.0)
      v=(int)(clip*256.0);
      if (v<0) v=0;
      else if (v>0xff) v=0xff;
      if (sr_encode_u8(dst,v)<0) return -1;
    }
    if (srcp>=srcc) {
      if (sr_encode_u8(dst,0x00)<0) return -1;
    } else {
      SCALAR(gate,0.0,1.0)
      v=(int)(gate*256.0);
      if (v<0) v=0;
      else if (v>0xff) v=0xff;
      if (sr_encode_u8(dst,v)<0) return -1;
    }
    (*state)|=STATE_PIPELINE|STATE_ANYTHING;
    return 0;
  }
  
  if ((kwc==5)&&!memcmp(kw,"delay",5)) {
    if (sr_encode_u8(dst,0x0a)<0) return -1;
    SCALAR(ms,0.0,1024.0)
    int v=(int)ms/4; if (v<0) v=0; else if (v>0xff) v=0xff;
    if (sr_encode_u8(dst,v)<0) return -1;
    SCALAR(dry,0.0,1.0)
    v=(int)(dry*256.0); if (v<0) v=0; else if (v>0xff) v=0xff;
    if (sr_encode_u8(dst,v)<0) return -1;
    SCALAR(wet,0.0,1.0)
    v=(int)(wet*256.0); if (v<0) v=0; else if (v>0xff) v=0xff;
    if (sr_encode_u8(dst,v)<0) return -1;
    SCALAR(store,0.0,1.0)
    v=(int)(store*256.0); if (v<0) v=0; else if (v>0xff) v=0xff;
    if (sr_encode_u8(dst,v)<0) return -1;
    SCALAR(feedback,0.0,1.0)
    v=(int)(feedback*256.0); if (v<0) v=0; else if (v>0xff) v=0xff;
    if (sr_encode_u8(dst,v)<0) return -1;
    (*state)|=STATE_PIPELINE|STATE_ANYTHING;
    return 0;
  }
  
  if ((kwc==6)&&!memcmp(kw,"detune",6)) {
    if (sr_encode_u8(dst,0x0b)<0) return -1;
    SCALAR(ms,0.0,1024.0)
    int v=(int)ms/4; if (v<0) v=0; else if (v>0xff) v=0xff;
    if (sr_encode_u8(dst,v)<0) return -1;
    SCALAR(depth,0.0,1.0)
    v=(int)(depth*256.0); if (v<0) v=0; else if (v>0xff) v=0xff;
    if (sr_encode_u8(dst,v)<0) return -1;
    (*state)|=STATE_PIPELINE|STATE_ANYTHING;
    return 0;
  }
  
  if ((kwc==6)&&!memcmp(kw,"lopass",6)) {
    if (sr_encode_u8(dst,0x0c)<0) return -1;
    SCALAR(hz,0.0,65535.0)
    int v=(int)hz; if (v<0) v=0; else if (v>0xffff) v=0xffff;
    if (sr_encode_intbe(dst,v,2)<0) return -1;
    (*state)|=STATE_PIPELINE|STATE_ANYTHING;
    return 0;
  }
  
  if ((kwc==6)&&!memcmp(kw,"hipass",6)) {
    if (sr_encode_u8(dst,0x0d)<0) return -1;
    SCALAR(hz,0.0,65535.0)
    int v=(int)hz; if (v<0) v=0; else if (v>0xffff) v=0xffff;
    if (sr_encode_intbe(dst,v,2)<0) return -1;
    (*state)|=STATE_PIPELINE|STATE_ANYTHING;
    return 0;
  }
  
  if ((kwc==5)&&!memcmp(kw,"bpass",5)) {
    if (sr_encode_u8(dst,0x0e)<0) return -1;
    SCALAR(hz,0.0,65535.0)
    int v=(int)hz; if (v<0) v=0; else if (v>0xffff) v=0xffff;
    if (sr_encode_intbe(dst,v,2)<0) return -1;
    SCALAR(width,0.0,65535.0)
    v=(int)width; if (v<0) v=0; else if (v>0xffff) v=0xffff;
    if (sr_encode_intbe(dst,v,2)<0) return -1;
    (*state)|=STATE_PIPELINE|STATE_ANYTHING;
    return 0;
  }
  
  if ((kwc==5)&&!memcmp(kw,"notch",5)) {
    if (sr_encode_u8(dst,0x0f)<0) return -1;
    SCALAR(hz,0.0,65535.0)
    int v=(int)hz; if (v<0) v=0; else if (v>0xffff) v=0xffff;
    if (sr_encode_intbe(dst,v,2)<0) return -1;
    SCALAR(width,0.0,65535.0)
    v=(int)width; if (v<0) v=0; else if (v>0xffff) v=0xffff;
    if (sr_encode_intbe(dst,v,2)<0) return -1;
    (*state)|=STATE_PIPELINE|STATE_ANYTHING;
    return 0;
  }
  
  FAIL("Unknown command '%.*s'",kwc,kw)
  #undef SCALAR
  return -1;
}

/* Compile, main entry point.
 */
 
int sfg_compile(struct sr_encoder *dst,const char *src,int srcc,const char *refname) {
  if (!dst) return -1;
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  if (sr_encode_raw(dst,"\0SFG",4)<0) return -1;
  struct sr_decoder decoder={.v=src,.c=srcc};
  int lineno=1,linec,pvindex=0,err;
  int lenp=0; // If zero, we are at global scope.
  const char *name=0;
  int namec=0;
  double master=1.0;
  int state=0;
  const char *line;
  for (;(linec=sr_decode_line(&line,&decoder))>0;lineno++) {
    while (linec&&((unsigned char)line[linec-1]<=0x20)) linec--;
    while (linec&&((unsigned char)line[0]<=0x20)) { line++; linec--; }
    if (!linec||(line[0]=='#')) continue;
    
    if (lenp) {
      if ((linec==3)&&!memcmp(line,"end",3)) {
        int len=dst->c-lenp-2;
        if ((len<0)||(len>0xffff)) FAIL("Invalid block length %d",len)
        ((uint8_t*)dst->v)[lenp]=len>>8;
        ((uint8_t*)dst->v)[lenp+1]=len;
        lenp=0;
        continue;
      }
      if ((err=sfg_compile_line(dst,line,linec,&state,&master,pvindex,name,namec,refname,lineno))<0) return err;
      
    } else {
      const char *kw=line;
      int kwc=0,linep=0;
      while ((linep<linec)&&((unsigned char)line[linep++]>0x20)) kwc++;
      while ((linep<linec)&&((unsigned char)line[linep]<=0x20)) linep++;
      const char *ixtoken=line+linep;
      int ixtokenc=0;
      while ((linep<linec)&&((unsigned char)line[linep++]>0x20)) ixtokenc++;
      while ((linep<linec)&&((unsigned char)line[linep]<=0x20)) linep++;
      name=line+linep;
      namec=linec-linep;
      int index;
      if ((sr_int_eval(&index,ixtoken,ixtokenc)<2)||(index<=pvindex)||(index>0xffff)||(kwc!=5)||memcmp(kw,"begin",5)) {
        FAIL("Expected 'begin INDEX [NAME]' with INDEX in %d..65535, found '%.*s'",pvindex+1,linec,line)
      }
      int dindex=index-pvindex;
      while (dindex>256) { // Emit dummies until we reach the desired index. sic '>' not '>='
        if (sr_encode_raw(dst,"\xff\0\0",3)<0) return -1;
        dindex-=256;
      }
      if (sr_encode_u8(dst,dindex-1)<0) return -1;
      lenp=dst->c; // Stash the length position and emit a dummy.
      if (sr_encode_raw(dst,"\0\0",2)<0) return -1;
      master=1.0;
      pvindex=index;
      state=0;
    }
  }
  if (lenp) {
    FAIL("Unclosed sound block for index %d '%.*s'",pvindex,namec,name)
  }
  return 0;
}
