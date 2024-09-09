#include "synth_formats.h"
#include "opt/serial/serial.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

#define FAIL(fmt,...) { \
  if (!path) return -1; \
  fprintf(stderr,"%s:%d: "fmt"\n",path,lineno,##__VA_ARGS__); \
  return -2; \
}

/* Note id.
 */
 
static int synth_noteid_eval(const char *src,int srcc) {
  if (!src) return -1;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  if (!srcc) return -1;
  
  // Friendly form: [a-gA-G][#sb]?(-1|[0-9])
  // MIDI notes run from 0=c-1 thru 127=g9
  // Beware! Octaves run C,D,E,F,G,A,B, not A,B,C,D,E,F,G.
  // That's what happens when you put musicians in charge of defining a spec.
  int tone=-1;
  switch (src[0]) {
    case 'c': case 'C': tone=0; break;
    case 'd': case 'D': tone=2; break;
    case 'e': case 'E': tone=4; break;
    case 'f': case 'F': tone=5; break;
    case 'g': case 'G': tone=7; break;
    case 'a': case 'A': tone=9; break;
    case 'b': case 'B': tone=11; break;
  }
  if (tone>=0) {
    int srcp=1;
    if (srcp<srcc) {
      if ((src[srcp]=='#')||(src[srcp]=='s')) { tone++; srcp++; }
      else if (src[srcp]=='b') { tone--; srcp++; }
    }
    int octave=-2;
    if ((srcp==srcc-2)&&(src[srcp]=='-')&&(src[srcp+1]=='1')) {
      octave=-1;
      srcp=srcc;
    } else if ((srcp==srcc-1)&&(src[srcp]>='0')&&(src[srcp]<='9')) {
      octave=src[srcp]-'0';
      srcp=srcc;
    }
    if ((srcp==srcc)&&(octave>=-1)) {
      int noteid=(octave+1)*12+tone;
      if ((noteid>=0)&&(noteid<0x80)) return noteid;
    }
  }
  
  // Also allow plain integers in 0..127.
  int noteid=-1;
  if (sr_int_eval(&noteid,src,srcc)<2) return -1;
  if ((noteid>=0)&&(noteid<0x80)) return noteid;
  return -1;
}

/* Named wave shapes, also we allow numbers 0..255.
 */
 
static int synth_shape_eval(const char *src,int srcc) {
  if (!src) return -1;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  if ((srcc==4)&&!memcmp(src,"sine",4)) return 0;
  if ((srcc==6)&&!memcmp(src,"square",6)) return 1;
  if ((srcc==3)&&!memcmp(src,"saw",3)) return 0;
  if ((srcc==8)&&!memcmp(src,"triangle",8)) return 0;
  int v;
  if ((sr_int_eval(&v,src,srcc)>=2)&&(v>=0)&&(v<0x100)) return v;
  return -1;
}

/* Compile envelope in beeeeep header command.
 * Input: VALUE [MS VALUE...] [.. VALUE [MS VALUE...]]
 * One VALUE may have a star after it to denote sustain point -- must be the same index on both sides.
 * (srcrange) negative if signed. We'll emit biased such that 0x8000 means 0.0.
 */
 
static int synth_beeeeep_compile_env(
  struct sr_encoder *dst,
  const char *src,int srcc,
  double srcrange,
  const char *path,int lineno
) {
  #define ENV_POINT_LIMIT 32
  struct env_point {
    uint16_t mslo,mshi;
    uint16_t vlo,vhi;
  } pointv[ENV_POINT_LIMIT];
  int pointc=0;
  int susp=-1;
  int srcp=0;
  while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
  
  #define RDMS(fld) { \
    const char *token=src+srcp; \
    int tokenc=0; \
    while ((srcp<srcc)&&((unsigned char)src[srcp++]>0x20)) tokenc++; \
    while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++; \
    int v; \
    if ((sr_int_eval(&v,token,tokenc)<2)||(v<0)||(v>0xffff)) { \
      FAIL("Expected envelope point delay in 0..65535 (ms), found '%.*s'",tokenc,token) \
    } \
    fld=v; \
  }
  // RDVALUE evaluates true if it's marked as the sustain point.
  #define RDVALUE(fld) ({ \
    const char *token=src+srcp; \
    int tokenc=0; \
    while ((srcp<srcc)&&((unsigned char)src[srcp++]>0x20)) tokenc++; \
    while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++; \
    int _result=0; \
    if ((tokenc>=1)&&(token[tokenc-1]=='*')) { _result=1; tokenc--; } \
    double f; \
    int v; \
    if (srcrange<0.0) { \
      if ((sr_double_eval(&f,token,tokenc)<0)||(f<srcrange)||(f>-srcrange)) { \
        FAIL("Expected envelope value in %.0f..%.0f, found '%.*s'",srcrange,-srcrange,tokenc,token) \
      } \
      v=(int)(((f-srcrange)*32768.0)/-srcrange); \
    } else { \
      if ((sr_double_eval(&f,token,tokenc)<0)||(f<0.0)||(f>srcrange)) { \
        FAIL("Expected envelope value in 0..%.0f, found '%.*s'",srcrange,tokenc,token) \
      } \
      v=(int)((f*65536.0)/srcrange); \
    } \
    if (v<0) v=0; else if (v>0xffff) v=0xffff; \
    fld=v; \
    _result; \
  })
  
  // Start with the initial lo value.
  pointv[0].mslo=0;
  if (RDVALUE(pointv[0].vlo)) susp=0;
  pointc=1;
  
  // Add points, lo side only.
  for (;;) {
    if (srcp>srcc-2) break;
    if ((src[srcp]=='.')&&(src[srcp+1]=='.')) break;
    if (pointc>=ENV_POINT_LIMIT) FAIL("Too many envelope points, artificial limit of %d.",ENV_POINT_LIMIT)
    RDMS(pointv[pointc].mslo)
    if (RDVALUE(pointv[pointc].vlo)) {
      if (susp>=0) FAIL("Multiple sustain points in envelope.")
      susp=pointc;
    }
    pointc++;
  }
  
  // If the next token is "..", read the hi side.
  if ((srcp<=srcc-2)&&(src[srcp]=='.')&&(src[srcp+1]=='.')) {
    srcp+=2;
    while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
    int hisusp=-1;
    pointv[0].mshi=0;
    if (RDVALUE(pointv[0].vhi)) hisusp=0;
    int hic=1;
    for (;;) {
      if (srcp>=srcc) break;
      if (hic>=ENV_POINT_LIMIT) FAIL("Too many envelope points, artificial limit of %d.",ENV_POINT_LIMIT)
      RDMS(pointv[hic].mshi)
      if (RDVALUE(pointv[hic].vhi)) {
        if (hisusp>=0) FAIL("Multiple sustain points in envelope.")
        hisusp=hic;
      }
      hic++;
    }
    if (pointc!=hic) FAIL("Envelope lo and hi must be the same length, have %d and %d.",pointc,hic)
    if (susp<0) susp=hisusp;
    else if (hisusp<0) ;
    else if (susp!=hisusp) FAIL("Different index indicated for sustain point lo vs hi. Mark the same point, or omit one side.")
    
  // No hi side, so duplicate all the lo values.
  } else {
    struct env_point *point=pointv;
    int i=pointc;
    for (;i-->0;point++) {
      point->mshi=point->mslo;
      point->vhi=point->vlo;
    }
  }
  
  // Encode it.
  if (sr_encode_intbe(dst,pointv[0].vlo,2)<0) return -1;
  if (sr_encode_intbe(dst,pointv[0].vhi,2)<0) return -1;
  if (sr_encode_u8(dst,susp)<0) return -1;
  int i=1; for (;i<pointc;i++) {
    if (sr_encode_intbe(dst,pointv[i].mslo,2)<0) return -1;
    if (sr_encode_intbe(dst,pointv[i].mshi,2)<0) return -1;
    if (sr_encode_intbe(dst,pointv[i].vlo,2)<0) return -1;
    if (sr_encode_intbe(dst,pointv[i].vhi,2)<0) return -1;
  }
  
  #undef RDMS
  #undef RDVALUE
  #undef ENV_POINT_LIMIT
  return srcp;
}

