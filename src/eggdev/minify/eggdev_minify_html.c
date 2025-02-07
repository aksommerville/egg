#include "mf_internal.h"
#include "eggdev/eggdev_html.h"

/* Cleanup.
 */
 
void eggdev_minify_html_cleanup(struct eggdev_minify_html *ctx) {
}

/* Examine and emit <link> tag.
 */
 
struct mf_html_link {
  const char *rel,*href;
  int relc,hrefc;
};

static int mf_html_link_cb(const char *k,int kc,const char *v,int vc,void *userdata) {
  struct mf_html_link *link=userdata;
  if ((kc==3)&&!memcmp(k,"rel",3)) {
    link->rel=v;
    link->relc=vc;
  } else if ((kc==4)&&!memcmp(k,"href",4)) {
    link->href=v;
    link->hrefc=vc;
  }
  return 0;
}
 
static int mf_html_link_singleton(struct eggdev_minify_html *ctx,const char *expr,int exprc) {
  struct mf_html_link link={0};
  if (eggdev_html_for_each_attribute(expr,exprc,mf_html_link_cb,&link)<0) return -1;
  
  if ((link.relc==10)&&!memcmp(link.rel,"stylesheet",10)) {
    char path[1024];
    int pathc=eggdev_relative_path(path,sizeof(path),ctx->srcpath,-1,link.href,link.hrefc);
    if ((pathc<1)||(pathc>=sizeof(path))) return -1;
    if (sr_encode_raw(ctx->dst,"<style>",-1)<0) return -1;
    char *src=0;
    int srcc=file_read(&src,path);
    if (srcc<0) {
      fprintf(stderr,"%s: Failed to read file for CSS <link> at %s:%d\n",path,ctx->srcpath,eggdev_lineno(ctx->src,expr-ctx->src));
      return -2;
    }
    int err=eggdev_minify_inner(ctx->dst,src,srcc,path,EGGDEV_FMT_CSS);
    free(src);
    if (err<0) {
      fprintf(stderr,"%s:%d: Error minifying external CSS\n",ctx->srcpath,eggdev_lineno(ctx->src,expr-ctx->src));
      return -2;
    }
    return sr_encode_raw(ctx->dst,"</style>",-1);
    
  } else if ((link.relc==4)&&!memcmp(link.rel,"icon",4)&&((link.hrefc<5)||memcmp(link.href,"data:",5))) {
    char path[1024];
    int pathc=eggdev_relative_path(path,sizeof(path),ctx->srcpath,-1,link.href,link.hrefc);
    if ((pathc<1)||(pathc>=sizeof(path))) return -1;
    void *src=0;
    int srcc=file_read(&src,path);
    if (srcc<0) {
      fprintf(stderr,"%s: Failed to read file for favicon at %s:%d\n",path,ctx->srcpath,eggdev_lineno(ctx->src,expr-ctx->src));
      return -2;
    }
    const char *mime=eggdev_guess_mime_type(path,src,srcc);
    if (sr_encode_fmt(ctx->dst,"<link rel=\"icon\" href=\"data:%s;base64,",mime)<0) {
      free(src);
      return -1;
    }
    int err=sr_encode_base64(ctx->dst,src,srcc);
    free(src);
    if (err<0) return -1;
    if (sr_encode_raw(ctx->dst,"\"/>",-1)<0) return -1;
    return 0;
    
  } else {
    return sr_encode_raw(ctx->dst,expr,exprc);
  }
}

/* Received <script>. If it's external, include that, otherwise use the body. Minify and emit.
 */
 
