#include "egg-stdlib.h"

void *memcpy(void *dst,const void *src,unsigned long c) {
  if (((*(int*)dst)&3)||((*(int*)src)&3)||(c&3)) {
    char *DST=dst;
    const char *SRC=src;
    for (;c-->0;DST++,SRC++) *DST=*SRC;
  } else {
    int *DST=dst;
    const int *SRC=src;
    c>>=2;
    for (;c-->0;DST++,SRC++) *DST=*SRC;
  }
  return dst;
}

void *memmove(void *dst,const void *src,int c) {
  if (dst<src) {
    memcpy(dst,src,c);
  } else {
    char *DST=(char*)dst+c;
    const char *SRC=(const char*)src+c;
    while (c-->0) *(--DST)=*(--SRC);
  }
  return dst;
}

int memcmp(const void *a,const void *b,int c) {
  if (a==b) return 0;
  if (!a) return -1;
  if (!b) return 1;
  const unsigned char *A=a,*B=b;
  for (;c-->0;A++,B++) {
    int cmp=*A-*B;
    if (cmp) return cmp;
  }
  return 0;
}

int strncmp(const char *a,const char *b,int limit) {
  if (a==b) return 0;
  if (!a) return -1;
  if (!b) return 1;
  for (;limit-->0;a++,b++) {
    int cmp=*a-*b;
    if (cmp) return cmp;
  }
  return 0;
}

char *strdup(const char *src) {
  if (!src) return 0;
  int c=0;
  while (src[c]) c++;
  char *dst=malloc(c+1);
  if (!dst) return 0;
  memcpy(dst,src,c);
  dst[c]=0;
  return dst;
}

void *memset(void *dst,unsigned char src,int c) {
  unsigned char *DST=dst;
  for (;c-->0;DST++) *DST=src;
  return dst;
}
