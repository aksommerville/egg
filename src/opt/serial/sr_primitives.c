#include "serial.h"
#include <string.h>
#include <limits.h>
#include <stdint.h>
#include <math.h>

/* Case-insensitive memcmp.
 */

int sr_memcasecmp(const void *a,const void *b,int c) {
  if (a==b) return 0;
  while (c-->0) {
    uint8_t cha=*(uint8_t*)a; a=(char*)a+1;
    uint8_t chb=*(uint8_t*)b; b=(char*)b+1;
    if ((cha>='A')&&(cha<='Z')) cha+=0x20;
    if ((chb>='A')&&(chb<='Z')) chb+=0x20;
    if (cha<chb) return -1;
    if (cha>chb) return 1;
  }
  return 0;
}

/* Measure whitespace.
 */
 
int sr_space_measure(const char *src,int srcc) {
  if (!src) return 0;
  int srcp=0;
  if (srcc<0) {
    while (src[srcp]&&((unsigned char)src[srcp]<=0x20)) srcp++;
  } else {
    while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
  }
  return srcp;
}

/* Measure integer.
 */

int sr_int_measure(const char *src,int srcc) {
  if (!src) return 0;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  int srcp=0;

  if (srcp>=srcc) return 0;
  if ((src[srcp]=='+')||(src[srcp]=='-')) srcp++;

  if (srcp>=srcc) return 0;
  if ((src[srcp]<'0')||(src[srcp]>'9')) return 0;
  srcp++;

  while ((srcp<srcc)&&(
    ((src[srcp]>='0')&&(src[srcp]<='9'))||
    ((src[srcp]>='a')&&(src[srcp]<='z'))||
    ((src[srcp]>='A')&&(src[srcp]<='Z'))
  )) srcp++;

  return srcp;
}

/* Evaluate integer.
 */
 
int sr_int_eval(int *dst,const char *src,int srcc) {
  if (!src) return -1;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  int srcp=0,positive=1,base=10;

  if (srcp>=srcc) return -1;
  if (src[srcp]=='-') {
    positive=0;
    if (++srcp>=srcc) return -1;
  } else if (src[srcp]=='+') {
    if (++srcp>=srcc) return -1;
  }

  if ((srcp<srcc-2)&&(src[srcp]=='0')) switch (src[srcp+1]) {
    case 'x': case 'X': base=16; srcp+=2; break;
    case 'd': case 'D': base=10; srcp+=2; break;
    case 'o': case 'O': base= 8; srcp+=2; break;
    case 'b': case 'B': base= 2; srcp+=2; break;
  }

  int limit,overflow=0;
  if (positive) limit=UINT_MAX/base;
  else limit=INT_MIN/base;
  *dst=0;
  while (srcp<srcc) {
    int digit=sr_digit_eval(src[srcp++]);
    if ((digit<0)||(digit>=base)) return -1;
    if (positive) {
      if ((unsigned int)*dst>limit) overflow=1;
      (*dst)*=base;
      if ((unsigned int)*dst>UINT_MAX-digit) overflow=1;
      (*dst)+=digit;
    } else {
      if (*dst<limit) overflow=1;
      (*dst)*=base;
      if (*dst<INT_MIN+digit) overflow=1;
      (*dst)-=digit;
    }
  }

  if (overflow) return 0;
  if (positive&&(*dst<0)) return 1;
  return 2;
}

/* Represent integer.
 */
 
int sr_decsint_repr(char *dst,int dsta,int src) {
  int dstc;
  if (src<0) {
    dstc=2;
    int limit=-10;
    while (limit>=src) { dstc++; if (limit<INT_MIN/10) break; limit*=10; }
    if (dstc>dsta) return dstc;
    int i=dstc; for (;i-->1;src/=10) dst[i]='0'-src%10;
    dst[0]='-';
  } else {
    dstc=1;
    int limit=10;
    while (limit<=src) { dstc++; if (limit>INT_MAX/10) break; limit*=10; }
    if (dstc>dsta) return dstc;
    int i=dstc; for (;i-->0;src/=10) dst[i]='0'+src%10;
  }
  if (dstc<dsta) dst[dstc]=0;
  return dstc;
}

int sr_decuint_repr(char *dst,int dsta,int src,int mindigitc) {
  if (mindigitc>64) mindigitc=64; // arbitrary sanity limit
  int dstc=1;
  unsigned int limit=10;
  while (limit<=(unsigned int)src) { dstc++; if (limit>UINT_MAX/10) break; limit*=10; }
  if (dstc<mindigitc) dstc=mindigitc;
  if (dstc>dsta) return dstc;
  int i=dstc; for (;i-->0;src=(unsigned int)src/10) dst[i]='0'+((unsigned int)src)%10;
  if (dstc<dsta) dst[dstc]=0;
  return dstc;
}