/* Compile one Channel Header line to beeeeep from text.
 * Caller must hold on to a synth_beeeeep_chctx and zero it at the start of each Channel Header.
 * The format is largely context-free: Each line in the text becomes one instruction appended to the binary.
 * But there are some extra requirements around uniqueness and order, we need the context to enforce those.
 */
 
struct synth_beeeeep_chctx {
  int TODO;
};
 
static int synth_beeeeep_compile_header_line(
  struct sr_encoder *dst,
  struct synth_beeeeep_chctx *chctx,
  const char *kw,int kwc,
  const char *args,int argsc,
  const char *path,int lineno
) {
  int argsp=0,lenp=-1;
  while ((argsp<argsc)&&((unsigned char)args[argsp]<=0x20)) argsp++;
  #define RDTOKEN const char *token=args+argsp; int tokenc=0; { \
    while ((argsp<argsc)&&((unsigned char)args[argsp++]>0x20)) tokenc++; \
    while ((argsp<argsc)&&((unsigned char)args[argsp]<=0x20)) argsp++; \
  }
  #define ARG_U08(name) { \
    RDTOKEN \
    double f; \
    if ((sr_double_eval(&f,token,tokenc)<0)||(f<0.0)||(f>1.0)) { \
      FAIL("Expected 0..1 for %s, found '%.*s'",name,tokenc,token) \
    } \
    int i=lround(f*255.0); \
    if (i<0) i=0; else if (i>0xff) i=0xff; \
    if (sr_encode_u8(dst,i)<0) return -1; \
  }
  #define ARG_U88(name) { \
    RDTOKEN \
    double f; \
    if ((sr_double_eval(&f,token,tokenc)<0)||(f<0.0)||(f>256.0)) { \
      FAIL("Expected 0.0..256.0 for %s, found '%.*s'",name,tokenc,token) \
    } \
    int i=lround(f*255.0); \
    if (i<0) i=0; else if (i>0xffff) i=0xffff; \
    if (sr_encode_intbe(dst,i,2)<0) return -1; \
  }
  #define ARG_U016(name) { \
    RDTOKEN \
    double f; \
    if ((sr_double_eval(&f,token,tokenc)<0)||(f<0.0)||(f>1.0)) { \
      FAIL("Expected 0..1 for %s, found '%.*s'",name,tokenc,token) \
    } \
    int i=lround(f*65535.0); \
    if (i<0) i=0; else if (i>0xffff) i=0xffff; \
    if (sr_encode_intbe(dst,i,2)<0) return -1; \
  }
  #define ARG_S016(name) { \
    RDTOKEN \
    double f; \
    if ((sr_double_eval(&f,token,tokenc)<0)||(f<-1.0)||(f>1.0)) { \
      FAIL("Expected -1..1 for %s, found '%.*s'",name,tokenc,token) \
    } \
    int i=lround(f*32767.0); \
    if (i<-32768) i=-32768; else if (i>32767) i=32767; \
    if (sr_encode_intbe(dst,i,2)<0) return -1; \
  }
  #define ARG_U16(name) { \
    RDTOKEN \
    int v; \
    if ((sr_int_eval(&v,token,tokenc)<2)||(v<0)||(v>0xffff)) { \
      FAIL("Expected 0..65535 for %s, found '%.*s'",name,tokenc,token) \
    } \
    if (sr_encode_intbe(dst,v,2)<0) return -1; \
  }
  #define ARG_S8(name) { \
    RDTOKEN \
    int v; \
    if ((sr_int_eval(&v,token,tokenc)<2)||(v<-128)||(v>127)) { \
      FAIL("Expected -128..127 for %s, found '%.*s'",name,tokenc,token) \
    } \
    if (sr_encode_u8(dst,v)<0) return -1; \
  }
  #define START(opcode) { \
    if (sr_encode_u8(dst,opcode)<0) return -1; \
    lenp=dst->c; \
    if (sr_encode_u8(dst,0)<0) return -1; \
  }
  #define FINISH { \
    if (lenp<0) return -1; \
    int len=dst->c-lenp-1; \
    if (len>0xff) FAIL("'%.*s' too long (%d, limit 255)",kwc,kw,len) \
    ((uint8_t*)dst->v)[lenp]=len; \
    if (argsp<argsc) FAIL("Unexpected tokens after '%.*s' command: '%.*s'",kwc,kw,argsc-argsp,args+argsp) \
    return 0; \
  }

  if ((kwc==6)&&!memcmp(kw,"master",6)) {
    START(0x01)
    ARG_U08("trim")
    FINISH
  }
  
  if ((kwc==3)&&!memcmp(kw,"pan",3)) {
    // pan has a special OOB value "mono"=>0
    START(0x02)
    RDTOKEN
    if ((tokenc==4)&&!memcmp(token,"mono",4)) {
      if (sr_encode_u8(dst,0)<0) return -1;
    } else {
      double f;
      if ((sr_double_eval(&f,token,tokenc)<0)||(f<-1.0)||(f>1.0)) {
        FAIL("Expected -1..1 or 'mono' for pan, found '%.*s'",tokenc,token)
      }
      int i=(int)((f+1.0)*128.0);
      if (i<1) i=1; else if (i>0xff) i=0xff;
      if (sr_encode_u8(dst,i)<0) return -1;
    }
    FINISH
  }
  
  if ((kwc==5)&&!memcmp(kw,"drums",5)) {
    START(0x03)
    ARG_U16("rid")
    ARG_S8("bias")
    ARG_U08("trimlo")
    ARG_U08("trimhi")
    FINISH
  }
  
  if ((kwc==5)&&!memcmp(kw,"wheel",5)) {
    START(0x04)
    ARG_U16("cents")
    FINISH
  }
  
  if ((kwc==3)&&!memcmp(kw,"sub",3)) {
    START(0x05)
    ARG_U16("width (hz)")
    FINISH
  }
  
  if ((kwc==5)&&!memcmp(kw,"shape",5)) {
    START(0x06)
    RDTOKEN
    int v=synth_shape_eval(token,tokenc);
    if (v<0) FAIL("Expected 'sine', 'square', 'saw', 'triangle', or 0..255, found '%.*s'",tokenc,token)
    if (sr_encode_u8(dst,v)<0) return -1;
    FINISH
  }
  
  if ((kwc==9)&&!memcmp(kw,"harmonics",9)) {
    START(0x07)
    while (argsp<argsc) {
      ARG_U016("coefficient")
    }
    FINISH
  }
  
  if ((kwc==2)&&!memcmp(kw,"fm",2)) {
    START(0x08)
    ARG_U88("fm rate")
    ARG_U88("fm range")
    FINISH
  }
  
  if ((kwc==5)&&!memcmp(kw,"fmenv",5)) {
    START(0x09)
    int err=synth_beeeeep_compile_env(dst,args+argsp,argsc-argsp,1.0,path,lineno);
    if (err<0) return err;
    argsp+=err;
    FINISH
  }
  
  if ((kwc==5)&&!memcmp(kw,"fmlfo",5)) {
    START(0x0a)
    ARG_U16("period (ms)")
    ARG_U08("depth")
    ARG_U08("phase")
    FINISH
  }
  
  if ((kwc==8)&&!memcmp(kw,"pitchenv",8)) {
    START(0x0b)
    int err=synth_beeeeep_compile_env(dst,args+argsp,argsc-argsp,-2400.0,path,lineno);
    if (err<0) return err;
    argsp+=err;
    FINISH
  }
  
  if ((kwc==8)&&!memcmp(kw,"pitchlfo",8)) {
    START(0x0c)
    ARG_U16("period (ms)")
    ARG_U16("cents")
    ARG_U08("phase")
    FINISH
  }
  
  if ((kwc==5)&&!memcmp(kw,"level",5)) {
    START(0x0d)
    int err=synth_beeeeep_compile_env(dst,args+argsp,argsc-argsp,1.0,path,lineno);
    if (err<0) return err;
    argsp+=err;
    FINISH
  }
  
  if ((kwc==8)&&!memcmp(kw,"levellfo",8)) {
    START(0x0e)
    ARG_U16("period (ms)")
    ARG_U08("depth")
    ARG_U08("phase")
    FINISH
  }
  
  if ((kwc==4)&&!memcmp(kw,"gain",4)) {
    START(0x80)
    ARG_U88("gain")
    ARG_U08("clip")
    ARG_U08("gate")
    FINISH
  }
  
  if ((kwc==10)&&!memcmp(kw,"waveshaper",10)) {
    START(0x81)
    int ptc=0;
    while (argsp<argsc) {
      ARG_S016("level")
      ptc++;
    }
    if (ptc<2) FAIL("'waveshaper' must have at least 2 control points")
    FINISH
  }
  
  if ((kwc==5)&&!memcmp(kw,"delay",5)) {
    START(0x82)
    ARG_U16("period (ms)")
    ARG_U08("dry")
    ARG_U08("wet")
    ARG_U08("store")
    ARG_U08("feedback")
    FINISH
  }
  
  if ((kwc==6)&&!memcmp(kw,"detune",6)) {
    START(0x83)
    ARG_U16("period (ms)")
    ARG_U16("cents")
    ARG_U08("phase")
    FINISH
  }
  
  if ((kwc==7)&&!memcmp(kw,"tremolo",7)) {
    START(0x84)
    ARG_U16("period (ms)")
    ARG_U016("depth")
    ARG_U08("phase")
    FINISH
  }
  
  if ((kwc==6)&&!memcmp(kw,"lopass",6)) {
    START(0x85)
    ARG_U16("cutoff (hz)")
    FINISH
  }
  
  if ((kwc==6)&&!memcmp(kw,"hipass",6)) {
    START(0x86)
    ARG_U16("cutoff (hz)")
    FINISH
  }
  
  if ((kwc==5)&&!memcmp(kw,"bpass",5)) {
    START(0x87)
    ARG_U16("center (hz)")
    ARG_U16("width (hz)")
    FINISH
  }
  
  if ((kwc==5)&&!memcmp(kw,"notch",5)) {
    START(0x88)
    ARG_U16("center (hz)")
    ARG_U16("width (hz)")
    FINISH
  }
  
  // Finally, we accept raw hex dumps. The "keyword" must be a token of two chars. After that, we only require an even total length.
  if (kwc==2) {
    int hi=sr_digit_eval(kw[0]);
    int lo=sr_digit_eval(kw[1]);
    if ((hi>=0)&&(hi<16)&&(lo>=0)&&(lo<16)) {
      START((hi<<4)|lo)
      hi=-1;
      for (;argsp<argsc;argsp++) {
        char ch=args[argsp];
        if ((unsigned char)ch<=0x20) continue;
        int digit=sr_digit_eval(ch);
        if ((digit<0)||(digit>=16)) goto _not_hexdump_;
        if (hi<0) hi=digit;
        else {
          if (sr_encode_u8(dst,(hi<<4)|digit)<0) return -1;
          hi=-1;
        }
      }
      if (hi>=0) goto _not_hexdump_;
      FINISH
    }
   _not_hexdump_:;
  }

  #undef RDTOKEN
  #undef ARG_U08
  #undef ARG_U88
  #undef ARG_U016
  #undef ARG_S016
  #undef ARG_U16
  #undef ARG_S8
  #undef START
  #undef FINISH
  FAIL("Unexpected command '%.*s'",kwc,kw)
}

