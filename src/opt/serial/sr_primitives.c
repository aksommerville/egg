#include "serial.h"
#include <string.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdint.h>

/* Case-insensitive memcmp.
 */
 
int sr_memcasecmp(const void *a,const void *b,int c) {
  if (a==b) return 0;
  if (c<1) return 0;
  while (c-->0) {
    unsigned char cha=*(unsigned char*)a; a=(char*)a+1;
    unsigned char chb=*(unsigned char*)b; b=(char*)b+1;
    if ((cha>='A')&&(cha<='Z')) cha+=0x20;
    if ((chb>='A')&&(chb<='Z')) chb+=0x20;
    if (cha<chb) return -1;
    if (cha>chb) return 1;
  }
  return 0;
}

/* Evaluate integer.
 */

int sr_int_eval(int *v,const char *src,int srcc) {
  if (!src) return -1;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  int srcp=0,positive=1,base=10;
  
  if (srcp>=srcc) return -1;
  if (src[srcp]=='-') {
    if (++srcp>=srcc) return -1;
    positive=0;
  } else if (src[srcp]=='+') {
    if (++srcp>=srcc) return -1;
  }
  
  if ((src[srcp]=='0')&&(srcp<=srcc-3)) switch (src[srcp+1]) {
    case 'b': case 'B': base=2; srcp+=2; break;
    case 'o': case 'O': base=8; srcp+=2; break;
    case 'd': case 'D': base=10; srcp+=2; break;
    case 'x': case 'X': base=16; srcp+=2; break;
  }
  
  int limit,overflow=0;
  if (positive) limit=UINT_MAX/base;
  else limit=INT_MIN/base;
  
  *v=0;
  if (srcp>=srcc) return -1;
  for (;srcp<srcc;srcp++) {
    int digit=src[srcp];
         if ((digit>='0')&&(digit<='9')) digit=digit-'0';
    else if ((digit>='a')&&(digit<='f')) digit=digit-'a'+10;
    else if ((digit>='A')&&(digit<='F')) digit=digit-'A'+10;
    else return -1;
    if (digit>=base) return -1;
    if (positive) {
      if ((unsigned int)(*v)>limit) overflow=1;
      (*v)*=base;
      if ((unsigned int)(*v)>UINT_MAX-digit) overflow=1;
      (*v)+=digit;
    } else {
      if (*v<limit) overflow=1;
      (*v)*=base;
      if (*v<INT_MIN+digit) overflow=1;
      (*v)-=digit;
    }
  }
  if (overflow) return 0;
  if (positive&&(*v<0)) return 1;
  return 2;
}

/* Represent integer.
 */

int sr_decsint_repr(char *dst,int dsta,int v) {
  int dstc;
  if (v<0) {
    dstc=2;
    int limit=-10;
    while (v<=limit) { dstc++; if (limit<INT_MIN/10) break; limit*=10; }
    if (dstc>dsta) return dstc;
    int i=dstc; for (;i-->1;v/=10) dst[i]='0'-v%10;
    dst[0]='-';
  } else {
    dstc=1;
    int limit=10;
    while (v>=limit) { dstc++; if (limit>INT_MAX/10) break; limit*=10; }
    if (dstc>dsta) return dstc;
    int i=dstc; for (;i-->0;v/=10) dst[i]='0'+v%10;
  }
  if (dstc<dsta) dst[dstc]=0;
  return dstc;
}

int sr_decsint64_repr(char *dst,int dsta,int64_t v) {
  int dstc;
  if (v<0) {
    dstc=2;
    int64_t limit=-10;
    while (v<=limit) { dstc++; if (limit<INT64_MIN/10) break; limit*=10; }
    if (dstc>dsta) return dstc;
    int i=dstc; for (;i-->1;v/=10) dst[i]='0'-v%10;
    dst[0]='-';
  } else {
    dstc=1;
    int64_t limit=10;
    while (v>=limit) { dstc++; if (limit>INT64_MAX/10) break; limit*=10; }
    if (dstc>dsta) return dstc;
    int i=dstc; for (;i-->0;v/=10) dst[i]='0'+v%10;
  }
  if (dstc<dsta) dst[dstc]=0;
  return dstc;
}