int sr_hexuint_repr(char *dst,int dsta,int src,int mindigitc) {
  if (mindigitc>64) mindigitc=64; // arbitrary sanity limit
  int dstc=1;
  unsigned int limit=~0x0f;
  while (limit&src) { dstc++; limit<<=4; }
  if (dstc<mindigitc) dstc=mindigitc;
  if (dstc>dsta) return dstc;
  int i=dstc; for (;i-->0;src>>=4) dst[i]="0123456789abcdef"[src&15];
  if (dstc<dsta) dst[dstc]=0;
  return dstc;
}

int sr_hexuint_repr_prefixed(char *dst,int dsta,int src,int mindigitc) {
  if (mindigitc>64) mindigitc=64; // arbitrary sanity limit
  int dstc=3;
  unsigned int limit=~0x0f;
  while (limit&src) { dstc++; limit<<=4; }
  if (dstc<mindigitc) dstc=mindigitc;
  if (dstc>dsta) return dstc;
  dst[0]='0';
  dst[1]='x';
  int i=dstc; for (;i-->2;src>>=4) dst[i]="0123456789abcdef"[src&15];
  if (dstc<dsta) dst[dstc]=0;
  return dstc;
}

/* Measure float.
 */

int sr_double_measure(const char *src,int srcc) {
  if (!src) return 0;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  int srcp=0;

  if (srcp>=srcc) return 0;
  if ((src[srcp]=='-')||(src[srcp]=='+')) srcp++;

  if (srcp>=srcc) return 0;
  if ((src[srcp]<='0')||(src[srcp]>='9')) return 0;
  srcp++;
  while ((srcp<srcc)&&(src[srcp]>='0')&&(src[srcp]<='9')) srcp++;

  if ((srcp<srcc)&&(src[srcp]=='.')) {
    if (++srcp>=srcc) return -1;
    if ((src[srcp]<='0')||(src[srcp]>='9')) return 0;
    srcp++;
    while ((srcp<srcc)&&(src[srcp]>='0')&&(src[srcp]<='9')) srcp++;
  }

  if ((srcp<srcc)&&((src[srcp]=='e')||(src[srcp]=='E'))) {
    if (++srcp>=srcc) return -1;
    if ((src[srcp]=='-')||(src[srcp]=='+')) {
      if (++srcp>=srcc) return -1;
    }
    if ((src[srcp]<='0')||(src[srcp]>='9')) return 0;
    srcp++;
    while ((srcp<srcc)&&(src[srcp]>='0')&&(src[srcp]<='9')) srcp++;
  }

  if (srcp<srcc) {
    if ((src[srcp]>='0')&&(src[srcp]<='9')) return 0;
    if ((src[srcp]>='a')&&(src[srcp]<='z')) return 0;
    if ((src[srcp]>='A')&&(src[srcp]<='Z')) return 0;
  }
  return srcp;
}

/* Evaluate double.
 */
 
int sr_double_eval(double *dst,const char *src,int srcc) {
  if (!src) return -1;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  int srcp=0,positive=1;

  if (srcp>=srcc) return -1;
  if (src[srcp]=='-') {
    positive=0;
    if (++srcp>=srcc) return -1;
  } else if (src[srcp]=='+') {
    if (++srcp>=srcc) return -1;
  }

  *dst=0.0;
  if ((src[srcp]<'0')||(src[srcp]>'9')) return -1;
  while ((srcp<srcc)&&(src[srcp]>='0')&&(src[srcp]<='9')) {
    int digit=src[srcp++]-'0';
    (*dst)*=10.0;
    (*dst)+=digit;
  }

  if ((srcp<srcc)&&(src[srcp]=='.')) {
    if (++srcp>=srcc) return -1;
    if ((src[srcp]<'0')||(src[srcp]>'9')) return -1;
    double coef=1.0;
    while ((srcp<srcc)&&(src[srcp]>='0')&&(src[srcp]<='9')) {
      int digit=src[srcp++]-'0';
      coef*=0.1;
      (*dst)+=digit*coef;
    }
  }

  if ((srcp<srcc)&&((src[srcp]=='e')||(src[srcp]=='E'))) {
    if (++srcp>=srcc) return -1;
    int epositive=1,e=0;
    if (src[srcp]=='-') {
      epositive=0;
      if (++srcp>=srcc) return -1;
    } else if (src[srcp]=='+') {
      if (++srcp>=srcc) return -1;
    }
    if ((src[srcp]<'0')||(src[srcp]>'9')) return -1;
    while ((srcp<srcc)&&(src[srcp]>='0')&&(src[srcp]<='9')) {
      int digit=src[srcp++]-'0';
      e*=10;
      e+=digit;
      if (e>99) e=99;
    }
    if (epositive) {
      while (e-->0) (*dst)*=10.0;
    } else {
      while (e-->0) (*dst)*=0.1;
    }
  }

  if (srcp<srcc) return -1;
  if (!positive) *dst=-*dst;
  return 0;
}