/* Binary song or single sound from text.
 */
 
static int synth_beeeeep_from_text(struct sr_encoder *dst,const char *src,int srcc,const char *path,int lineno) {
  if (sr_encode_raw(dst,"\xbe\xee\xeep",4)<0) return -1;
  struct sr_decoder decoder={.v=src,.c=srcc};
  const char *line;
  int linec;
  int stage=-1; // -1:init, -2:events, >=0:channel
  int chlenp=0; // Position in (dst) of channel header length, when (stage>=0).
  int chc=0;
  struct synth_beeeeep_chctx chctx={0};
  
  #define ENDCHANNEL if (stage>=0) { \
    /*TODO Validate context. */ \
    int chlen=dst->c-chlenp-2; \
    if (chlen<=0) { \
      /* Emit an End of Header byte so the header's length isn't zero. */ \
      if (sr_encode_u8(dst,0x00)<0) return -1; \
      chlen=1; \
    } \
    if (chlen>0xffff) FAIL("Channel Header too long (%d, limit 65535)",chlen) \
    ((uint8_t*)dst->v)[chlenp]=chlen>>8; \
    ((uint8_t*)dst->v)[chlenp+1]=chlen; \
  }
  
  for (;(linec=sr_decode_line(&line,&decoder))>0;lineno++) {
    while (linec&&((unsigned char)line[linec-1]<=0x20)) linec--;
    while (linec&&((unsigned char)line[0]<=0x20)) { linec--; line++; }
    if (!linec) continue;
    if (line[0]=='#') continue;
    
    const char *kw=line;
    int linep=0,kwc=0;
    while ((linep<linec)&&((unsigned char)line[linep++]>0x20)) kwc++;
    while ((linep<linec)&&((unsigned char)line[linep]<=0x20)) linep++;
    const char *args=line+linep;
    int argsc=linec-linep;
    
    // "channel CHID" to begin a Channel Header.
    if ((kwc==7)&&!memcmp(kw,"channel",7)) {
      ENDCHANNEL
      if (stage<=-2) FAIL("'channel' not permitted after 'events'")
      int chid;
      if ((sr_int_eval(&chid,args,argsc)<2)||(chid<0)) FAIL("Expected channel id, found '%.*s'",argsc,args)
      if ((stage>=0)&&(chid<=stage)) FAIL("Channels out of order. Found %d but expected at least %d.",chid,stage+1)
      if (chid>=8) FAIL("Channel ID must be in 0..7, found %d",chid) // The format allows >=8 in headers, but not in events.
      stage++;
      while (stage<chid) { // Emit dummy Channel Headers if provided chid are sparse.
        if (sr_encode_raw(dst,"\0\1\0",3)<0) return -1;
        stage++;
      }
      chlenp=dst->c;
      if (sr_encode_raw(dst,"\0\0",2)<0) return -1;
      chc=stage+1;
      memset(&chctx,0,sizeof(chctx));
      continue;
    }
    
    // "events" to finalize Channel Headers and begin receiving Events.
    if ((kwc==6)&&!memcmp(kw,"events",6)) {
      if (argsc) FAIL("Unexpected argument to 'events'")
      if (stage<=-2) FAIL("Redundant 'events'")
      ENDCHANNEL
      if (sr_encode_raw(dst,"\0\0",2)<0) return -1; // Channel Headers terminator.
      stage=-2;
      // It's tempting to assert (chc>0) here, but let's allow that the sound might be a specific duration of silence.
      continue;
    }
    
    // In the outer scope, only "channel" and "events" are legal.
    if (stage==-1) FAIL("Expected 'channel' or 'events', found '%.*s'",kwc,kw)
    
    // In the Events block, there are three commands: delay, wheel, note.
    if (stage<=-2) {
      if ((kwc==5)&&!memcmp(kw,"delay",5)) {
        int ms;
        if ((sr_int_eval(&ms,args,argsc)<2)||(ms<1)) FAIL("Expected delay in milliseconds, found '%.*s'",argsc,args)
        if (ms>=10000) FAIL("Unreasonably long delay %d ms. Artificially limited to 9999.",ms)
        while (ms>=0x80) {
          if (sr_encode_u8(dst,0x7f)<0) return -1;
          ms-=0x7f;
        }
        if (sr_encode_u8(dst,ms)<0) return -1;
        continue;
      }
      // Wheel and Note both begin with chid.
      int chid;
      if ((sr_int_eval(&chid,kw,kwc)<2)||(chid<0)||(chid>=chc)) {
        FAIL("Expected 'delay' or channel ID (<%d), found '%.*s'",chc,argsc,args)
      }
      // Second token distinguishes Wheel from Note.
      const char *op=args;
      int opc=0,argsp=0;
      while ((argsp<argsc)&&((unsigned char)args[argsp++]>0x20)) opc++;
      while ((argsp<argsc)&&((unsigned char)args[argsp]<=0x20)) argsp++;
      if ((opc==5)&&!memcmp(op,"wheel",5)) {
        double v;
        if ((sr_double_eval(&v,args+argsp,argsc-argsp)<0)||(v<-1.0)||(v>1.0)) {
          FAIL("Expected wheel value in -1..1, found '%.*s'",argsc-argsp,args+argsp)
        }
        int vi=(int)(v*512.0);
        if (vi<-512) vi=-512;
        else if (vi>511) vi=511;
        if (sr_encode_u8(dst,0xe0|(chid<<2)|((vi>>8)&0x03))<0) return -1;
        if (sr_encode_u8(dst,vi)<0) return -1;
        continue;
      }
      // Note: "CHID NOTEID VELOCITY DURMS"
      int noteid=synth_noteid_eval(op,opc);
      if (noteid<0) FAIL("Expected 'wheel' or note, found '%.*s'",opc,op)
      op=args+argsp;
      opc=0;
      while ((argsp<argsc)&&((unsigned char)args[argsp++]>0x20)) opc++;
      while ((argsp<argsc)&&((unsigned char)args[argsp]<=0x20)) argsp++;
      int velocity;
      if ((sr_int_eval(&velocity,op,opc)<2)||(velocity<0)||(velocity>0x7f)) {
        FAIL("Expected velocity in 0..127, found '%.*s'",opc,op)
      }
      int durms=0;
      if (argsp<argsc) {
        if ((sr_int_eval(&durms,args+argsp,argsc-argsp)<2)||(durms<0)) {
          FAIL("Expected duration in milliseconds, found '%.*s'",argsc-argsp,args+argsp)
        }
      }
      if (!durms) {
        if (sr_encode_u8(dst,0x80|(chid<<2)|(noteid>>5))<0) return -1;
        if (sr_encode_u8(dst,(noteid<<3)|(velocity>>4))<0) return -1;
      } else if (durms<=0x100) {
        if (sr_encode_u8(dst,0xa0|(chid<<2)|(noteid>>5))<0) return -1;
        if (sr_encode_u8(dst,(noteid<<3)|(velocity>>4))<0) return -1;
        if (sr_encode_u8(dst,durms-1)<0) return -1;
      } else {
        durms-=0x100;
        durms=(durms+16)/32;
        durms-=1;
        if ((durms>0xff)&&path) {
          fprintf(stderr,"%s:%d:WARNING: Note duration too long. Clamping to %d ms.\n",path,lineno,256*33);
          durms=0xff;
        }
        if (sr_encode_u8(dst,0xc0|(chid<<2)|(noteid>>5))<0) return -1;
        if (sr_encode_u8(dst,(noteid<<3)|(velocity>>4))<0) return -1;
        if (sr_encode_u8(dst,durms)<0) return -1;
      }
      continue;
    }
    
    // We are in a Channel Header.
    int err=synth_beeeeep_compile_header_line(dst,&chctx,kw,kwc,args,argsc,path,lineno);
    if (err<0) return err;
  }
  ENDCHANNEL
  if (stage>-2) {
    if (sr_encode_raw(dst,"\0\0",2)<0) return -1; // Channel Headers terminator.
  }
  #undef ENDCHANNEL
  return 0;
}

