#include "arr.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdint.h>

/* Cleanup.
 */
 
void arr_cleanup(struct arr *arr) {
  if (arr->resv) free(arr->resv);
  memset(arr,0,sizeof(struct arr));
}

/* Append resource.
 */
 
static int arr_res_add(struct arr *arr,int pathp,int pathc,int bodyp,int bodyc) {
  if (arr->resc>=arr->resa) {
    int na=arr->resa=256;
    if (na>INT_MAX/sizeof(struct arr_res)) return -1;
    void *nv=realloc(arr->resv,sizeof(struct arr_res)*na);
    if (!nv) return -1;
    arr->resv=nv;
    arr->resa=na;
  }
  struct arr_res *res=arr->resv+arr->resc++;
  res->hash=arr_hash((char*)arr->src+pathp,pathc);
  res->path=(char*)arr->src+pathp;
  res->pathc=pathc;
  res->body=(char*)arr->src+bodyp;
  res->bodyc=bodyc;
  return 0;
}

/* Compare entries.
 */
 
static int arr_res_cmp(const void *a,const void *b) {
  const struct arr_res *A=a,*B=b;
  int d;
  if (d=A->hash-B->hash) return d;
  if (d=A->pathc-B->pathc) return d;
  return memcmp(A->path,B->path,A->pathc);
}

/* Init.
 */

int arr_init(struct arr *arr,const void *src,int srcc) {
  if (!src||(srcc<20)) return -1;
  if (memcmp(src,"\xff\x0egg",4)) return -1;
  arr_cleanup(arr);
  arr->src=src;
  arr->srcc=srcc;
  const uint8_t *SRC=src;
  
  // Header, and validate overall geometry.
  int hdrlen=(SRC[4]<<24)|(SRC[5]<<16)|(SRC[6]<<8)|SRC[7];
  int toclen=(SRC[8]<<24)|(SRC[9]<<16)|(SRC[10]<<8)|SRC[11];
  int pathlen=(SRC[12]<<24)|(SRC[13]<<16)|(SRC[14]<<8)|SRC[15];
  int bodylen=(SRC[16]<<24)|(SRC[17]<<16)|(SRC[18]<<8)|SRC[19];
  if ((hdrlen<0)||(toclen<0)||(pathlen<0)||(bodylen<0)) return -1;
  if (hdrlen<20) return -1;
  int toc0=hdrlen;
  if (toc0>srcc-toclen) return -1;
  int path0=toc0+toclen;
  if (path0>srcc-pathlen) return -1;
  int body0=path0+pathlen;
  if (body0>srcc-bodylen) return -1;
  
  // Iterate TOC, Paths, Bodies together.
  int tocp=0,pathp=0,bodyp=0,tocstop=toclen-4;
  while (tocp<=tocstop) {
    int rpathc=SRC[toc0+tocp++];
    int rbodyc=SRC[toc0+tocp++]<<16;
    rbodyc|=SRC[toc0+tocp++]<<8;
    rbodyc|=SRC[toc0+tocp++];
    if (pathp>pathlen-rpathc) return -1;
    if (bodyp>bodylen-rbodyc) return -1;
    if (arr_res_add(arr,path0+pathp,rpathc,body0+bodyp,rbodyc)<0) return -1;
    pathp+=rpathc;
    bodyp+=rbodyc;
  }
  
  qsort(arr->resv,arr->resc,sizeof(struct arr_res),arr_res_cmp);
  
  return 0;
}

/* Get resource.
 */

int arr_get(void *dstpp,const struct arr *arr,const char *path,int pathc) {
  if (!arr) return 0;
  if (!path) pathc=0; else if (pathc<0) { pathc=0; while (path[pathc]) pathc++; }
  int p=arr_search(arr,arr_hash(path,pathc),path,pathc);
  if (p<0) return 0;
  const struct arr_res *res=arr->resv+p;
  if (dstpp) *(const void**)dstpp=res->body;
  return res->bodyc;
}

/* Hash.
 */

int arr_hash(const char *path,int pathc) {
  unsigned int hash=0;
  for (;pathc-->0;path++) {
    hash=(hash<<3)|(hash>>29);
    hash^=(unsigned char)(*path);
  }
  return hash;
}

/* Search.
 */
 
int arr_search(const struct arr *arr,int hash,const char *path,int pathc) {
  int lo=0,hi=arr->resc;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
    const struct arr_res *q=arr->resv+ck;
         if (hash<q->hash) hi=ck;
    else if (hash>q->hash) lo=ck+1;
    else if (pathc<q->pathc) hi=ck;
    else if (pathc>q->pathc) lo=ck+1;
    else {
      int cmp=memcmp(path,q->path,pathc);
           if (cmp<0) hi=ck;
      else if (cmp>0) lo=ck+1;
      else return ck;
    }
  }
  return -lo-1;
}