/* Represent double.
 */
 
int sr_double_repr(char *dst,int dsta,double src) {
  if (!dst||(dsta<0)) dsta=0;

  // Substitutions.
  switch (fpclassify(src)) {
    case FP_ZERO: {
        if (dsta>=3) memcpy(dst,"0.0",3);
        if (dsta>3) dst[3]=0;
        return 3;
      }
    case FP_NAN: {
        if (dsta>=3) memcpy(dst,"nan",3);
        if (dsta>3) dst[3]=0;
        return 3;
      }
    case FP_INFINITE: if (src<0.0) {
        if (dsta>=4) memcpy(dst,"-inf",4);
        if (dsta>4) dst[4]=0;
        return 4;
      } else {
        if (dsta>=3) memcpy(dst,"inf",3);
        if (dsta>3) dst[3]=0;
        return 3;
      }
  }

  // Emit sign and do the rest positive-only.
  int dstc=0;
  if (src<0.0) {
    if (dstc<dsta) dst[dstc]='-';
    dstc++;
    src=-src;
  }

  // Select exponent if outside some arbitrary range.
  int exp=0;
  if (src>=1000000.0) {
    while ((src>=10.0)&&(exp<99)) { src/=10.0; exp++; }
  } else if (src<0.001) {
    while ((src<1.0)&&(exp>-99)) { src*=10.0; exp--; }
  }

  // Split whole and fraction. Emit whole as an integer.
  double whole,fract;
  fract=modf(src,&whole);
  if (whole>=UINT_MAX) dstc+=sr_decuint_repr(dst+dstc,dsta-dstc,UINT_MAX,0);
  else dstc+=sr_decuint_repr(dst+dstc,dsta-dstc,UINT_MAX,0);

  // Fraction is dicey. Emit 1..9 digits, and cook them a bit first.
  char fractv[9];
  int fractc=0;
  for (;fractc<sizeof(fractv);fractc++) {
    fract*=10.0;
    int digit=(int)fract;
    fract-=digit;
    if (digit<0) digit=0;
    else if (digit>9) digit=9;
    fractv[fractc]='0'+digit;
  }
  while ((fractc>1)&&(fractv[fractc-1]=='0')) fractc--;
  int ninec=0;
  while ((ninec<fractc)&&(fractv[fractc-1]=='9')) ninec++;
  if ((ninec>=3)&&(ninec<fractc)) {
    fractc-=ninec;
    fractv[fractc-1]++;
  }
  if (dstc<dsta) dst[dstc]='.';
  dstc++;
  if (dstc<=dsta-fractc) memcpy(dst+dstc,fractv,fractc);
  dstc+=fractc;

  // Emit exponent if present.
  if (exp) {
    if (dstc<dsta) dst[dstc]='e';
    dstc++;
    dstc+=sr_decsint_repr(dst+dstc,dsta-dstc,exp);
  }

  if (dstc<dsta) dst[dstc]=0;
  return dstc;
}

/* JSON float.
 * Use sr_double_repr() but pick off nan, inf, and values that will require an exponent.
 */
 
int sr_double_repr_json(char *dst,int dsta,double src) {
  switch (fpclassify(src)) {
    case FP_ZERO: {
        if (dsta>=3) memcpy(dst,"0.0",3);
        if (dsta>3) dst[3]=0;
        return 3;
      }
    case FP_NAN: case FP_INFINITE: {
        if (dsta>=4) memcpy(dst,"null",4);
        if (dsta>4) dst[4]=0;
        return 4;
      }
  }
  if (src>=1000000.0) return sr_double_repr(dst,dsta,1000000.0);
  if (src<=-1000000.0) return sr_double_repr(dst,dsta,-1000000.0);
  if ((src>-0.001)&&(src<0.001)) return sr_double_repr(dst,dsta,0.0);
  return sr_double_repr(dst,dsta,src);
}

/* Measure string.
 */

int sr_string_measure(const char *src,int srcc,int *simple) {
  if (simple) *simple=1;
  if (!src) return 0;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  if (srcc<1) return 0;
  if ((src[0]!='"')&&(src[0]!='\'')&&(src[0]!='`')) return 0;
  int srcp=1;
  while (1) {
    if (srcp>=srcc) return 0;
    if (src[srcp]==src[0]) return srcp+1;
    if (src[srcp]=='\\') {
      if (simple) *simple=0;
      if (++srcp>=srcc) return 0;
    }
    srcp++;
  }
}

/* Evaluate string.
 */
 