/* Either binary format from text.
 */
 
int synth_egg_from_text(struct sr_encoder *dst,const void *src,int srcc,const char *path) {
  struct sr_decoder decoder={.v=src,.c=srcc};
  const char *line;
  int linec,lineno=1;
  
  // Skip lines until the first that isn't empty.
  for (;(linec=sr_decode_line(&line,&decoder))>0;lineno++) {
    while (linec&&((unsigned char)line[linec-1]<=0x20)) linec--;
    while (linec&&((unsigned char)line[0]<=0x20)) { linec--; line++; }
    if (!linec) continue;
    if (line[0]=='#') continue;
    break;
  }
  
  // If the whole line is "song", the remainder of input is a single sound.
  if ((linec==4)&&!memcmp(line,"song",4)) {
    return synth_beeeeep_from_text(dst,(char*)decoder.v+decoder.p,decoder.c-decoder.p,path,lineno);
  }
  
  // We'll be producing a multi-sound resource.
  if (sr_encode_raw(dst,"\0ESS",4)<0) return -1;
  int pvindex=0;
  while (linec>0) {
    
    const char *kw=line;
    int kwc=0,linep=0;
    while ((linep<linec)&&((unsigned char)line[linep++]>0x20)) kwc++;
    while ((linep<linec)&&((unsigned char)line[linep]<=0x20)) linep++;
    const char *ixtoken=line+linep;
    int ixtokenc=0;
    while ((linep<linec)&&((unsigned char)line[linep++]>0x20)) ixtokenc++;
    while ((linep<linec)&&((unsigned char)line[linep]<=0x20)) linep++;
    const char *name=line+linep;
    int namec=linec-linep;
    if ((kwc!=5)||memcmp(kw,"sound",5)) FAIL("Expected 'sound INDEX [NAME]'.")
    int index;
    if ((sr_int_eval(&index,ixtoken,ixtokenc)<2)||(index<=pvindex)||(index>0xffff)) {
      FAIL("Invalid sound index '%.*s', expected %d..65535.",ixtokenc,ixtoken,pvindex+1)
    }
    
    int d=index-pvindex;
    while (d>256) {
      if (sr_encode_raw(dst,"\xff\0\0",3)<0) return -1;
      d-=256;
    }
    if (d<1) return -1;
    if (sr_encode_u8(dst,d-1)<0) return -1;
    int lenp=dst->c;
    if (sr_encode_raw(dst,"\0\0",2)<0) return -1;
    pvindex=index;
    
    // Skip lines until the end or the next "sound".
    int srcp0=decoder.p;
    int lineno0=lineno+1;
    int introp=decoder.p;
    for (;(linec=sr_decode_line(&line,&decoder))>0;) {
      lineno++;
      introp=decoder.p-linec;
      while (linec&&((unsigned char)line[linec-1]<=0x20)) linec--;
      while (linec&&((unsigned char)line[0]<=0x20)) { linec--; line++; }
      if (!linec) continue;
      if (line[0]=='#') continue;
      if ((linec>5)&&!memcmp(line,"sound",5)&&((unsigned char)line[5]<=0x20)) break;
    }
    
    // Compile a beeeeep with whatever we skipped.
    const char *bsrc=(char*)decoder.v+srcp0;
    int bsrcc=introp-srcp0;
    int err=synth_beeeeep_from_text(dst,bsrc,bsrcc,path,lineno0);
    if (err<0) return err;
    int len=dst->c-lenp-2;
    if ((len<0)||(len>0xffff)) FAIL("Invalid sound length %d",len)
    uint8_t *lendst=(uint8_t*)dst->v+lenp;
    lendst[0]=len>>8;
    lendst[1]=len;
  }
  return 0;
}

