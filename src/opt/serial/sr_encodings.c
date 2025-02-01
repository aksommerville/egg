#include "serial.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <limits.h>

// App programmer! You probably need to tweak these defines to get the right endian.h.
// SHA-1 uses it.
#if USE_macos
  #include <machine/endian.h>
#elif USE_mswin
  #define LITTLE_ENDIAN 1234
  #define BIG_ENDIAN 4321
  #define BYTE_ORDER LITTLE_ENDIAN
#else
  #include <endian.h>
#endif

/* Base64 encode.
 */
 
int sr_base64_encode(char *dst,int dsta,const void *src,int srcc) {
  if (!dst||(dsta<0)) dsta=0;
  if ((srcc<0)||(srcc&&!src)) return -1;

  // We emit without any extra formatting, so it's easy to determine the final output length in advance.
  // Do that, and get out quick if the buffer is short.
  int unitc=(srcc+2)/3;
  if (unitc>INT_MAX/4) return -1;
  int dstc=unitc*4;
  if (dstc>dsta) return dstc;

  // Emit the full units. This is either (unitc) or (unitc-1).
  const char *alphabet="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  const uint8_t *SRC=src;
  int fullunitc=unitc;
  if (srcc%3) fullunitc--;
  for (;fullunitc-->0;SRC+=3,dst+=4) {
    dst[0]=alphabet[SRC[0]>>2];
    dst[1]=alphabet[((SRC[0]<<4)|(SRC[1]>>4))&0x3f];
    dst[2]=alphabet[((SRC[1]<<2)|(SRC[2]>>6))&0x3f];
    dst[3]=alphabet[SRC[2]&0x3f];
  }

  // Emit the partial unit if there is one.
  switch (srcc%3) {
    case 0: break;
    case 1: {
        dst[0]=alphabet[SRC[0]>>2];
        dst[1]=alphabet[(SRC[0]<<4)&0x3f];
        dst[2]='=';
        dst[3]='=';
        dst+=4;
      } break;
    case 2: {
        dst[0]=alphabet[SRC[0]>>2];
        dst[1]=alphabet[((SRC[0]<<4)|(SRC[1]>>4))&0x3f];
        dst[2]=alphabet[(SRC[1]<<2)&0x3f];
        dst[3]='=';
        dst+=4;
      } break;
  }

  if (dstc<dsta) *dst=0;
  return dstc;
}

/* Base64 decode.
 */
 
int sr_base64_decode(void *dst,int dsta,const char *src,int srcc) {
  if (!dst||(dsta<0)) dsta=0;
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  uint8_t *DST=dst;
  int dstc=0,srcp=0;
  uint8_t tmp[4]; // inputs 0..63
  int tmpc=0;
  int finale=0; // how many '=' at the end
  while (srcp<srcc) {
    char indigit=src[srcp++];
    if ((unsigned char)indigit<=0x20) continue;
    if (indigit=='=') { finale++; continue; }
    if (finale) return -1;
    
    if ((indigit>='A')&&(indigit<='Z')) tmp[tmpc++]=indigit-'A';
    else if ((indigit>='a')&&(indigit<='z')) tmp[tmpc++]=indigit-'a'+26;
    else if ((indigit>='0')&&(indigit<='9')) tmp[tmpc++]=indigit-'0'+52;
    else if (indigit=='+') tmp[tmpc++]=62;
    else if (indigit=='/') tmp[tmpc++]=63;
    else return -1;

    if (tmpc>=4) {
      if (dstc<=dsta-3) {
        DST[dstc++]=(tmp[0]<<2)|(tmp[1]>>4);
        DST[dstc++]=(tmp[1]<<4)|(tmp[2]>>2);
        DST[dstc++]=(tmp[2]<<6)|tmp[3];
      } else dstc+=3;
      tmpc=0;
    }
  }
  // We could assert that (finale) takes us to the end of a unit, but I'm not sure the spec actually requires it, and also I don't care.
  switch (tmpc) {
    case 0: break; // Ended on a code unit, nice.
    case 1: { // tmpc==1 should not happen, but if so, assume the 2 missing low bits are zero.
        if (dstc<dsta) DST[dstc]=tmp[0]<<2;
        dstc++;
      } break;
    case 2: { // "AA==" => 1 byte
        if (dstc<dsta) DST[dstc]=(tmp[0]<<2)|(tmp[1]>>4);
        dstc++;
      } break;
    case 3: { // "AAA=" => 2 bytes
        if (dstc<=dsta-2) {
          DST[dstc++]=(tmp[0]<<2)|(tmp[1]>>4);
          DST[dstc++]=(tmp[1]<<4)|(tmp[2]>>2);
        } else dstc+=2;
      } break;
  }
  if (dstc<dsta) DST[dstc]=0;
  return dstc;
}

/* URL encode.
 */

int sr_url_encode(char *dst,int dsta,const char *src,int srcc) {
  if (!dst||(dsta<0)) dsta=0;
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  int dstc=0,srcp=0;
  for (;srcp<srcc;srcp++) {
    if (
      ((src[srcp]>='a')&&(src[srcp]<='z'))||
      ((src[srcp]>='A')&&(src[srcp]<='Z'))||
      ((src[srcp]>='0')&&(src[srcp]<='9'))||
      (src[srcp]=='!')||
      (src[srcp]=='\'')||
      (src[srcp]=='(')||
      (src[srcp]==')')||
      (src[srcp]=='*')||
      (src[srcp]=='-')||
      (src[srcp]=='.')||
      (src[srcp]=='_')||
      (src[srcp]=='~')||
    0) {
      if (dstc<dsta) dst[dstc]=src[srcp];
      dstc++;
    } else {
      if (dstc<=dsta-3) {
        dst[dstc++]='%';
        dst[dstc++]="0123456789ABCDEF"[(src[srcp]>>4)&15];
        dst[dstc++]="0123456789ABCDEF"[src[srcp]&15];
      } else dstc+=3;
    }
  }
  if (dstc<dsta) dst[dstc]=0;
  return dstc;
}

/* URL decode.
 */
 
int sr_url_decode(char *dst,int dsta,const char *src,int srcc) {
  if (!dst||(dsta<0)) dsta=0;
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  int dstc=0,srcp=0;
  while (srcp<srcc) {
    if (src[srcp]=='%') {
      if (srcp>srcc-3) return -1;
      int hi=sr_digit_eval(src[srcp+1]);
      int lo=sr_digit_eval(src[srcp+2]);
      if ((hi<0)||(hi>15)||(lo<0)||(lo>15)) return -1;
      if (dstc<dsta) dst[dstc]=(hi<<4)|lo;
      dstc++;
      srcp+=3;
    } else {
      if (dstc<dsta) dst[dstc]=src[srcp];
      dstc++;
      srcp++;
    }
  }
  if (dstc<dsta) dst[dstc]=0;
  return dstc;
}