int sr_string_eval(char *dst,int dsta,const char *src,int srcc) {
  if (!dst||(dsta<0)) dsta=0;
  if (!src) return -1;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  if ((srcc<2)||(src[0]!=src[srcc-1])) return -1;
  if ((src[0]!='"')&&(src[0]!='\'')&&(src[0]!='`')) return -1;
  src++; srcc-=2;
  int dstc=0,srcp=0;
  while (srcp<srcc) {
    if (src[srcp]=='\\') {
      if (++srcp>=srcc) return -1;
      switch (src[srcp++]) {
        case '\\': case '"': case '\'': case '`': if (dstc<dsta) dst[dstc]=src[srcp-1]; dstc++; break;
        case '0': if (dstc<dsta) dst[dstc]=0x00; dstc++; break;
        case 'b': if (dstc<dsta) dst[dstc]=0x08; dstc++; break;
        case 't': if (dstc<dsta) dst[dstc]=0x09; dstc++; break;
        case 'n': if (dstc<dsta) dst[dstc]=0x0a; dstc++; break;
        case 'r': if (dstc<dsta) dst[dstc]=0x0d; dstc++; break;
        case 'e': if (dstc<dsta) dst[dstc]=0x1b; dstc++; break;
        case 'x': {
            if (srcp>srcc-2) return -1;
            int hi=sr_digit_eval(src[srcp++]);
            int lo=sr_digit_eval(src[srcp++]);
            if ((hi<0)||(hi>15)) return -1;
            if ((lo<0)||(lo>15)) return -1;
            if (dstc<dsta) dst[dstc]=(hi<<4)|lo;
            dstc++;
          } break;
        case 'U': {
            int ch=0,digitc=0;
            while ((srcp<srcc)&&(digitc<6)) {
              int digit=sr_digit_eval(src[srcp]);
              if ((digit<0)||(digit>15)) break;
              srcp++;
              ch<<=4;
              ch|=digit;
            }
            if (!digitc) return -1;
            if ((srcp<srcc)&&(src[srcp]==';')) srcp++;
            int err=sr_utf8_encode(dst+dstc,dsta-dstc,ch);
            if (err<0) return -1;
            dstc+=err;
          } break;
        case 'u': {
            if (srcp>srcc-4) return -1;
            int ch=0;
            int i=4; while (i-->0) {
              int digit=sr_digit_eval(src[srcp++]);
              if ((digit<0)||(digit>15)) return -1;
              ch<<=4;
              ch|=digit;
            }
            if ((ch>=0xd800)&&(ch<0xdc00)&&(srcp<=srcc-6)&&(src[srcp]=='\\')&&(src[srcp+1]=='u')) {
              const char *next=src+srcp+2;
              int lo=0;
              for (i=4;i-->0;) {
                int digit=sr_digit_eval(src[srcp++]);
                if ((digit<0)||(digit>15)) return -1;
                lo<<=4;
                lo|=digit;
              }
              if ((lo>=0xd800)&&(lo<0xe000)) {
                srcp+=6;
                ch=0x10000+(((ch&0x3ff)<<10)|(lo&0x3ff));
              }
            }
            int err=sr_utf8_encode(dst+dstc,dsta-dstc,ch);
            if (err<0) return -1;
            dstc+=err;
          } break;
        default: return -1;
      }
    } else {
      if (dstc<dsta) dst[dstc]=src[srcp];
      dstc++;
      srcp++;
    }
  }
  if (dstc<dsta) dst[dstc]=0;
  return dstc;
}

/* Reprsent string, non-JSON.
 */
 