/* Text from encoded envelope.
 */
 
static int synth_beeeeep_uncompile_env(struct sr_encoder *dst,const uint8_t *src,int srcc,double srcrange) {
  if (srcc<5) return -1;
  if ((srcc-5)&7) return -1;
  int ptc=(srcc-5)>>3;
  int susp=src[4];
  
  #define TIME(srcp) { \
    int v=(src[srcp]<<8)|src[(srcp)+1]; \
    if (sr_encode_fmt(dst," %d",v)<0) return -1; \
  }
  #define VALUE(p,index) { \
    int vi=(src[p]<<8)|src[(p)+1]; \
    double vf; \
    if (srcrange<0.0) { \
      vf=(double)(vi-0x8000)/32768.0; \
    } else { \
      vf=((double)vi*srcrange)/65536.0; \
    } \
    if (sr_encode_fmt(dst," %f",vf)<0) return -1; \
    if ((index)==susp) { \
      if (sr_encode_u8(dst,'*')<0) return -1; \
    } \
  }
  
  // Emit the lo side, simple enough.
  VALUE(0,0)
  int srcp=5,index=1;
  for (;srcp<srcc;srcp+=8,index++) {
    TIME(srcp)
    VALUE(srcp+4,index)
  }
  
  // Check for identical hi and lo. That's pretty common, and no sense emitting the hi side if identical.
  int identical=1;
  if ((src[0]!=src[2])||(src[1]!=src[3])) {
    identical=0;
  } else {
    for (srcp=5;srcp<srcc;srcp+=4) { // step by 4: time and value are distinct passes here
      if ((src[srcp]!=src[srcp+2])||(src[srcp+1]!=src[srcp+3])) {
        identical=0;
        break;
      }
    }
  }
  
  // Only if not identical, emit the hi side.
  if (!identical) {
    if (sr_encode_raw(dst," ..",3)<0) return -1;
    VALUE(2,0)
    for (srcp=7,index=1;srcp<srcc;srcp+=8,index++) { // Same as lo side but 2 bytes further in.
      TIME(srcp)
      VALUE(srcp+4,index)
    }
  }
  
  #undef VALUE
  return srcc;
}

