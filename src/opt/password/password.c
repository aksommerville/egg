#include "password.h"

/* XOR against neighbor.
 */
 
void password_filter(void *v,int c) {
  unsigned char *V=v;
  int i=1; for (;i<c;i++,V++) V[1]^=V[0];
}

void password_unfilter(void *v,int c) {
  unsigned char *V=v;
  int i=c; while (i-->1) V[i]^=V[i-1];
}

/* Checksum.
 */

unsigned int password_checksum(const void *v,int c) {
  unsigned int sum=0xaa55e718;
  const unsigned char *V=v;
  for (;c-->0;V++) {
    sum=(sum<<1)|(sum>>31);
    sum^=*V;
  }
  return sum;
}

/* Add index.
 */

void password_add_index(void *v,int c) {
  unsigned char *V=v;
  int i=0; for (;i<c;i++,V++) (*V)+=i;
}

void password_sub_index(void *v,int c) {
  unsigned char *V=v;
  int i=0; for (;i<c;i++,V++) (*V)-=i;
}

/* Base64 primitives.
 */
 
static const char password_b64_alphabet[]=
  "#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[" // 0x23..0x5b = 57
  "]^_`abc" // 0x5d..0x63 = 7
;

static inline int password_b64_decode_1(char src) {
  if ((src>=0x23)&&(src<=0x5b)) return src-0x23;
  if ((src>=0x5d)&&(src<=0x63)) return src-0x5d+57;
  return -1;
}

/* Base64 buffers.
 */

int password_b64_encode(char *dst,int dsta,const void *src,int srcc) {
  if ((srcc<3)||(srcc%3)||!src||((dsta>0)&&!dst)) return -1;
  int i=srcc/3;
  int dstc=i*4;
  if (dstc>dsta) return dstc;
  const unsigned char *SRC=src;
  for (;i-->0;SRC+=3,dst+=4) {
    dst[0]=password_b64_alphabet[(SRC[0]>>2)&0x3f];
    dst[1]=password_b64_alphabet[((SRC[0]<<4)|(SRC[1]>>4))&0x3f];
    dst[2]=password_b64_alphabet[((SRC[1]<<2)|(SRC[2]>>6))&0x3f];
    dst[3]=password_b64_alphabet[SRC[2]&0x3f];
  }
  return dstc;
}
 
int password_b64_decode(void *dst,int dsta,const char *src,int srcc) {
  if ((srcc<4)||(srcc&3)||!src||((dsta>0)&&!dst)) return -1;
  int i=srcc>>2;
  int dstc=i*3;
  if (dstc>dsta) return dstc;
  unsigned char *DST=dst;
  for (;i-->0;DST+=3,src+=4) {
    int a=password_b64_decode_1(src[0]); if (a<0) return -1;
    int b=password_b64_decode_1(src[1]); if (b<0) return -1;
    int c=password_b64_decode_1(src[2]); if (c<0) return -1;
    int d=password_b64_decode_1(src[3]); if (d<0) return -1;
    DST[0]=(a<<2)|(b>>4);
    DST[1]=(b<<4)|(c>>2);
    DST[2]=(c<<6)|d;
  }
  return dstc;
}