int sr_string_repr(char *dst,int dsta,const char *src,int srcc) {
  if (!dst||(dsta<0)) dsta=0;
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  int dstc=0,srcp=0;
  if (dstc<dsta) dst[dstc]='"';
  dstc++;
  for (;srcp<srcc;srcp++) {
    switch (src[srcp]) {
      case '\\': case '"': if (dstc<=dsta-2) { dst[dstc++]='\\'; dst[dstc++]=src[srcp]; } else dstc+=2; srcp++; break;
      case 0x00: if (dstc<=dsta-2) { dst[dstc++]='\\'; dst[dstc++]='0'; } else dstc+=2; srcp++; break;
      case 0x08: if (dstc<=dsta-2) { dst[dstc++]='\\'; dst[dstc++]='b'; } else dstc+=2; srcp++; break;
      case 0x09: if (dstc<=dsta-2) { dst[dstc++]='\\'; dst[dstc++]='t'; } else dstc+=2; srcp++; break;
      case 0x0a: if (dstc<=dsta-2) { dst[dstc++]='\\'; dst[dstc++]='n'; } else dstc+=2; srcp++; break;
      case 0x0d: if (dstc<=dsta-2) { dst[dstc++]='\\'; dst[dstc++]='r'; } else dstc+=2; srcp++; break;
      case 0x1b: if (dstc<=dsta-2) { dst[dstc++]='\\'; dst[dstc++]='e'; } else dstc+=2; srcp++; break;
      default: if ((src[srcp]>=0x20)&&(src[srcp]<=0x7e)) {
          if (dstc<dsta) dst[dstc]=src[srcp];
          dstc++;
          srcp++;
        } else {
          int ch,seqlen;
          if ((seqlen=sr_utf8_decode(&ch,src+srcp,srcc-srcp))>0) {
            if (dstc<=dsta-2) {
              dst[dstc++]='\\';
              dst[dstc++]='U';
            }
            int err=sr_hexuint_repr(dst+dstc,dsta-dstc,ch,1);
            if ((err<1)||(err>6)) { dstc-=2; goto _hex_byte_; }
            dstc+=err;
            srcp+=seqlen;
            if ((srcp<srcc)&&(src[srcp]==';')) {
              if (dstc<dsta) dst[dstc]=';';
              dstc++;
            }
          } else {
           _hex_byte_:;
            if (dstc<=dsta-4) {
              dst[dstc++]='\\';
              dst[dstc++]='x';
              dst[dstc++]="0123456789abcdef"[(src[srcp]>>4)&15];
              dst[dstc++]="0123456789abcdef"[src[srcp]&15];
            } else dstc+=4;
            srcp++;
          }
        }
    }
  }
  if (dstc<dsta) dst[dstc]='"';
  dstc++;
  if (dstc<dsta) dst[dstc]=0;
  return dstc;
}

/* Represent string, JSON.
 */
 
int sr_string_repr_json(char *dst,int dsta,const char *src,int srcc) {
  if (!dst||(dsta<0)) dsta=0;
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  int dstc=0,srcp=0;
  if (dstc<dsta) dst[dstc]='"';
  dstc++;
  while (srcp<srcc) {
    switch (src[srcp]) {
      case '\\': case '"': if (dstc<=dsta-2) { dst[dstc++]='\\'; dst[dstc++]=src[srcp]; } else dstc+=2; srcp++; break;
      //TODO I'm not sure this is the complete set of JSON named escapes.
      case 0x08: if (dstc<=dsta-2) { dst[dstc++]='\\'; dst[dstc++]='b'; } else dstc+=2; srcp++; break;
      case 0x09: if (dstc<=dsta-2) { dst[dstc++]='\\'; dst[dstc++]='t'; } else dstc+=2; srcp++; break;
      case 0x0a: if (dstc<=dsta-2) { dst[dstc++]='\\'; dst[dstc++]='n'; } else dstc+=2; srcp++; break;
      case 0x0d: if (dstc<=dsta-2) { dst[dstc++]='\\'; dst[dstc++]='r'; } else dstc+=2; srcp++; break;
      default: if ((src[srcp]>=0x20)&&(src[srcp]<=0x7e)) {
          if (dstc<dsta) dst[dstc]=src[srcp];
          dstc++;
          srcp++;
        } else {
          int ch,seqlen;
          if ((seqlen=sr_utf8_decode(&ch,src+srcp,srcc-srcp))<1) {
            ch=(unsigned char)src[srcp];
            seqlen=1;
          }
          srcp+=seqlen;
          if (ch>=0x10000) {
            if (dstc<=dsta-12) {
              ch-=0x10000;
              int hi=0xd800|(ch>>10);
              int lo=0xdc00|(ch&0x3ff);
              dst[dstc++]='\\';
              dst[dstc++]='u';
              dst[dstc++]="0123456789abcdef"[(hi>>12)&15];
              dst[dstc++]="0123456789abcdef"[(hi>> 8)&15];
              dst[dstc++]="0123456789abcdef"[(hi>> 4)&15];
              dst[dstc++]="0123456789abcdef"[(hi>> 0)&15];
              dst[dstc++]='\\';
              dst[dstc++]='u';
              dst[dstc++]="0123456789abcdef"[(lo>>12)&15];
              dst[dstc++]="0123456789abcdef"[(lo>> 8)&15];
              dst[dstc++]="0123456789abcdef"[(lo>> 4)&15];
              dst[dstc++]="0123456789abcdef"[(lo>> 0)&15];
            } else dstc+=12;
          } else if (dstc<=dsta-6) {
            dst[dstc++]='\\';
            dst[dstc++]='u';
            dst[dstc++]="0123456789abcdef"[(ch>>12)&15];
            dst[dstc++]="0123456789abcdef"[(ch>> 8)&15];
            dst[dstc++]="0123456789abcdef"[(ch>> 4)&15];
            dst[dstc++]="0123456789abcdef"[(ch>> 0)&15];
          } else dstc+=6;
        }
    }
  }
  if (dstc<dsta) dst[dstc]='"';
  dstc++;
  if (dstc<dsta) dst[dstc]=0;
  return dstc;
}