/* Text from beeeeep channel header, not including the introducer line.
 */
 
static int synth_text_from_beeeeep_header(struct sr_encoder *dst,const uint8_t *src,int srcc,const char *path) {
  int srcp=0;
  
  #define ARG_U08 { \
    if (srcp>srcc-1) return -1; \
    double v=src[srcp++]/255.0; \
    if (sr_encode_fmt(dst," %f",v)<0) return -1; \
  }
  #define ARG_U88 { \
    if (srcp>srcc-2) return -1; \
    double v=((src[srcp]<<8)|src[srcp+1])/255.0; \
    srcp+=2; \
    if (sr_encode_fmt(dst," %f",v)<0) return -1; \
  }
  #define ARG_U016 { \
    if (srcp>srcc-2) return -1; \
    int v=(src[srcp]<<8)|src[srcp+1]; \
    srcp+=2; \
    if (sr_encode_fmt(dst," %f",(double)v/65535.0)<0) return -1; \
  }
  #define ARG_S016 { \
    if (srcp>srcc-2) return -1; \
    int16_t v=(src[srcp]<<8)|src[srcp+1]; \
    srcp+=2; \
    double vf=(double)v/32767.0; \
    if (vf<-1.0) vf=-1.0; else if (vf>1.0) vf=1.0; \
    if (sr_encode_fmt(dst," %f",vf)<0) return -1; \
  }
  #define ARG_S8 { \
    if (srcp>srcc-1) return -1; \
    int8_t v=src[srcp++]; \
    if (sr_encode_fmt(dst," %d",v)<0) return -1; \
  }
  #define ARG_U16 { \
    if (srcp>srcc-2) return -1; \
    int v=(src[srcp]<<8)|src[srcp+1]; \
    srcp+=2; \
    if (sr_encode_fmt(dst," %d",v)<0) return -1; \
  }
  
  while (srcp<srcc) {
    if (srcp>srcc-2) return -1;
    uint8_t opcode=src[srcp++];
    uint8_t len=src[srcp++];
    fprintf(stderr,"hdr 0x%02x len=%d @%d/%d\n",opcode,len,srcp,srcc);
    if (srcp>srcc-len) return -1;
    if (!opcode) break;
    int nextp=srcp+len;
    switch (opcode) {

      case 0x01: {
          if (sr_encode_raw(dst,"master",6)<0) return -1;
          ARG_U08
        } break;
        
      case 0x02: {
          if (sr_encode_raw(dst,"pan",3)<0) return -1;
          if (srcp>=srcc) return -1;
          if (src[srcp]==0) {
            if (sr_encode_raw(dst," mono",5)<0) return -1;
          } else {
            if (sr_encode_fmt(dst," %f",(src[srcp]-0x80)/128.0)<0) return -1;
          }
          srcp++;
        } break;
        
      case 0x03: {
          if (sr_encode_raw(dst,"drums",5)<0) return -1;
          ARG_U16
          ARG_S8
          ARG_U08
          ARG_U08
        } break;
        
      case 0x04: {
          if (sr_encode_raw(dst,"wheel",5)<0) return -1;
          ARG_U16
        } break;
        
      case 0x05: {
          if (sr_encode_raw(dst,"sub",3)<0) return -1;
          ARG_U16
        } break;
        
      case 0x06: {
          if (sr_encode_raw(dst,"shape",5)<0) return -1;
          if (srcp>=srcc) return -1;
          switch (src[srcp]) {
            case 0: if (sr_encode_raw(dst," sine",5)<0) return -1; break;
            case 1: if (sr_encode_raw(dst," square",7)<0) return -1; break;
            case 2: if (sr_encode_raw(dst," saw",4)<0) return -1; break;
            case 3: if (sr_encode_raw(dst," triangle",9)<0) return -1; break;
            default: if (sr_encode_fmt(dst," %d",src[srcp])<0) return -1; break;
          }
          srcp++;
        } break;
        
      case 0x07: {
          if (sr_encode_raw(dst,"harmonics",9)<0) return -1;
          if (len&1) return -1;
          int coefc=len>>1;
          while (coefc-->0) {
            ARG_U016
          }
        } break;
        
      case 0x08: {
          if (sr_encode_raw(dst,"fm",2)<0) return -1;
          ARG_U88
          ARG_U88
        } break;
        
      case 0x09: {
          if (sr_encode_raw(dst,"fmenv",5)<0) return -1;
          if (synth_beeeeep_uncompile_env(dst,src+srcp,len,1.0)<0) return -1;
        } break;
        
      case 0x0a: {
          if (sr_encode_raw(dst,"fmlfo",5)<0) return -1;
          ARG_U16
          ARG_U016
          ARG_U08
        } break;
        
      case 0x0b: {
          if (sr_encode_raw(dst,"pitchenv",8)<0) return -1;
          if (synth_beeeeep_uncompile_env(dst,src+srcp,len,-2400.0)<0) return -1;
        } break;
        
      case 0x0c: {
          if (sr_encode_raw(dst,"pitchlfo",8)<0) return -1;
          ARG_U16
          ARG_U16
          ARG_U08
        } break;
        
      case 0x0d: {
          if (sr_encode_raw(dst,"level",5)<0) return -1;
          if (synth_beeeeep_uncompile_env(dst,src+srcp,len,1.0)<0) return -1;
        } break;
        
      case 0x0e: {
          if (sr_encode_raw(dst,"levellfo",8)<0) return -1;
          ARG_U16
          ARG_U016
          ARG_U08
        } break;
        
      case 0x80: {
          if (sr_encode_raw(dst,"gain",4)<0) return -1;
          ARG_U88
          ARG_U08
          ARG_U08
        } break;
        
      case 0x81: {
          if (sr_encode_raw(dst,"waveshaper",10)<0) return -1;
          if (len&1) return -1;
          int i=len>>1;
          while (i-->0) {
            ARG_S016
          }
        } break;
        
      case 0x82: {
          if (sr_encode_raw(dst,"delay",5)<0) return -1;
          ARG_U16
          ARG_U08
          ARG_U08
          ARG_U08
          ARG_U08
        } break;
        
      case 0x83: {
          if (sr_encode_raw(dst,"detune",6)<0) return -1;
          ARG_U16
          ARG_U16
          ARG_U08
        } break;
        
      case 0x84: {
          if (sr_encode_raw(dst,"tremolo",7)<0) return -1;
          ARG_U16
          ARG_U016
          ARG_U08
        } break;
        
      case 0x85: {
          if (sr_encode_raw(dst,"lopass",6)<0) return -1;
          ARG_U16
        } break;
        
      case 0x86: {
          if (sr_encode_raw(dst,"hipass",6)<0) return -1;
          ARG_U16
        } break;
        
      case 0x87: {
          if (sr_encode_raw(dst,"bpass",5)<0) return -1;
          ARG_U16
          ARG_U16
        } break;
        
      case 0x88: {
          if (sr_encode_raw(dst,"notch",5)<0) return -1;
          ARG_U16
          ARG_U16
        } break;
      
      default: {
          // Unknown Channel Header opcode is OK. We can emit a generic hexdump.
          if (sr_encode_fmt(dst,"%02x",opcode)<0) return -1;
          int i=0; for (;i<len;i++) {
            if (sr_encode_fmt(dst," %02x",src[srcp+i])<0) return -1;
          }
        }
    }
    if (sr_encode_u8(dst,0x0a)<0) return -1;
    srcp=nextp;
  }
  #undef ARG_U08
  #undef ARG_U88
  #undef ARG_U016
  #undef ARG_S016
  #undef ARG_S8
  #undef ARG_U16
  return 0;
}