static int mf_html_script(struct eggdev_minify_html *ctx,const char *body,int bodyc,const char *expr,int exprc) {
  const char *relurl;
  int relurlc=eggdev_html_get_attribute(&relurl,expr,exprc,"src",3);
  if (relurlc>0) {
    char path[1024];
    int pathc=eggdev_relative_path(path,sizeof(path),ctx->srcpath,-1,relurl,relurlc);
    if ((pathc<1)||(pathc>=sizeof(path))) return -1;
    if (sr_encode_raw(ctx->dst,"<script>",-1)<0) return -1;
    char *src=0;
    int srcc=file_read(&src,path);
    if (srcc<0) {
      fprintf(stderr,"%s: Failed to read file for <script> tag at %s:%d\n",path,ctx->srcpath,eggdev_lineno(ctx->src,expr-ctx->src));
      return -2;
    }
    if (eggdev_minify_inner(ctx->dst,src,srcc,path,EGGDEV_FMT_JS)<0) {
      free(src);
      fprintf(stderr,"%s:%d: Error minifying external Javascript\n",ctx->srcpath,eggdev_lineno(ctx->src,expr-ctx->src));
      return -2;
    }
    free(src);
    if (sr_encode_raw(ctx->dst,"</script>",-1)<0) return -1;
    return 0;
    
  } else {
    if (sr_encode_raw(ctx->dst,"<script>",-1)<0) return -1;
    int err=eggdev_minify_inner(ctx->dst,body,bodyc,ctx->srcpath,EGGDEV_FMT_JS);
    if (err<0) {
      fprintf(stderr,"%s:%d: Error minifying <script>\n",ctx->srcpath,eggdev_lineno(ctx->src,expr-ctx->src));
      return -2;
    }
    if (sr_encode_raw(ctx->dst,"</script>",-1)<0) return -1;
    return 0;
  }
}

/* Received inline <style>. Minify and emit.
 */
 
static int mf_html_style_inline(struct eggdev_minify_html *ctx,const char *body,int bodyc) {
  if (sr_encode_raw(ctx->dst,"<style>",-1)<0) return -1;
  if (eggdev_minify_inner(ctx->dst,body,bodyc,ctx->srcpath,EGGDEV_FMT_CSS)<0) {
    fprintf(stderr,"%s:%d: Error minifying <style>\n",ctx->srcpath,eggdev_lineno(ctx->src,body-ctx->src));
    return -2;
  }
  if (sr_encode_raw(ctx->dst,"</style>",-1)<0) return -1;
  return 0;
}

/* Minify HTML, main entry point.
 */
 
int eggdev_minify_html(struct eggdev_minify_html *ctx) {
  struct eggdev_html_reader reader;
  eggdev_html_reader_init(&reader,ctx->src,ctx->srcc,ctx->srcpath);
  const char *expr;
  int exprc,err;
  while ((exprc=eggdev_html_reader_next(&expr,&reader))>0) {
    switch (eggdev_html_expression_type(expr,exprc)) {
    
      // Singletons might inline a file but usually emit verbatim.
      case EGGDEV_HTML_EXPR_SINGLETON: {
          const char *name;
          int namec=eggdev_html_get_tag_name(&name,expr,exprc);
          if ((namec==4)&&!memcmp(name,"link",4)) {
            if ((err=mf_html_link_singleton(ctx,expr,exprc))<0) return err;
          } else {
            if (sr_encode_raw(ctx->dst,expr,exprc)<0) return -1;
          }
        } break;
      
      // Open tags for <script> and <style>, minify the interior text.
      // Others, emit and proceed.
      case EGGDEV_HTML_EXPR_OPEN: {
          const char *name;
          int namec=eggdev_html_get_tag_name(&name,expr,exprc);
          if ((namec==6)&&!memcmp(name,"script",6)) {
            const char *body;
            int bodyc=eggdev_html_close(&body,&reader);
            if (bodyc<0) return bodyc;
            if ((err=mf_html_script(ctx,body,bodyc,expr,exprc))<0) return err;
          } else if ((namec==5)&&!memcmp(name,"style",5)) {
            const char *body;
            int bodyc=eggdev_html_close(&body,&reader);
            if (bodyc<0) return bodyc;
            if ((err=mf_html_style_inline(ctx,body,bodyc))<0) return err;
          } else {
            if (sr_encode_raw(ctx->dst,expr,exprc)<0) return -1;
          }
        } break;
        
      // TEXT and CLOSE, just trim the leading and trailing space.
      case EGGDEV_HTML_EXPR_CLOSE:
      case EGGDEV_HTML_EXPR_TEXT: {
          while (exprc&&((unsigned char)expr[exprc-1]<=0x20)) exprc--;
          while (exprc&&((unsigned char)expr[0]<=0x20)) { exprc--; expr++; }
          if (sr_encode_raw(ctx->dst,expr,exprc)<0) return -1;
        } break;
      // Ignore SPACE, COMMENT, and anything unknown.
    }
  }
  return 0;
}