int sr_decuint_repr(char *dst,int dsta,int v,int mindigitc) {
  int dstc=1;
  unsigned int limit=10;
  while ((unsigned int)v>=limit) { dstc++; if (limit>UINT_MAX/10) break; limit*=10; }
  if (mindigitc>64) mindigitc=64;
  if (dstc<mindigitc) dstc=mindigitc;
  if (dstc>dsta) return dstc;
  int i=dstc; for (;i-->0;v=(unsigned int)v/10) dst[i]='0'+((unsigned int)v)%10;
  if (dstc<dsta) dst[dstc]=0;
  return dstc;
}

int sr_hexuint_repr(char *dst,int dsta,int v,int prefix,int mindigitc) {
  int digitc=1;
  unsigned int limit=~0xf;
  while (v&limit) { digitc++; limit<<=4; }
  if (mindigitc>64) mindigitc=64;
  if (digitc<mindigitc) digitc=mindigitc;
  int dstc=digitc;
  if (prefix) dstc+=2;
  if (dstc>dsta) return dstc;
  int i=dstc;
  if (prefix) {
    for (;i-->2;v=(unsigned int)v>>4) dst[i]="0123456789abcdef"[v&15];
    dst[0]='0';
    dst[1]='x';
  } else {
    for (;i-->0;v=(unsigned int)v>>4) dst[i]="0123456789abcdef"[v&15];
  }
  if (dstc<dsta) dst[dstc]=0;
  return dstc;
}

/* Evaluate float.
 */

int sr_double_eval(double *v,const char *src,int srcc) {
  if (!src) return -1;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  
  // Special tokens.
  if ((srcc==3)&&!sr_memcasecmp(src,"NAN",3)) { *v=NAN; return 0; }
  if ((srcc==3)&&!sr_memcasecmp(src,"INF",3)) { *v=INFINITY; return 0; }
  if ((srcc==4)&&!sr_memcasecmp(src,"-INF",4)) { *v=-INFINITY; return 0; }
  
  int srcp=0,positive=1;
  if (srcp>=srcc) return -1;
  if (src[srcp]=='-') {
    if (++srcp>=srcc) return -1;
    positive=0;
  } else if (src[srcp]=='+') {
    if (++srcp>=srcc) return -1;
  }
  
  *v=0;
  if ((srcp>=srcc)||(src[srcp]<'0')||(src[srcp]>'9')) return -1;
  while ((srcp<srcc)&&(src[srcp]>='0')&&(src[srcp]<='9')) {
    double digit=src[srcp++]-'0';
    (*v)*=10.0;
    (*v)+=digit;
  }
  
  if ((srcp<srcc)&&(src[srcp]=='.')) {
    if (++srcp>=srcc) return -1;
    if ((src[srcp]<'0')||(src[srcp]>'9')) return -1;
    double coef=1.0;
    while ((srcp<srcc)&&(src[srcp]>='0')&&(src[srcp]<='9')) {
      coef/=10.0;
      double digit=src[srcp++]-'0';
      (*v)+=digit*coef;
    }
  }
  
  if ((srcp<srcc)&&((src[srcp]=='e')||(src[srcp]=='E'))) {
    if (++srcp>=srcc) return -1;
    int epositive=1;
    if (src[srcp]=='-') {
      if (++srcp>=srcc) return -1;
      epositive=0;
    } else if (src[srcp]=='+') {
      if (++srcp>=srcc) return -1;
    }
    if ((src[srcp]<'0')||(src[srcp]>'9')) return -1;
    int exp=0;
    while ((srcp<srcc)&&(src[srcp]>='0')&&(src[srcp]<='9')) {
      int digit=src[srcp++]-'0';
      exp*=10;
      exp+=digit;
      if (exp>99) exp=99;
    }
    if (epositive) {
      while (exp-->0) (*v)*=10.0;
    } else {
      while (exp-->0) (*v)*=0.1;
    }
  }
  
  if (srcp<srcc) return -1;
  if (!positive) *v=-*v;
  return 0;
}

/* Represent float.
 */
    
