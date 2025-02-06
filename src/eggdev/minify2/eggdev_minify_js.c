#include "mf_internal.h"
#include <stdarg.h>

/* Cleanup.
 */
 
static void mf_file_cleanup(struct mf_file *file) {
  if (file->path) free(file->path);
  if (file->src) free(file->src);
}

static void mf_textcache_cleanup(struct mf_textcache *tc) {
  if (tc->v) free(tc->v);
}
 
void eggdev_minify_js_cleanup(struct eggdev_minify_js *ctx) {
  if (ctx->filev) {
    while (ctx->filec-->0) mf_file_cleanup(ctx->filev+ctx->filec);
    free(ctx->filev);
  }
  if (ctx->textcachev) {
    while (ctx->textcachec-->0) mf_textcache_cleanup(ctx->textcachev+ctx->textcachec);
    free(ctx->textcachev);
  }
  mf_node_del(ctx->root);
}

/* File list.
 */
 
struct mf_file *mf_js_add_file(struct eggdev_minify_js *ctx,const char *path,const void *src,int srcc) {
  if (ctx->filec>=ctx->filea) {
    int na=ctx->filea+8;
    if (na>INT_MAX/sizeof(struct mf_file)) return 0;
    void *nv=realloc(ctx->filev,sizeof(struct mf_file)*na);
    if (!nv) return 0;
    ctx->filev=nv;
    ctx->filea=na;
  }
  void *nv=0;
  if (!src) {
    if ((srcc=file_read(&nv,path))<0) return 0;
  } else if (srcc>0) {
    if (!(nv=malloc(srcc))) return 0;
    memcpy(nv,src,srcc);
  }
  struct mf_file *file=ctx->filev+ctx->filec++;
  memset(file,0,sizeof(struct mf_file));
  file->src=nv;
  file->srcc=srcc;
  if (!(file->path=strdup(path))) {
    ctx->filec--;
    mf_file_cleanup(file);
    return 0;
  }
  file->fileid=ctx->filec;
  return file;
}

struct mf_file *mf_js_get_file_by_path(struct eggdev_minify_js *ctx,const char *path) {
  struct mf_file *file=ctx->filev;
  int i=ctx->filec;
  for (;i-->0;file++) {
    if (!strcmp(file->path,path)) return file;
  }
  return 0;
}

struct mf_file *mf_js_get_file_by_id(struct eggdev_minify_js *ctx,int fileid) {
  struct mf_file *file=ctx->filev;
  int i=ctx->filec;
  for (;i-->0;file++) {
    if (file->fileid==fileid) return file;
  }
  return 0;
}

/* Text cache.
 */
 
char *mf_js_text_intern(struct eggdev_minify_js *ctx,const char *src,int srcc) {
  if (!ctx||(srcc<0)||(srcc&&!src)) return 0;
  if (!srcc) return "";
  // We could search for existing instance of this string in (textcachev) but I doubt it's worth the effort.
  struct mf_textcache *tc=ctx->textcachev;
  int i=ctx->textcachec;
  for (;i-->0;tc++) {
    if (tc->c<=tc->a-srcc) {
      char *dst=tc->v+tc->c;
      memcpy(dst,src,srcc);
      tc->c+=srcc;
      return dst;
    }
  }
  if (ctx->textcachec>=ctx->textcachea) {
    int na=ctx->textcachea+8;
    if (na>INT_MAX/sizeof(struct mf_textcache)) return 0;
    void *nv=realloc(ctx->textcachev,sizeof(struct mf_textcache)*na);
    if (!nv) return 0;
    ctx->textcachev=nv;
    ctx->textcachea=na;
  }
  tc=ctx->textcachev+ctx->textcachec++;
  memset(tc,0,sizeof(struct mf_textcache));
  tc->a=srcc;
  if (tc->a<INT_MAX-4096) tc->a=(tc->a+4096)&~4095;
  if (!(tc->v=malloc(tc->a))) {
    ctx->textcachec--;
    return 0;
  }
  memcpy(tc->v,src,srcc);
  tc->c=srcc;
  return tc->v;
}

/* Log errors.
 */
 
