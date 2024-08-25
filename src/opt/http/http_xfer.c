#include "http_internal.h"

/* Delete.
 */
 
static void http_header_cleanup(struct http_header *header) {
  if (header->v) free(header->v);
}

void http_xfer_del(struct http_xfer *xfer) {
  if (!xfer) return;
  sr_encoder_cleanup(&xfer->body);
  if (xfer->topline) free(xfer->topline);
  if (xfer->headerv) {
    while (xfer->headerc-->0) http_header_cleanup(xfer->headerv+xfer->headerc);
    free(xfer->headerv);
  }
  free(xfer);
}

/* New.
 */

struct http_xfer *http_xfer_new(struct http_context *ctx) {
  struct http_xfer *xfer=calloc(1,sizeof(struct http_xfer));
  if (!xfer) return 0;
  xfer->ctx=ctx;
  return xfer;
}

/* Configure for client request.
 */

int http_xfer_configure_client_request(
  struct http_xfer *xfer,
  const char *method,
  const char *url,int urlc,
  int (*cb)(struct http_xfer *req,struct http_xfer *rsp),
  void *userdata
) {
  struct http_url surl={0};
  if (http_url_split(&surl,url,urlc)<0) return -1;
  char tmp[1024];
  int tmpc=snprintf(tmp,sizeof(tmp),"%s %.*s HTTP/1.1",method,surl.pathc+surl.queryc,surl.path);
  if ((tmpc<1)||(tmpc>=sizeof(tmp))) return -1;
  if (http_xfer_set_topline(xfer,tmp,tmpc)<0) return -1;
  xfer->cb=cb;
  xfer->userdata=userdata;
  return 0;
}

/* Cancel request.
 */

void http_request_cancel(struct http_xfer *req) {
  struct http_socket *sock=http_context_socket_for_request(req->ctx,req);
  if (!sock) return;
  http_socket_force_defunct(sock);
}

/* Trivial accessors.
 */

void *http_xfer_get_userdata(const struct http_xfer *xfer) {
  if (!xfer) return 0;
  return xfer->userdata;
}

void http_xfer_set_userdata(struct http_xfer *xfer,void *userdata) {
  xfer->userdata=userdata;
}

int http_xfer_get_topline(void *dstpp,const struct http_xfer *xfer) {
  *(void**)dstpp=xfer->topline;
  return xfer->toplinec;
}

int http_xfer_set_topline(struct http_xfer *xfer,const char *src,int srcc) {
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  while (srcc&&((unsigned char)src[srcc-1]<=0x20)) srcc--;
  while (srcc&&((unsigned char)src[0]<=0x20)) { srcc--; src++; }
  char *nv=0;
  if (srcc) {
    if (!(nv=malloc(srcc+1))) return -1;
    memcpy(nv,src,srcc);
    nv[srcc]=0;
  }
  if (xfer->topline) free(xfer->topline);
  xfer->topline=nv;
  xfer->toplinec=srcc;
  return 0;
}

struct sr_encoder *http_xfer_get_body(struct http_xfer *xfer) {
  return &xfer->body;
}

/* Topline conveniences.
 */
 
int http_xfer_get_method(char *dst,int dsta,const struct http_xfer *xfer) {
  const char *src=xfer->topline;
  int srcc=0;
  while ((srcc<xfer->toplinec)&&((unsigned char)src[srcc]>0x20)) srcc++;
  if (srcc>dsta) return srcc;
  int i=srcc;
  while (i-->0) {
    if ((src[i]>='a')&&(src[i]<='z')) dst[i]=src[i]-0x20;
    else dst[i]=src[i];
  }
  if (srcc<dsta) dst[srcc]=0;
  return srcc;
}

