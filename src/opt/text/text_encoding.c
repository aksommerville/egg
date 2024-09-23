#include "text_internal.h"

/* Decode UTF-8.
 */
 
int text_utf8_decode(int *codepoint,const void *src,int srcc) {
  if (srcc<1) return 0;
  const uint8_t *SRC=src;
  if (!(SRC[0]&0x80)) { *codepoint=SRC[0]; return 1; }
  if (!(SRC[0]&0x40)) return -1;
  if (!(SRC[0]&0x20)) {
    if (srcc<2) return -1;
    if ((SRC[1]&0xc0)!=0x80) return -1;
    *codepoint=((SRC[0]&0x1f)<<6)|(SRC[1]&0x3f);
    return 2;
  }
  if (!(SRC[0]&0x10)) {
    if (srcc<3) return -1;
    if ((SRC[1]&0xc0)!=0x80) return -1;
    if ((SRC[2]&0xc0)!=0x80) return -1;
    *codepoint=((SRC[0]&0x0f)<<12)|((SRC[1]&0x3f)<<6)|(SRC[2]&0x3f);
    return 3;
  }
  if (!(SRC[0]&0x08)) {
    if (srcc<4) return -1;
    if ((SRC[1]&0xc0)!=0x80) return -1;
    if ((SRC[2]&0xc0)!=0x80) return -1;
    if ((SRC[3]&0xc0)!=0x80) return -1;
    *codepoint=((SRC[0]&0x07)<<18)|((SRC[1]&0x3f)<<12)|((SRC[2]&0x3f)<<6)|(SRC[3]&0x3f);
    return 4;
  }
  return -1;
}
