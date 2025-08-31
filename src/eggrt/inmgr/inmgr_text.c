#include "inmgr_internal.h"

/* Button ID.
 */
 
int inmgr_btnid_eval(int *dst,const char *src,int srcc) {
  if (!src) return -1;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  // Force uppercase. If it's too long, just use it as is.
  char norm[32];
  if (srcc<=sizeof(norm)) {
    int i=srcc; while (i-->0) {
      if ((src[i]>='a')&&(src[i]<='z')) norm[i]=src[i]-0x20;
      else norm[i]=src[i];
    }
    src=norm;
  }
  #define _(tag) if ((srcc==sizeof(#tag)-1)&&!memcmp(src,#tag,srcc)) { *dst=INMGR_BTN_##tag; return 0; }
  INMGR_FOR_EACH_BUTTON
  #undef _
  return inmgr_hexuint_eval(dst,src,srcc);
}

int inmgr_btnid_repr(char *dst,int dsta,int btnid) {
  const char *src=0;
  int srcc=0;
  switch (btnid) {
    #define _(tag) case INMGR_BTN_##tag: src=#tag; srcc=sizeof(#tag)-1; break;
    INMGR_FOR_EACH_BUTTON
    #undef _
  }
  if (!src) return inmgr_hexuint_repr(dst,dsta,btnid);
  if (srcc<=dsta) {
    memcpy(dst,src,srcc);
    if (srcc<dsta) dst[srcc]=0;
  }
  return srcc;
}

/* Unsigned hexadecimal integer.
 */
 
int inmgr_hexuint_eval(int *dst,const char *src,int srcc) {
  if (!src) return -1;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  if ((srcc>2)&&(!memcmp(src,"0x",2)||!memcmp(src,"0X",2))) { src+=2; srcc-=2; }
  if (!srcc) return -1;
  *dst=0;
  for (;srcc-->0;src++) {
    int digit=*src;
         if ((digit>='0')&&(digit<='9')) digit=digit-'0';
    else if ((digit>='a')&&(digit<='f')) digit=digit-'a'+10;
    else if ((digit>='A')&&(digit<='F')) digit=digit-'A'+10;
    else return -1;
    if ((*dst)&0xf0000000) return -1;
    (*dst)<<=4;
    (*dst)|=digit;
  }
  return 0;
}

int inmgr_hexuint_repr(char *dst,int dsta,int src) {
  int dstc=1,limit=~0xf;
  while (src&limit) { dstc++; limit<<=4; }
  if (dstc<=dsta) {
    int i=dstc;
    for (;i-->0;src>>=4) dst[i]="0123456789abcdef"[src&15];
    if (dstc<dsta) dst[dstc]=0;
  }
  return dstc;
}

/* Check for verbatim token in comment.
 */
 
int inmgr_comment_has_token(const char *comment,int commentc,const char *token,int tokenc) {
  if (!comment||!token) return 0;
  if (commentc<0) { commentc=0; while (comment[commentc]) commentc++; }
  if (tokenc<0) { tokenc=0; while (token[tokenc]) tokenc++; }
  int commentp=0;
  while (commentp<commentc) {
    if ((unsigned char)comment[commentp]<=0x20) {
      commentp++;
      continue;
    }
    const char *q=comment+commentp;
    int qc=0;
    while ((commentp<commentc)&&((unsigned char)comment[commentp++]>0x20)) qc++;
    if ((qc==tokenc)&&!memcmp(q,token,tokenc)) return 1;
  }
  return 0;
}