int http_xfer_get_path(void *dstpp,const struct http_xfer *xfer) {
  const char *src=xfer->topline;
  int srcc=xfer->toplinec;
  int srcp=0;
  while ((srcp<srcc)&&((unsigned char)src[srcp]>0x20)) srcp++; // method
  while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
  *(const void**)dstpp=src+srcp;
  int dstc=0;
  while ((srcp<srcc)&&((unsigned char)src[srcp]>0x20)&&(src[srcp]!='?')) { srcp++; dstc++; }
  return dstc;
}

int http_xfer_get_query(void *dstpp,const struct http_xfer *xfer) {
  const char *src=xfer->topline;
  int srcc=xfer->toplinec;
  int srcp=0;
  while ((srcp<srcc)&&((unsigned char)src[srcp]>0x20)) srcp++; // method
  while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
  while ((srcp<srcc)&&((unsigned char)src[srcp]>0x20)&&(src[srcp]!='?')) srcp++; // path
  int dstc=0;
  if ((srcp<srcc)&&(src[srcp]=='?')) {
    srcp++;
    *(const void**)dstpp=src+srcp;
    while ((srcp<srcc)&&((unsigned char)src[srcp]>0x20)) { srcp++; dstc++; }
  }
  return dstc;
}

int http_xfer_get_status(const struct http_xfer *xfer) {
  const char *src=xfer->topline;
  int srcc=xfer->toplinec;
  int srcp=0;
  while ((srcp<srcc)&&((unsigned char)src[srcp]>0x20)) srcp++; // protocol
  while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
  int status=0;
  while ((srcp<srcc)&&((unsigned char)src[srcp]>0x20)) {
    int digit=src[srcp++]-'0';
    if ((digit<0)||(digit>9)) return 0;
    status*=10;
    status+=digit;
  }
  return status;
}

int http_xfer_get_message(void *dstpp,const struct http_xfer *xfer) {
  const char *src=xfer->topline;
  int srcc=xfer->toplinec;
  int srcp=0;
  while ((srcp<srcc)&&((unsigned char)src[srcp]>0x20)) srcp++; // protocol
  while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
  while ((srcp<srcc)&&((unsigned char)src[srcp]>0x20)) srcp++; // status
  while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
  *(const void**)dstpp=src+srcp;
  return srcc-srcp;
}

int http_xfer_set_status(struct http_xfer *xfer,int status,const char *fmt,...) {
  char tmp[1024];
  int tmpc=snprintf(tmp,sizeof(tmp),"HTTP/1.1 %03d ",status);
  if (fmt&&fmt[0]) {
    va_list vargs;
    va_start(vargs,fmt);
    int err=vsnprintf(tmp+tmpc,sizeof(tmp)-tmpc,fmt,vargs);
    if ((err>0)&&(tmpc+err<sizeof(tmp))) tmpc+=err;
  }
  return http_xfer_set_topline(xfer,tmp,tmpc);
}

/* Iterate headers.
 */

int http_xfer_for_each_header(
  const struct http_xfer *xfer,
  int (*cb)(const char *k,int kc,const char *v,int vc,void *userdata),
  void *userdata
) {
  struct http_header *header=xfer->headerv;
  int i=xfer->headerc,err;
  for (;i-->0;header++) {
    const char *v=header->v+header->kc;
    int vc=header->c-header->kc;
    if ((vc>0)&&(v[0]==':')) { v++; vc--; }
    while (vc&&((unsigned char)v[0]<=0x20)) { v++; vc--; }
    if (err=cb(header->v,header->kc,v,vc,userdata)) return err;
  }
  return 0;
}

/* Add header blindly.
 */