/* Text from beeeeep, no introducer.
 */
 
static int synth_text_from_beeeeep(struct sr_encoder *dst,const uint8_t *src,int srcc,const char *path) {
  fprintf(stderr,"%s srcc=%d...\n",__func__,srcc);
  if ((srcc<4)||memcmp(src,"\xbe\xee\xeep",4)) return -1;
  int srcp=4,err;
  
  // Channel Headers.
  int chid=-1;
  while (srcp<=srcc-2) {
    chid++;
    int len=(src[srcp]<<8)|src[srcp+1];
    srcp+=2;
    if (!len) break;
    if (srcp>srcc-len) return -1;
    if (!len) continue;
    fprintf(stderr,"channel headers chid=%d len=%d...\n",chid,len);
    if (sr_encode_fmt(dst,"channel %d\n",chid)<0) return -1;
    if ((err=synth_text_from_beeeeep_header(dst,src+srcp,len,path))<0) return err;
    srcp+=len;
  }
  
  // Events.
  if (sr_encode_raw(dst,"events\n",7)<0) return -1;
  while (srcp<srcc) {
    uint8_t lead=src[srcp++];
    fprintf(stderr,"event 0x%02x\n",lead);
    if (!lead) break; // End of Song.
    if (!(lead&0x80)) {
      if (sr_encode_fmt(dst,"delay %d\n",lead)<0) return -1;
      continue;
    }
    switch (lead&0xe0) {
      case 0x80: {
          if (srcp>srcc-1) return -1;
          uint8_t c1=src[srcp++];
          int chid=(lead>>2)&7;
          int noteid=((lead&3)<<5)|(c1>>3);
          int velocity=(c1&7)<<4;
          velocity|=velocity>>3;
          velocity|=velocity>>6;
          if (sr_encode_fmt(dst,"%d %d %d 0\n",chid,noteid,velocity)<0) return -1;
        } break;
      case 0xa0: {
          if (srcp>srcc-2) return -1;
          uint8_t c1=src[srcp++];
          uint8_t c2=src[srcp++];
          int chid=(lead>>2)&7;
          int noteid=((lead&3)<<5)|(c1>>3);
          int velocity=(c1&7)<<4;
          velocity|=velocity>>3;
          velocity|=velocity>>6;
          int dur=c2+1;
          if (sr_encode_fmt(dst,"%d %d %d %d\n",chid,noteid,velocity,dur)<0) return -1;
        } break;
      case 0xc0: {
          if (srcp>srcc-2) return -1;
          uint8_t c1=src[srcp++];
          uint8_t c2=src[srcp++];
          int chid=(lead>>2)&7;
          int noteid=((lead&3)<<5)|(c1>>3);
          int velocity=(c1&7)<<4;
          velocity|=velocity>>3;
          velocity|=velocity>>6;
          int dur=(c2+1)*32+256;
          if (sr_encode_fmt(dst,"%d %d %d %d\n",chid,noteid,velocity,dur)<0) return -1;
        } break;
      case 0xe0: { // 111cccvv vvvvvvvv          : Pitch Wheel.
          if (srcp>srcc-1) return -1;
          uint8_t c1=src[srcp++];
          int chid=(lead>>2)&7;
          int v=((lead&3)<<8)|c1;
          if (v&0x200) v|=~0x3ff; // s10
          double vf=(double)v/512.0;
          if (vf<-1.0) vf=-1.0; else if (vf>1.0) vf=1.0;
          if (sr_encode_fmt(dst,"%d wheel %f\n",chid,vf)<0) return -1;
        } break;
    }
  }
  
  return 0;
}

