#include "http_internal.h"

/* Measure line through \r\n.
 */
 
int http_measure_line(const char *src,int srcc) {
  int srcp=0;
  int stopp=srcc-2;
  for (;;) {
    if (srcp>stopp) return 0;
    if ((src[srcp]==0x0d)&&(src[srcp+1]==0x0a)) return srcp+2;
    srcp++;
  }
}

/* Match path.
 */
 
static int http_path_match_inner(const char *pat,int patc,const char *src,int srcc) {
  int patp=0,srcp=0;
  while (1) {
    
    if (patp>=patc) {
      if (srcp>=srcc) return 1;
      return 0;
    }
    
    if (pat[patp]=='*') {
      patp++;
      if ((patp<patc)&&(pat[patp]=='*')) {
        patp++;
        if (patp>=patc) return 1; // Trailing double star matches everything, no need to check.
        while (srcp<srcc) {
          if (http_path_match_inner(pat+patp,patc-patp,src+srcp,srcc-srcp)) return 1;
          srcp++;
        }
      } else {
        while (srcp<srcc) {
          if (http_path_match_inner(pat+patp,patc-patp,src+srcp,srcc-srcp)) return 1;
          if (src[srcp]=='/') return 0;
          srcp++;
        }
      }
      return 0;
    }
    
    if (srcp>=srcc) return 0;
    
    if (pat[patp]!=src[srcp]) return 0;
    patp++;
    srcp++;
  }
}
 
int http_path_match(const char *pat,int patc,const char *src,int srcc) {
  if (!pat) patc=0; else if (patc<0) { patc=0; while (pat[patc]) patc++; }
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  while (patc&&(pat[patc-1]=='/')) patc--;
  while (patc&&(pat[0]=='/')) { patc--; pat++; }
  while (srcc&&(src[srcc-1]=='/')) srcc--;
  while (srcc&&(src[0]=='/')) { srcc--; src++; }
  return http_path_match_inner(pat,patc,src,srcc);
}

/* Split URL.
 */
 
int http_url_split(struct http_url *dst,const char *src,int srcc) {
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  memset(dst,0,sizeof(struct http_url));
  int srcp=0;
  
  /* First token up to :/.?#
   */
  const char *token=src+srcp;
  int tokenc=0;
  while ((srcp<srcc)&&(src[srcp]!=':')&&(src[srcp]!='/')&&(src[srcp]!='.')&&(src[srcp]!='?')&&(src[srcp]!='#')) {
    tokenc++;
    srcp++;
  }
  
  /* If we're stopped at "://", consume it, and this token is the scheme.
   * Otherwise token is the start of host, and read on thru any dots.
   */
  if (tokenc) {
    if ((srcp<=srcc-3)&&!memcmp(src+srcp,"://",3)) {
      dst->scheme=token;
      dst->schemec=tokenc;
      srcp+=3;
      dst->host=src+srcp;
    } else {
      dst->host=token;
      dst->hostc=tokenc;
    }
    while ((srcp<srcc)&&(src[srcp]!=':')&&(src[srcp]!='/')&&(src[srcp]!='?')&&(src[srcp]!='#')) {
      dst->hostc++;
      srcp++;
    }
    
  /* But if no token was read, check for "//HOST..."
   */
  } else if ((srcp<=srcc-2)&&(src[srcp]=='/')&&(src[srcp+1]=='/')) {
    srcp+=2;
    dst->host=src+srcp;
    while ((srcp<srcc)&&(src[srcp]!=':')&&(src[srcp]!='/')&&(src[srcp]!='?')&&(src[srcp]!='#')) {
      dst->hostc++;
      srcp++;
    }
    
  /* Otherwise we must insist at least that a path be present.
   */
  } else if ((srcp>=srcc)||(src[srcp]!='/')) {
    return -1;
  }
  
  /* Then port, path, query, fragment all read a bit neater.
   */
  if ((srcp<srcc)&&(src[srcp]==':')) {
    srcp++;
    dst->port=src+srcp;
    while ((srcp<srcc)&&(src[srcp]>='0')&&(src[srcp]<='9')) { srcp++; dst->portc++; }
  }
  if ((srcp<srcc)&&(src[srcp]=='/')) {
    dst->path=src+srcp;
    while ((srcp<srcc)&&(src[srcp]!='?')&&(src[srcp]!='#')) { srcp++; dst->pathc++; }
  }
  if ((srcp<srcc)&&(src[srcp]=='?')) {
    dst->query=src+srcp;
    while ((srcp<srcc)&&(src[srcp]!='#')) { srcp++; dst->queryc++; }
  }
  if ((srcp<srcc)&&(src[srcp]=='#')) {
    dst->fragment=src+srcp;
    dst->fragmentc=srcc-srcp;
    srcp=srcc;
  }
  
  if (srcp<srcc) return -1;
  return 0;
}

/* Dispatch server requests.
 */
 
int http_dispatch_(struct http_xfer *req,struct http_xfer *rsp,...) {

  int method=0;
  char methodstr[16];
  int methodstrc=http_xfer_get_method(methodstr,sizeof(methodstr),req);
  if ((methodstrc<0)||(methodstrc>=sizeof(methodstr))) method=0;
  else if (!strcmp(methodstr,"GET")) method=HTTP_METHOD_GET;
  else if (!strcmp(methodstr,"POST")) method=HTTP_METHOD_POST;
  else if (!strcmp(methodstr,"PUT")) method=HTTP_METHOD_PUT;
  else if (!strcmp(methodstr,"DELETE")) method=HTTP_METHOD_DELETE;
  else if (!strcmp(methodstr,"PATCH")) method=HTTP_METHOD_PATCH;
  
  const char *path=0;
  int pathc=http_xfer_get_path(&path,req);
  if (pathc<0) pathc=0;
  
  va_list vargs;
  va_start(vargs,rsp);
  for (;;) {
    int qmethod=va_arg(vargs,int);
    if (qmethod<0) break;
    const char *qpath=va_arg(vargs,const char *);
    int (*cb)(struct http_xfer *req,struct http_xfer *rsp)=va_arg(vargs,void*);
    if (qmethod&&(qmethod!=method)) continue;
    if (qpath&&qpath[0]&&!http_path_match(qpath,-1,path,pathc)) continue;
    if (!cb) return -1;
    return cb(req,rsp);
  }
  return -1;
}