int http_xfer_add_header(struct http_xfer *xfer,const char *k,int kc,const char *v,int vc) {
  if (xfer->headerc>=xfer->headera) {
    int na=xfer->headera+8;
    if (na>INT_MAX/sizeof(struct http_header)) return -1;
    void *nv=realloc(xfer->headerv,sizeof(struct http_header)*na);
    if (!nv) return -1;
    xfer->headerv=nv;
    xfer->headera=na;
  }
  if (!k) kc=0; else if (kc<0) { kc=0; while (k[kc]) kc++; }
  if (!v) vc=0; else if (vc<0) { vc=0; while (v[vc]) vc++; }
  int nc=kc+2+vc;
  char *nv=malloc(nc+1);
  if (!nv) return -1;
  memcpy(nv,k,kc);
  memcpy(nv+kc,": ",2);
  memcpy(nv+kc+2,v,vc);
  nv[nc]=0;
  struct http_header *header=xfer->headerv+xfer->headerc++;
  header->v=nv;
  header->c=nc;
  header->kc=kc;
  return 0;
}

/* Replace or add header.
 */

int http_xfer_set_header(struct http_xfer *xfer,const char *k,int kc,const char *v,int vc) {
  if (!k) kc=0; else if (kc<0) { kc=0; while (k[kc]) kc++; }
  if (!v) vc=0; else if (vc<0) { vc=0; while (v[vc]) vc++; }
  struct http_header *header=xfer->headerv;
  int i=xfer->headerc;
  for (;i-->0;header++) {
    if (header->kc!=kc) continue;
    if (sr_memcasecmp(header->v,k,kc)) continue;
    int nc=kc+2+vc;
    if (nc<=header->c) {
      memcpy(header->v+kc+2,v,vc);
      header->v[nc]=0;
      header->c=nc;
    } else {
      char *nv=malloc(nc+1);
      if (!nv) return 1;
      memcpy(nv,k,kc);
      memcpy(nv+kc,": ",2);
      memcpy(nv+kc+2,v,vc);
      nv[nc]=0;
      if (header->v) free(header->v);
      header->v=nv;
      header->kc=kc;
      header->c=nc;
    }
    return 0;
  }
  return http_xfer_add_header(xfer,k,kc,v,vc);
}

/* Get header by key.
 */
 
int http_xfer_get_header(void *dstpp,const struct http_xfer *xfer,const char *k,int kc) {
  if (!k) kc=0; else if (kc<0) { kc=0; while (k[kc]) kc++; }
  const struct http_header *header=xfer->headerv;
  int i=xfer->headerc;
  for (;i-->0;header++) {
    if (header->kc!=kc) continue;
    if (sr_memcasecmp(header->v,k,kc)) continue;
    *(void**)dstpp=header->v+kc+2;
    return header->c-kc-2;
  }
  return 0;
}

int http_xfer_get_header_int(int *v,const struct http_xfer *xfer,const char *k,int kc) {
  const char *tmp=0;
  int tmpc=http_xfer_get_header(&tmp,xfer,k,kc);
  if (tmpc<1) return -1;
  if (sr_int_eval(v,tmp,tmpc)<2) return -1;
  return 0;
}

/* Iterate query params.
 */
 
static int http_xfer_for_each_param_urlencoded(
  const struct http_xfer *xfer,
  int (*cb)(const char *k,int kc,const char *v,int vc,void *userdata),
  void *userdata,
  const char *src,int srcc
) {
  int srcp=0,err;
  while (srcp<srcc) {
    const char *k=src+srcp,*v=0;
    int kc=0,vc=0;
    while ((srcp<srcc)&&(src[srcp]!='=')&&(src[srcp]!='&')) { srcp++; kc++; }
    if ((srcp<srcc)&&(src[srcp]=='=')) {
      srcp++;
      v=src+srcp;
      while ((srcp<srcc)&&(src[srcp]!='&')) { srcp++; vc++; }
    }
    if (err=cb(k,kc,v,vc,userdata)) return err;
  }
  return 0;
}

static int http_xfer_for_each_param_multipart(
  const struct http_xfer *xfer,
  int (*cb)(const char *k,int kc,const char *v,int vc,void *userdata),
  void *userdata,
  const char *src,int srcc
) {
  //TODO Query parameters from "multipart/form-data"
  return 0;
}