/* Text from either binary format.
 */

int synth_text_from_egg(struct sr_encoder *dst,const void *src,int srcc,const char *path) {

  if ((srcc>=4)&&!memcmp(src,"\xbe\xee\xeep",4)) {
    if (sr_encode_raw(dst,"song\n",5)<0) return -1;
    return synth_text_from_beeeeep(dst,src,srcc,path);
  }
  
  if ((srcc>=4)&&!memcmp(src,"\0ESS",4)) {
    int srcp=4,index=0;
    const uint8_t *SRC=src;
    while (srcp<srcc) {
      int delta=SRC[srcp++];
      if (srcp>srcc-2) return -1;
      int len=(SRC[srcp]<<8)|SRC[srcp+1];
      srcp+=2;
      if (srcp>srcc-len) return -1;
      index+=delta+1;
      if (index>0xffff) return -1;
      if (sr_encode_fmt(dst,"\nsound %d\n",index)<0) return -1;
      if (synth_text_from_beeeeep(dst,SRC+srcp,len,path)<0) return -1;
      srcp+=len;
    }
    return 0;
  }
  
  return -1;
}

/* MIDI from binary song.
 */

int synth_midi_from_egg(struct sr_encoder *dst,const void *src,int srcc,const char *path) {
  return -1;//TODO
}