/* Split comma-delimited list.
 */
 
int sr_string_split(
  const char *src,int srcc,
  char sep,
  int (*cb)(const char *word,int wordc,void *userdata),
  void *userdata
) {
  if (!src) return 0;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  int srcp=0,err;
  if ((unsigned char)sep<=0x20) {
    while (srcp<srcc) {
      if ((unsigned char)src[srcp]<=0x20) { srcp++; continue; }
      const char *word=src+srcp;
      int wordc=0;
      while ((srcp<srcc)&&((unsigned char)src[srcp++]<=0x20)) wordc++;
      if (err=cb(word,wordc,userdata)) return err;
    }
  } else {
    while (srcp<srcc) {
      if ((unsigned char)src[srcp]<=0x20) { srcp++; continue; }
      const char *word=src+srcp;
      int wordc=0;
      while ((srcp<srcc)&&(src[srcp]!=sep)) { srcp++; wordc++; }
      if (srcp<srcc) srcp++;
      while (wordc&&((unsigned char)word[wordc-1]<=0x20)) wordc--;
      if (err=cb(word,wordc,userdata)) return err;
    }
  }
  return 0;
}

/* Match pattern.
 */

static int sr_pattern_match_1(const char *pat,int patc,const char *src,int srcc) {
  int patp=0,srcp=0,score=1;
  while (1) {

    // Pattern termination ends the operation.
    if (patp>=patc) {
      if (srcp>=srcc) return score;
      return 0;
    }

    // Wildcards.
    if (pat[patp]=='*') {
      patp++; while ((patp<patc)&&(pat[patp]=='*')) patp++; // Adjacent stars are redundant.
      if (srcp>=srcc) return score; // Trailing stars automatically match.
      while (srcp<srcc) {
        int sub=sr_pattern_match_1(pat+patp,patc-patp,src+srcp,srcc-srcp);
        if (sub>0) {
          if (score>INT_MAX-sub) return INT_MAX;
          return score+sub;
        }
        srcp++;
      }
      return 0;
    }

    // With wildcards out of the way, end of (src) is a mismatch.
    if (srcp>=srcc) return 0;

    // Whitespace collapses and doesn't contribute to the score.
    if ((unsigned char)pat[patp]<=0x20) {
      if ((unsigned char)src[srcp]>0x20) return 0;
      while ((patp<patc)&&((unsigned char)pat[patp]<=0x20)) patp++;
      while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
      continue;
    }

    // Backslash matches one character case-sensitively and does contribute to the score.
    if (pat[patp]=='\\') {
      if (++patp>=patc) return 0; // Malformed pattern.
      if (pat[patp++]!=src[srcp++]) return 0;
      if (score<INT_MAX) score++;
      continue;
    }

    // Everything else is case-insensitive and does contribute to the score.
    char cha=pat[patp++]; if ((cha>='A')&&(cha<='Z')) cha+=0x20;
    char chb=src[srcp++]; if ((chb>='A')&&(chb<='Z')) chb+=0x20;
    if (cha!=chb) return 0;
    if (score<INT_MAX) score++;
  }
}

int sr_pattern_match(const char *pat,int patc,const char *src,int srcc) {
  if (!pat) patc=0; else if (patc<0) { patc=0; while (pat[patc]) patc++; }
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  while (patc&&((unsigned char)pat[patc-1]<=0x20)) patc--;
  while (patc&&((unsigned char)pat[0]<=0x20)) { patc--; pat++; }
  while (srcc&&((unsigned char)src[srcc-1]<=0x20)) srcc--;
  while (srcc&&((unsigned char)src[0]<=0x20)) { srcc--; src++; }
  return sr_pattern_match_1(pat,patc,src,srcc);
}

/* Binary integers.
 */

int sr_intbe_encode(void *dst,int dsta,int src,int size) {
  if (size<0) size=-size; // sign doesn't matter when encoding, but allow it for consistency
  if ((size<1)||(size>4)) return -1;
  if (!dst) dsta=0;
  if (size>dsta) return size;
  uint8_t *DST=dst;
  switch (size) {
    case 1: DST[0]=src; break;
    case 2: DST[0]=src>>8; DST[1]=src; break;
    case 3: DST[0]=src>>16; DST[1]=src>>8; DST[2]=src; break;
    case 4: DST[0]=src>>24; DST[1]=src>>16; DST[2]=src>>8; DST[3]=src; break;
  }
  return size;
}
  
