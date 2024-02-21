#include "arw_internal.h"

/* Cleanup.
 */
 
static void arw_res_cleanup(struct arw_res *res) {
  if (res->path) free(res->path);
  if (res->serial) free(res->serial);
  if (res->name) free(res->name);
}
 
void arw_cleanup(struct arw *arw) {
  if (arw->resv) {
    while (arw->resc-->0) arw_res_cleanup(arw->resv+arw->resc);
    free(arw->resv);
  }
  memset(arw,0,sizeof(struct arw));
}

/* Add resource, handoff.
 */

struct arw_res *arw_add_handoff(struct arw *arw,const char *path,int pathc,void *serial,int serialc) {
  if ((serialc<0)||(serialc&&!serial)) return 0;
  if (!path) pathc=0; else if (pathc<0) { pathc=0; while (path[pathc]) pathc++; }
  if (arw->resc>=arw->resa) {
    int na=arw->resa+256;
    if (na>INT_MAX/sizeof(struct arw_res)) return 0;
    void *nv=realloc(arw->resv,sizeof(struct arw_res)*na);
    if (!nv) return 0;
    arw->resv=nv;
    arw->resa=na;
  }
  char *npath=malloc(pathc+1);
  if (!npath) return 0;
  memcpy(npath,path,pathc);
  npath[pathc]=0;
  struct arw_res *res=arw->resv+arw->resc++;
  memset(res,0,sizeof(struct arw_res));
  res->path=npath;
  res->pathc=pathc;
  res->serial=serial;
  res->serialc=serialc;
  return res;
}

/* Add resource, conveniences.
 */
  
struct arw_res *arw_add_copy(struct arw *arw,const char *path,int pathc,const void *serial,int serialc) {
  if (serialc<0) return 0;
  void *nv=0;
  if (serialc) {
    if (!(nv=malloc(serialc))) return 0;
    memcpy(nv,serial,serialc);
  }
  struct arw_res *res=arw_add_handoff(arw,path,pathc,nv,serialc);
  if (!res) {
    if (nv) free(nv);
    return 0;
  }
  return res;
}

/* Search, replace, and remove existing resource.
 */
 
struct arw_res *arw_find(const struct arw *arw,const char *path,int pathc) {
  if (!path) pathc=0; else if (pathc<0) { pathc=0; while (path[pathc]) pathc++; }
  struct arw_res *res=arw->resv;
  int i=arw->resc;
  for (;i-->0;res++) {
    if (res->pathc!=pathc) continue;
    if (memcmp(res->path,path,pathc)) continue;
    return res;
  }
  return 0;
}

int arw_replace_handoff(struct arw *arw,const char *path,int pathc,void *serial,int serialc) {
  if ((serialc<0)||(serialc&&!serial)) return -1;
  struct arw_res *res=arw_find(arw,path,pathc);
  if (!res) return -1;
  if (res->serial) free(res->serial);
  res->serial=serial;
  res->serialc=serialc;
  return 0;
}

int arw_replace_copy(struct arw *arw,const char *path,int pathc,const void *serial,int serialc) {
  if (serialc<0) return -1;
  char *nv=0;
  if (serialc>0) {
    if (!serial) return -1;
    if (!(nv=malloc(serialc))) return -1;
    memcpy(nv,serial,serialc);
  }
  int err=arw_replace_handoff(arw,path,pathc,nv,serialc);
  if (err<0) free(nv);
  return err;
}

void arw_remove(struct arw *arw,const char *path,int pathc) {
  struct arw_res *res=arw_find(arw,path,pathc);
  if (!res) return;
  int p=res-arw->resv;
  if ((p<0)||(p>=arw->resc)) return;
  arw_res_cleanup(res);
  arw->resc--;
  memmove(res,res+1,sizeof(struct arw_res)*(arw->resc-p));
}

/* Res properties.
 */
 
int arw_res_set_name(struct arw_res *res,const char *src,int srcc) {
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  char *nv=0;
  if (srcc) {
    if (!(nv=malloc(srcc+1))) return -1;
    memcpy(nv,src,srcc);
    nv[srcc]=0;
  }
  if (res->name) free(res->name);
  res->name=nv;
  res->namec=srcc;
  return 0;
}

/* Finalize.
 */
 
int arw_finalize(char *msg,int msga,struct arw *arw) {
  if (!msg||(msga<0)) msga=0;
  if (msga>0) msg[0]=0;
  
  int detailsp=-1;
  struct arw_res *res=arw->resv;
  int i=arw->resc;
  for (;i-->0;res++) {
    if (res->pathc>0xff) {
      snprintf(msg,msga,"Input file name too long (%d, limit 255).",res->pathc);
      return -1;
    }
    if (res->serialc>0xffffff) {
      snprintf(msg,msga,"Input file '%.*s' exceeds limit 16777216 (%d).",res->pathc,res->path,res->serialc);
      return -1;
    }
    if ((res->pathc==7)&&!memcmp(res->path,"details",7)) {
      detailsp=res-arw->resv;
    }
  }
  
  if (detailsp>0) {
    struct arw_res tmp=arw->resv[detailsp];
    memmove(arw->resv+1,arw->resv,sizeof(struct arw_res)*detailsp);
    arw->resv[0]=tmp;
  } else {
    snprintf(msg,msga,"No 'details' file.");
  }
  
  return 0;
}

/* Encode.
 */
 
int arw_encode(struct sr_encoder *dst,const struct arw *arw) {
  
  // 20-byte header placeholder. Only the first 8 are final.
  int hdrp=dst->c;
  if (sr_encode_raw(dst,
    "\xff\x0egg" // signature
    "\0\0\0\x14" // header length
    "\0\0\0\0" // toc length
    "\0\0\0\0" // paths length
    "\0\0\0\0" // bodies length
  ,20)<0) return -1;
  
  #define INSLEN(hdrsubp,startp) { \
    int chunklen=dst->c-(startp); \
    uint8_t *lendst=(uint8_t*)dst->v+hdrp+hdrsubp; \
    lendst[0]=chunklen>>24; \
    lendst[1]=chunklen>>16; \
    lendst[2]=chunklen>>8; \
    lendst[3]=chunklen; \
  }
  
  // TOC.
  const struct arw_res *res=arw->resv;
  int i=arw->resc;
  int tocp=dst->c;
  for (;i-->0;res++) {
    if (res->pathc>0xff) return -1;
    if (res->serialc>0xffffff) return -1;
    if (sr_encode_u8(dst,res->pathc)<0) return -1;
    if (sr_encode_intbe(dst,res->serialc,3)<0) return -1;
  }
  INSLEN(8,tocp)
  
  // Paths.
  int pathsp=dst->c;
  for (res=arw->resv,i=arw->resc;i-->0;res++) {
    if (sr_encode_raw(dst,res->path,res->pathc)<0) return -1;
  }
  INSLEN(12,pathsp)
  
  // Bodies.
  int bodiesp=dst->c;
  for (res=arw->resv,i=arw->resc;i-->0;res++) {
    if (sr_encode_raw(dst,res->serial,res->serialc)<0) return -1;
  }
  INSLEN(16,bodiesp)
  
  #undef INSLEN
  return 0;
}