int sr_double_repr(char *dst,int dsta,double v) {
  switch (fpclassify(v)) {
    case FP_NAN: if (dsta>=3) memcpy(dst,"NAN",3); if (dsta>3) dst[3]=0; return 3;
    case FP_INFINITE: if (v<0.0) {
        if (dsta>=4) memcpy(dst,"-INF",4); if (dsta>4) dst[4]=0; return 4;
      } else {
        if (dsta>=3) memcpy(dst,"INF",3); if (dsta>3) dst[3]=0; return 3;
      }
    case FP_ZERO: if (dsta>=3) memcpy(dst,"0.0",3); if (dsta>3) dst[3]=0; return 3;
  }

  // Emit sign and do the rest positive-only.
  int dstc=0;
  if (v<0.0) {
    if (dstc<dsta) dst[dstc]='-';
    dstc++;
    v=-v;
  }

  // Select exponent if outside some arbitrary range.
  int exp=0;
  if (v>=1000000.0) {
    while ((v>=10.0)&&(exp<99)) { v/=10.0; exp++; }
  } else if (v<0.001) {
    while ((v<1.0)&&(exp>-99)) { v*=10.0; exp--; }
  }

  // Split whole and fraction. Emit whole as an integer.
  double whole,fract;
  fract=modf(v,&whole);
  if (whole>=UINT_MAX) dstc+=sr_decuint_repr(dst+dstc,dsta-dstc,UINT_MAX,0);
  else dstc+=sr_decuint_repr(dst+dstc,dsta-dstc,whole,0);

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
  while ((ninec<fractc)&&(fractv[fractc-ninec-1]=='9')) ninec++;
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

/* Measure number token.
 */

int sr_number_measure(const char *src,int srcc,int *flags) {
  int _flags;
  if (!flags) flags=&_flags;
  *flags=0;
  if (!src) return 0;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  int srcp=0;
  if (srcp>=srcc) return 0;
  if ((src[srcp]=='+')||(src[srcp]=='-')) {
    if (++srcp>=srcc) return 0;
  }
  if ((src[srcp]<'0')||(src[srcp]>'9')) return 0;
  if ((srcp<=srcc-2)&&(src[srcp]=='0')) switch (src[srcp+1]) {
    case 'b': case 'B':
    case 'o': case 'O':
    case 'd': case 'D':
    case 'x': case 'X': {
        (*flags)|=SR_NUMBER_BASE;
        srcp+=2;
      } break;
  }
  while (srcp<srcc) {
         if ((src[srcp]>='0')&&(src[srcp]<='9')) ;
    else if ((src[srcp]>='a')&&(src[srcp]<='f')) ;
    else if ((src[srcp]>='A')&&(src[srcp]<='F')) ;
    else break;
    srcp++;
  }
  if ((srcp<=srcc-2)&&(src[srcp]=='.')&&(src[srcp+1]>='0')&&(src[srcp+1]<='9')) {
    (*flags)|=SR_NUMBER_FRACT;
    srcp+=2;
    while ((srcp<srcc)&&(src[srcp]>='0')&&(src[srcp]<='9')) srcp++;
  }
  if ((srcp<srcc)&&((src[srcp]=='e')||(src[srcp]=='E'))) {
    (*flags)|=SR_NUMBER_EXP;
    if (++srcp>=srcc) return 0;
    if ((src[srcp]=='-')||(src[srcp]=='+')) {
      if (++srcp>=srcc) return 0;
    }
    while ((srcp<srcc)&&(src[srcp]>='0')&&(src[srcp]<='9')) srcp++;
  }
  if (srcp>=srcc) return srcp;
  if ((src[srcp]>='a')&&(src[srcp]<='z')) return 0;
  if ((src[srcp]>='A')&&(src[srcp]<='Z')) return 0;
  if (src[srcp]=='_') return 0;
  return srcp;
}

/* Evaluate string.
 */

int sr_string_eval(char *dst,int dsta,const char *src,int srcc) {
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
        case '\\': case '"': case '\'': case '`': case '/': if (dstc<dsta) dst[dstc]=src[srcp-1]; dstc++; break;
        case '0': if (dstc<dsta) dst[dstc]=0x00; dstc++; break;
        case 'b': if (dstc<dsta) dst[dstc]=0x08; dstc++; break;
        case 't': if (dstc<dsta) dst[dstc]=0x09; dstc++; break;
        case 'n': if (dstc<dsta) dst[dstc]=0x0a; dstc++; break;
        case 'f': if (dstc<dsta) dst[dstc]=0x0c; dstc++; break;
        case 'r': if (dstc<dsta) dst[dstc]=0x0d; dstc++; break;
        case 'e': if (dstc<dsta) dst[dstc]=0x1b; dstc++; break;
        case 'x': {
            if (srcp>srcc-2) return -1;
            int hi=sr_digit_eval(src[srcp++]);
            int lo=sr_digit_eval(src[srcp++]);
            if ((hi<0)||(hi>0xf)||(lo<0)||(lo>0xf)) return -1;
            if (dstc<dsta) dst[dstc]=(hi<<4)|lo; dstc++;
          } break;
        case 'u': {
            if (srcp>srcc-4) return -1;
            int ch=0;
            int i=0; for (;i<4;i++) {
              int digit=sr_digit_eval(src[srcp++]);
              if ((digit<0)||(digit>0xf)) return -1;
              ch<<=4;
              ch|=digit;
            }
            if ((ch>=0xd800)&&(ch<0xdc00)&&(srcp<=srcc-6)&&(src[srcp]=='\\')&&(src[srcp+1]=='u')) {
              int lo=0;
              for (i=2;i<6;i++) {
                int digit=sr_digit_eval(src[srcp+i]);
                if ((digit<0)||(digit>0xf)) return -1;
                lo<<=4;
                lo|=digit;
              }
              if ((lo>=0xdc00)&&(lo<0xe000)) {
                srcp+=6;
                ch=0x10000+((ch&0x3ff)<<10)|(lo&0x3ff);
              }
            }
            int err=sr_utf8_encode(dst+dstc,dsta-dstc,ch);
            if (err<1) return -1;
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

/* Represent string.
 */
 
int sr_string_repr(char *dst,int dsta,const char *src,int srcc) {
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  int dstc=0,srcp=0;
  if (dstc<dsta) dst[dstc]='"'; dstc++;
  while (srcp<srcc) {
    switch (src[srcp]) {
      case '"': case '\\': if (dstc<=dsta-2) { dst[dstc++]='\\'; dst[dstc++]=src[srcp]; } else dstc+=2; srcp++; break;
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
            ch=src[srcp];
            seqlen=1;
          }
          srcp+=seqlen;
          if (ch>=0x10000) {
            if (dstc<=dsta-12) {
              ch-=0x1000;
              int hi=0xd800|(ch>>10);
              int lo=0xdc00|(ch&0x3ff);
              dst[dstc++]='\\';
              dst[dstc++]='u';
              dst[dstc++]="0123456789abcdef"[(hi>>12)&15];
              dst[dstc++]="0123456789abcdef"[(hi>> 8)&15];
              dst[dstc++]="0123456789abcdef"[(hi>> 4)&15];
              dst[dstc++]="0123456789abcdef"[(hi    )&15];
              dst[dstc++]='\\';
              dst[dstc++]='u';
              dst[dstc++]="0123456789abcdef"[(lo>>12)&15];
              dst[dstc++]="0123456789abcdef"[(lo>> 8)&15];
              dst[dstc++]="0123456789abcdef"[(lo>> 4)&15];
              dst[dstc++]="0123456789abcdef"[(lo    )&15];
            } else {
              dstc+=12;
            }
          } else {
            if (dstc<=dsta-6) {
              dst[dstc++]='\\';
              dst[dstc++]='u';
              dst[dstc++]="0123456789abcdef"[(ch>>12)&15];
              dst[dstc++]="0123456789abcdef"[(ch>> 8)&15];
              dst[dstc++]="0123456789abcdef"[(ch>> 4)&15];
              dst[dstc++]="0123456789abcdef"[(ch    )&15];
            } else {
              dstc+=6;
            }
          }
        }
    }
  }
  if (dstc<dsta) dst[dstc]='"';
  dstc++;
  if (dstc<dsta) dst[dstc]=0;
  return dstc;
}

/* Measure string.
 */
 
int sr_string_measure(const char *src,int srcc,int *simple) {
  if (!src) return 0;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  if (srcc<1) return 0;
  if ((src[0]!='"')&&(src[0]!='\'')&&(src[0]!='`')) return 0;
  if (simple) *simple=1;
  int srcp=1;
  while (1) {
    if (srcp>=srcc) return 0;
    if (src[srcp]=='\\') {
      if (simple) *simple=0;
      srcp+=2;
    } else {
      if (src[srcp++]==src[0]) return srcp;
    }
  }
}

/* Boolean tokens.
 */

int sr_bool_eval(int *v,const char *src,int srcc) {
  if (!src) { *v=0; return 0; }
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  if (srcc<1) { *v=0; return 0; }
  
  if ((srcc==4)&&!memcmp(src,"true",4)) { *v=1; return 0; }
  if ((srcc==5)&&!memcmp(src,"false",5)) { *v=0; return 0; }
  if ((srcc==3)&&!memcmp(src,"yes",3)) { *v=1; return 0; }
  if ((srcc==2)&&!memcmp(src,"no",2)) { *v=0; return 0; }
  
  if (sr_int_eval(v,src,srcc)>=1) {
    *v=(*v)?1:0;
    return 0;
  }
  
  return -1;
}

int sr_bool_repr(char *dst,int dsta,int v) {
  if (v) {
    if (dsta>=4) memcpy(dst,"true",4);
    if (dsta>4) dst[4]=0;
    return 4;
  } else {
    if (dsta>=5) memcpy(dst,"false",5);
    if (dsta>5) dst[5]=0;
    return 5;
  }
}

/* VLQ.
 */

int sr_vlq_encode(void *dst,int dsta,int v) {
  uint8_t *DST=dst;
  if (v<0) return -1;
  if (v<0x80) {
    if (dsta>=1) {
      DST[0]=v;
    }
    return 1;
  }
  if (v<0x4000) {
    if (dsta>=2) {
      DST[0]=0x80|(v>>7);
      DST[1]=v&0x7f;
    }
    return 2;
  }
  if (v<0x200000) {
    if (dsta>=3) {
      DST[0]=0x80|(v>>14);
      DST[1]=0x80|(v>>7);
      DST[2]=v&0x7f;
    }
    return 3;
  }
  if (v<0x10000000) {
    if (dsta>=4) {
      DST[0]=0x80|(v>>21);
      DST[1]=0x80|(v>>14);
      DST[2]=0x80|(v>>7);
      DST[3]=v&0x7f;
    }
    return 4;
  }
  return -1;
}

int sr_vlq_decode(int *v,const void *src,int srcc) {
  if (srcc<1) return -1;
  const uint8_t *SRC=src;
  if (!(SRC[0]&0x80)) {
    *v=SRC[0];
    return 1;
  }
  if (srcc<2) return -1;
  if (!(SRC[1]&0x80)) {
    *v=((SRC[0]&0x7f)<<7)|SRC[1];
    return 2;
  }
  if (srcc<3) return -1;
  if (!(SRC[2]&0x80)) {
    *v=((SRC[0]&0x7f)<<14)|((SRC[1]&0x7f)<<7)|SRC[2];
    return 3;
  }
  if (srcc<4) return -1;
  if (!(SRC[3]&0x80)) {
    *v=((SRC[0]&0x7f)<<21)|((SRC[1]&0x7f)<<14)|((SRC[2]&0x7f)<<7)|SRC[3];
    return 4;
  }
  return -1;
}

/* UTF-8.
 */
 
int sr_utf8_encode(void *dst,int dsta,int v) {
  uint8_t *DST=dst;
  if (v<0) return -1;
  if (v<0x80) {
    if (dsta>=1) {
      DST[0]=v;
    }
    return 1;
  }
  if (v<0x800) {
    if (dsta>=2) {
      DST[0]=0xc0|(v>>6);
      DST[1]=0x80|(v&0x3f);
    }
    return 2;
  }
  if (v<0x10000) {
    if (dsta>=3) {
      DST[0]=0xe0|(v>>12);
      DST[1]=0x80|((v>>6)&0x3f);
      DST[2]=0x80|(v&0x3f);
    }
    return 3;
  }
  if (v<0x110000) {
    if (dsta>=4) {
      DST[0]=0xf0|(v>>18);
      DST[1]=0x80|((v>>12)&0x3f);
      DST[2]=0x80|((v>>6)&0x3f);
      DST[3]=0x80|(v&0x3f);
    }
    return 4;
  }
  return -1;
}

int sr_utf8_decode(int *v,const void *src,int srcc) {
  const uint8_t *SRC=src;
  if (srcc<1) return -1;
  if (!(SRC[0]&0x80)) {
    *v=SRC[0];
    return 1;
  }
  if (!(SRC[0]&0x40)) return -1;
  if (!(SRC[0]&0x20)) {
    if (srcc<2) return -1;
    if ((SRC[1]&0xc0)!=0x80) return -1;
    *v=((SRC[0]&0x1f)<<6)|(SRC[1]&0x3f);
    return 2;
  }
  if (!(SRC[0]&0x10)) {
    if (srcc<3) return -1;
    if ((SRC[1]&0xc0)!=0x80) return -1;
    if ((SRC[2]&0xc0)!=0x80) return -1;
    *v=((SRC[0]&0x0f)<<12)|((SRC[1]&0x3f)<<6)|(SRC[2]&0x3f);
    return 3;
  }
  if (!(SRC[0]&0x08)) {
    if (srcc<4) return -1;
    if ((SRC[1]&0xc0)!=0x80) return -1;
    if ((SRC[2]&0xc0)!=0x80) return -1;
    if ((SRC[3]&0xc0)!=0x80) return -1;
    *v=((SRC[0]&0x07)<<18)|((SRC[1]&0x3f)<<12)|((SRC[2]&0x3f)<<6)|(SRC[3]&0x3f);
    return 4;
  }
  return -1;
}

/* Pattern match.
 */
 
static int sr_pattern_match_1(const char *pat,int patc,const char *src,int srcc) {
  int patp=0,srcp=0;
  while (1) {
    
    // Termination of pattern.
    if (patp>=patc) {
      if (srcp>=srcc) return 1;
      return 0;
    }
    
    // Wildcard.
    if (pat[patp]=='*') {
      patp++;
      while ((patp<patc)&&(pat[patp]=='*')) patp++; // adjacent stars are redundant
      if (patp>=patc) return 1; // terminal star matches all, done.
      while (srcp<srcc) {
        if (sr_pattern_match_1(pat+patp,patc-patp,src+srcp,srcc-srcp)) return 1;
        srcp++;
      }
      return 0;
    }
    
    // Termination of src is a mismatch, now that we know there's no wildcard.
    if (srcp>=srcc) return 0;
    
    // Whitespace.
    if ((unsigned char)pat[patp]<=0x20) {
      if ((unsigned char)src[srcp]>0x20) return 0;
      patp++; while ((patp<patc)&&((unsigned char)pat[patp]<=0x20)) patp++;
      srcp++; while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
      continue;
    }
    
    // G0 letters are case-insensitive, and everything else is verbatim.
    char patch=pat[patp++]; if ((patch>='A')&&(patch<='Z')) patch+=0x20;
    char srcch=src[srcp++]; if ((srcch>='A')&&(srcch<='Z')) srcch+=0x20;
    if (patch!=srcch) return 0;
  }
}

int sr_pattern_match(const char *pat,int patc,const char *src,int srcc) {
  if (!pat) patc=0; else if (patc<0) { patc=0; while (pat[patc]) patc++; }
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  while (patc&&((unsigned char)pat[patc-1]<=0x20)) patc--;
  while (patc&&((unsigned char)pat[0]<=0x20)) { pat++; patc--; }
  while (srcc&&((unsigned char)src[srcc-1]<=0x20)) srcc--;
  while (srcc&&((unsigned char)src[0]<=0x20)) { src++; srcc--; }
  return sr_pattern_match_1(pat,patc,src,srcc);
}