int sr_intle_encode(void *dst,int dsta,int src,int size) {
  if (size<0) size=-size; // sign doesn't matter when encoding, but allow it for consistency
  if ((size<1)||(size>4)) return -1;
  if (!dst) dsta=0;
  if (size>dsta) return size;
  uint8_t *DST=dst;
  switch (size) {
    case 1: DST[0]=src; break;
    case 2: DST[0]=src; DST[1]=src>>8; break;
    case 3: DST[0]=src; DST[1]=src>>8; DST[2]=src>>16; break;
    case 4: DST[0]=src; DST[1]=src>>8; DST[2]=src>>16; DST[3]=src>>24; break;
  }
  return size;
}

int sr_intbe_decode(int *v,const void *src,int srcc,int size) {
  int sign=0;
  if (size<0) { sign=1; size=-size; }
  if ((size<1)||(size>4)) return -1;
  if (size>srcc) return -1;
  const uint8_t *SRC=src;
  switch (size) {
    case 1: *v=SRC[0]; break;
    case 2: *v=(SRC[0]<<8)|SRC[1]; break;
    case 3: *v=(SRC[0]<<16)|(SRC[1]<<8)|SRC[2]; break;
    case 4: *v=(SRC[0]<<24)|(SRC[1]<<16)|(SRC[2]<<8)|SRC[3]; break;
  }
  if (sign&&(size<4)) {
    int mask=(~0)<<((size<<3)-1);
    if ((*v)&mask) (*v)|=mask;
  }
  return size;
}

int sr_intle_decode(int *v,const void *src,int srcc,int size) {
  int sign=0;
  if (size<0) { sign=1; size=-size; }
  if ((size<1)||(size>4)) return -1;
  if (size>srcc) return -1;
  const uint8_t *SRC=src;
  switch (size) {
    case 1: *v=SRC[0]; break;
    case 2: *v=SRC[0]|(SRC[1]<<8); break;
    case 3: *v=SRC[0]|(SRC[1]<<8)|(SRC[2]<<16); break;
    case 4: *v=SRC[0]|(SRC[1]<<8)|(SRC[2]<<16)|(SRC[3]<<24); break;
  }
  if (sign&&(size<4)) {
    int mask=(~0)<<((size<<3)-1);
    if ((*v)&mask) (*v)|=mask;
  }
  return size;
}

/* Fixed-point numbers.
 */

#define SR_FIXED_VALIDATE \
  if ((wholesize<0)||(wholesize>32)) return -1; \
  if ((fractsize<0)||(fractsize>24)) return -1; \
  int totalsize=wholesize+fractsize; \
  if (totalsize>32) return -1; \
  if (totalsize<1) return -1; \
  if (totalsize&7) { \
    if (sign) totalsize++; \
    if (totalsize&7) return -1; \
  } else if (sign) { \
    if (wholesize) wholesize--; \
    else fractsize--; \
  }

int sr_fixed_encode(void *dst,int dsta,double src,int sign,int wholesize,int fractsize) {
  SR_FIXED_VALIDATE

  int dstc=totalsize>>3;
  if (dstc>dsta) return dstc;
  uint8_t *DST=dst;

  uint32_t combined=(src*(1<<fractsize));
  switch (dstc) {
    case 1: DST[0]=combined; break;
    case 2: DST[0]=combined>>8; DST[1]=combined; break;
    case 3: DST[0]=combined>>16; DST[1]=combined>>8; DST[2]=combined; break;
    case 4: DST[0]=combined>>24; DST[1]=combined>>16; DST[2]=combined>>8; DST[3]=combined; break;
    default: return -1;
  }

  return dstc;
}

int sr_fixed_decode(double *dst,const void *src,int srcc,int sign,int wholesize,int fractsize) {
  SR_FIXED_VALIDATE

  const uint8_t *SRC=src;
  uint32_t combined;
  switch (totalsize) {
    case  8: if (srcc<1) return -1; combined=SRC[0]; break;
    case 16: if (srcc<2) return -1; combined=(SRC[0]<<8)|SRC[1]; break;
    case 24: if (srcc<3) return -1; combined=(SRC[0]<<16)|(SRC[1]<<24)|SRC[2]; break;
    case 32: if (srcc<4) return -1; combined=(SRC[0]<<24)|(SRC[1]<<16)|(SRC[2]<<8)|SRC[3]; break;
    default: return -1;
  }
  
  if (sign) {
    uint32_t mask=(~0)<<(totalsize-1);
    if (combined&mask) {
      combined|=mask;
      *dst=(int32_t)(combined>>fractsize);
      if (fractsize) {
        uint32_t denom=(1<<fractsize);
        (*dst)-=(double)(combined&(denom-1))/(double)denom;
      }
    } else goto _unsigned_;
  } else {
   _unsigned_:;
    *dst=combined>>fractsize;
    if (fractsize) {
      uint32_t denom=(1<<fractsize);
      (*dst)+=(double)(combined&(denom-1))/(double)denom;
    }
  }
  
  return totalsize>>3;
}