int http_xfer_for_each_param(
  const struct http_xfer *xfer,
  int (*cb)(const char *k,int kc,const char *v,int vc,void *userdata),
  void *userdata
) {
  int err;
  const char *query=0;
  int queryc=http_xfer_get_query(&query,xfer);
  if (queryc>0) {
    if (err=http_xfer_for_each_param_urlencoded(xfer,cb,userdata,query,queryc)) return err;
  }
  const char *ct=0;
  int ctc=http_xfer_get_header(&ct,xfer,"Content-Type",12);
  if ((ctc==33)&&!memcmp(ct,"application/x-www-form-urlencoded",33)) {
    if (err=http_xfer_for_each_param_urlencoded(xfer,cb,userdata,xfer->body.v,xfer->body.c)) return err;
  } else if ((ctc==19)&&!memcmp(ct,"multipart/form-data",19)) {
    if (err=http_xfer_for_each_param_multipart(xfer,cb,userdata,xfer->body.v,xfer->body.c)) return err;
  }
  return 0;
}

/* Get parameter by key.
 */

struct http_xfer_get_param_ctx {
  const char *k;
  int kc;
  char *dst;
  int dsta;
  int dstc;
};

static int http_xfer_get_param_cb(const char *k,int kc,const char *v,int vc,void *userdata) {
  struct http_xfer_get_param_ctx *ctx=userdata;
  if (kc!=ctx->kc) return 0;
  if (memcmp(k,ctx->k,kc)) return 0;
  if ((ctx->dstc=sr_url_decode(ctx->dst,ctx->dsta,v,vc))<0) return -1;
  return 1;
}
 
int http_xfer_get_param(char *dst,int dsta,const struct http_xfer *xfer,const char *k,int kc) {
  if (!k) return -1;
  if (kc<0) { kc=0; while (k[kc]) kc++; }
  struct http_xfer_get_param_ctx ctx={
    .k=k,
    .kc=kc,
    .dst=dst,
    .dsta=dsta,
    .dstc=-1,
  };
  if (http_xfer_for_each_param(xfer,http_xfer_get_param_cb,&ctx)<=0) return -1;
  return ctx.dstc;
}

int http_xfer_get_param_int(int *v,const struct http_xfer *xfer,const char *k,int kc) {
  char tmp[32];
  int tmpc=http_xfer_get_param(tmp,sizeof(tmp),xfer,k,kc);
  if (tmpc<1) return -1;
  if (sr_int_eval(v,tmp,tmpc)<2) return -1;
  return 0;
}

/* Encode.
 */

int http_xfer_encode(struct sr_encoder *dst,const struct http_xfer *xfer) {
  if (sr_encode_raw(dst,xfer->topline,xfer->toplinec)<0) return -1;
  if (sr_encode_raw(dst,"\r\n",2)<0) return -1;
  const struct http_header *header=xfer->headerv;
  int i=xfer->headerc;
  for (;i-->0;header++) {
    if (sr_encode_raw(dst,header->v,header->c)<0) return -1;
    if (sr_encode_raw(dst,"\r\n",2)<0) return -1;
  }
  if (sr_encode_fmt(dst,"Content-Length: %d\r\n",xfer->body.c)<0) return -1;
  if (sr_encode_raw(dst,"\r\n",2)<0) return -1;
  if (sr_encode_raw(dst,xfer->body.v,xfer->body.c)<0) return -1;
  return 0;
}

/* Decode. XXX We ended up not using this; this logic lives in socket instead.
 */
 
int http_xfer_decode(struct http_xfer *xfer,const void *src,int srcc) {
  fprintf(stderr,"%s XXX\n",__func__);
  return -1;
}

/* Check fully populated.
 */
 
int http_xfer_is_decoded(const struct http_xfer *xfer) {
  if (!xfer) return 0;
  if (!xfer->toplinec) return 0;
  return 1;
}