int mf_jserr(struct eggdev_minify_js *ctx,struct mf_token *token,const char *fmt,...) {
  
  // First the path and line number.
  if (token&&ctx) {
    struct mf_file *file=mf_js_get_file_by_id(ctx,token->fileid);
    if (file&&(token->srcp>=0)&&(token->srcp<=file->srcc)) {
      int lineno=eggdev_lineno(file->src,token->srcp);
      fprintf(stderr,"%s:%d: ",file->path,lineno);
    } else {
      fprintf(stderr,"%s: ",ctx->srcpath);
    }
  } else {
    fprintf(stderr,"%s: ",__func__);
  }
  
  // Message and newline.
  if (fmt&&fmt[0]) {
    va_list vargs;
    va_start(vargs,fmt);
    char msg[256];
    int msgc=vsnprintf(msg,sizeof(msg),fmt,vargs);
    if ((msgc>0)&&(msgc<sizeof(msg))) {
      while (msgc&&((unsigned char)msg[msgc-1]<=0x20)) msgc--; // Caller might have accidentally included a newline.
      fprintf(stderr,"%.*s\n",msgc,msg);
    } else {
      fprintf(stderr,"Unspecified error.\n");
    }
  } else {
    fprintf(stderr,"Unspecified error.\n");
  }
  
  // Highlight context if available.
  if (token&&ctx) {
    struct mf_file *file=mf_js_get_file_by_id(ctx,token->fileid);
    if (file&&(token->srcp>=0)&&(token->srcp<=file->srcc)) {
      const int line_limit=100;
      int linep=token->srcp,linec=0;
      while ((linep>0)&&(file->src[linep-1]!=0x0a)) { linep--; linec++; }
      while ((linep+linec<file->srcc)&&(file->src[linep+linec]!=0x0a)) linec++;
      const char *line=file->src+linep;
      while (linec&&((unsigned char)line[0]<=0x20)) { linep++; line++; linec--; }
      while (linec&&((unsigned char)line[linec-1]<=0x20)) linec--;
      if (linec<=line_limit) {
        fprintf(stderr,"  %.*s\n",linec,line);
        int relp=token->srcp-linep;
        if ((relp>=0)&&(relp<line_limit)) {
          char space[128];
          memset(space,' ',relp);
          fprintf(stderr,"  %.*s^\n",relp,space);
        }
      } else {
        //TODO Truncate line fore and aft.
      }
    }
  }
  
  return -2;
}

int mf_jserr_ifctx(struct eggdev_minify_js *ctx,struct mf_token *token,const char *fmt,...) {
  if (!ctx) return -1;
  va_list vargs;
  va_start(vargs,fmt);
  char msg[256];
  int msgc=(fmt?vsnprintf(msg,sizeof(msg),fmt,vargs):0);
  if ((msgc<0)||(msgc>sizeof(msg))) msgc=0;
  return mf_jserr(ctx,token,"%.*s",msgc,msg);
}

void mf_js_log(struct eggdev_minify_js *ctx,struct mf_token *token,const char *fmt,...) {
  char pfx[256],msg[256];
  int pfxc=0,msgc=0;
  if (ctx&&token) {
    struct mf_file *file=mf_js_get_file_by_id(ctx,token->fileid);
    const char *path=file?file->path:"<unknown>";
    int lineno=((token->srcp>=0)&&(token->srcp<=file->srcc))?eggdev_lineno(file->src,token->srcp):0;
    pfxc=snprintf(pfx,sizeof(pfx),"%s:%d: ",path,lineno);
    if ((pfxc<0)||(pfxc>=sizeof(pfx))) pfxc=0;
  }
  if (fmt&&fmt[0]) {
    va_list vargs;
    va_start(vargs,fmt);
    msgc=vsnprintf(msg,sizeof(msg),fmt,vargs);
    if ((msgc<0)||(msgc>=sizeof(msg))) msgc=0;
    while (msgc&&(msg[msgc-1]==0x0a)) msgc--;
  }
  if (pfxc||msgc) fprintf(stderr,"%.*s%.*s\n",pfxc,pfx,msgc,msg);
}

/* Minify Javascript, main entry point.
 */
 
int eggdev_minify_js(struct eggdev_minify_js *ctx,const char *src,int srcc) {
  int err;
  if (ctx->root) return -1;
  struct mf_file *file=mf_js_add_file(ctx,ctx->srcpath,src,srcc);
  if (!file) return -1;
  if (!(ctx->root=mf_node_new())) return -1;
  ctx->root->type=MF_NODE_TYPE_ROOT;
  
  if ((err=mf_js_gather_statements(ctx->root,ctx,file))<0) {
    if (err!=-2) fprintf(stderr,"%s: Unspecified error during initial compile.\n",ctx->srcpath);
    return -2;
  }
  
  if ((err=mf_js_digest(ctx))<0) {
    if (err!=-2) fprintf(stderr,"%s: Unspecified error during intermediate processing.\n",ctx->srcpath);
    return -2;
  }
  
  if ((err=mf_js_output(ctx->dst,ctx,ctx->root))<0) {
    if (err!=-2) fprintf(stderr,"%s: Unspecified error outputting Javascript\n",ctx->srcpath);
    return -2;
  }
  return 0;
}