/* UTF-8.
 */

int sr_utf8_encode(void *dst,int dsta,int src) {
  if (!dst) dsta=0;
  if (src<0) return -1;
  uint8_t *DST=dst;
  if (src<0x000080) {
    if (dsta>=1) {
      DST[0]=src;
    }
    return 1;
  }
  if (src<0x000800) {
    if (dsta>=2) {
      DST[0]=0xc0|(src>>6);
      DST[1]=0x80|(src&0x3f);
    }
    return 2;
  }
  if (src<0x010000) {
    if (dsta>=3) {
      DST[0]=0xe0|(src>>12);
      DST[1]=0x80|((src>>6)&0x3f);
      DST[2]=0x80|(src&0x3f);
    }
    return 3;
  }
  if (src<0x110000) {
    if (dsta>=4) {
      DST[0]=0xf0|(src>>18);
      DST[1]=0x80|((src>>12)&0x3f);
      DST[2]=0x80|((src>>6)&0x3f);
      DST[3]=0x80|(src&0x3f);
    }
    return 4;
  }
  return -1;
}

int sr_utf8_decode(int *dst,const void *src,int srcc) {
  if (srcc<1) return -1;
  const uint8_t *SRC=src;
  if (!(SRC[0]&0x80)) {
    *dst=SRC[0];
    return 1;
  }
  if (!(SRC[0]&0x40)) {
    return -1;
  }
  if (!(SRC[0]&0x20)) {
    if (srcc<2) return -1;
    if ((SRC[1]&0xc0)!=0x80) return -1;
    *dst=((SRC[0]&0x1f)<<6)|(SRC[1]&0x3f);
    return 2;
  }
  if (!(SRC[0]&0x10)) {
    if (srcc<3) return -1;
    if ((SRC[1]&0xc0)!=0x80) return -1;
    if ((SRC[2]&0xc0)!=0x80) return -1;
    *dst=((SRC[0]&0x0f)<<12)|((SRC[1]&0x3f)<<6)|(SRC[2]&0x3f);
    return 3;
  }
  if (!(SRC[0]&0x08)) {
    if (srcc<4) return -1;
    if ((SRC[1]&0xc0)!=0x80) return -1;
    if ((SRC[2]&0xc0)!=0x80) return -1;
    if ((SRC[3]&0xc0)!=0x80) return -1;
    *dst=((SRC[0]&0x3f)<<18)|((SRC[1]&0x3f)<<12)|((SRC[2]&0x3f)<<6)|(SRC[3]&0x3f);
    return 4;
  }
  return -1;
}

/* VLQ.
 */
 
int sr_vlq_encode(void *dst,int dsta,int src) {
  if (!dst) dsta=0;
  if (src<0) return -1;
  uint8_t *DST=dst;
  if (src<0x00000080) {
    if (dsta>=1) {
      DST[0]=src;
    }
    return 1;
  }
  if (src<0x00004000) {
    if (dsta>=2) {
      DST[0]=0x80|(src>>7);
      DST[1]=(src&0x7f);
    }
    return 2;
  }
  if (src<0x00200000) {
    if (dsta>=3) {
      DST[0]=0x80|(src>>14);
      DST[1]=0x80|((src>>7)&0x7f);
      DST[2]=src&0x7f;
    }
    return 3;
  }
  if (src<0x10000000) {
    if (dsta>=4) {
      DST[0]=0x80|(src>>21);
      DST[1]=0x80|((src>>14)&0x7f);
      DST[2]=0x80|((src>>7)&0x7f);
      DST[3]=src&0x7f;
    }
    return 4;
  }
  return -1;
}

int sr_vlq_decode(int *dst,const void *src,int srcc) {
  if (!src||(srcc<1)) return 0;
  const uint8_t *SRC=src;
  if (!(SRC[0]&0x80)) {
    *dst=SRC[0];
    return 1;
  }
  if (srcc<2) return -1;
  if (!(SRC[1]&0x80)) {
    *dst=((SRC[0]&0x7f)<<7)|SRC[1];
    return 2;
  }
  if (srcc<3) return -1;
  if (!(SRC[2]&0x80)) {
    *dst=((SRC[0]&0x7f)<<14)|((SRC[1]&0x7f)<<7)|SRC[2];
    return 3;
  }
  if (srcc<4) return -1;
  if (!(SRC[3]&0x80)) {
    *dst=((SRC[0]&0x7f)<<21)|((SRC[1]&0x7f)<<14)|((SRC[2]&0x7f)<<7)|SRC[3];
    return 4;
  }
  return -1;
}
