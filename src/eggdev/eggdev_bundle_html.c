#include "eggdev_internal.h"
#include "jst.h"

/* Context.
 */
 
struct eggdev_bundle_html {
  const char *rompath;
  const char *dstpath;
  struct sr_encoder dst;
  void *serial;
  int serialc;
  struct eggdev_rom rom;
  const char *title; // metadata "title"
  int titlec;
  const void *icon; // image:(metadata "iconImage"), raw serial, not base64-encoded yet
  int iconc;
  int fbw,fbh; // metadata "fb"
};

static void eggdev_bundle_html_cleanup(struct eggdev_bundle_html *ctx) {
  sr_encoder_cleanup(&ctx->dst);
  if (ctx->serial) free(ctx->serial);
  eggdev_rom_cleanup(&ctx->rom);
}

/* Acquire ROM.
 */
 
static int eggdev_bundle_html_acquire_rom(struct eggdev_bundle_html *ctx) {
  if (!eggdev.dstpath) {
    fprintf(stderr,"%s: Please specify output path as '-oPATH'\n",eggdev.exename);
    return -2;
  }
  ctx->dstpath=eggdev.dstpath;
  if (eggdev.srcpathc!=1) {
    fprintf(stderr,"%s: Expected exactly one input file, for bundling to HTML.\n",eggdev.exename);
    return -2;
  }
  ctx->rompath=eggdev.srcpathv[0];
  if ((ctx->serialc=file_read(&ctx->serial,ctx->rompath))<0) {
    fprintf(stderr,"%s: Failed to read file\n",ctx->rompath);
    return -2;
  }
  int err=eggdev_rom_add_rom_serial(&ctx->rom,ctx->serial,ctx->serialc,ctx->rompath);
  if (err<0) {
    if (err!=-2) fprintf(stderr,"%s: Unspecified error decoding ROM file.\n",ctx->rompath);
    return -2;
  }
  return 0;
}

/* Extract the needed metadata and store in context.
 */
 
static int eggdev_bundle_html_extract_metadata(struct eggdev_bundle_html *ctx) {
  int resp=eggdev_rom_search(&ctx->rom,EGG_TID_metadata,1);
  if (resp>=0) {
    const uint8_t *src=ctx->rom.resv[resp].serial;
    int srcc=ctx->rom.resv[resp].serialc;
    if ((srcc>=4)&&!memcmp(src,"\0EM\xff",4)) {
      int srcp=4;
      while (srcp<srcc) {
        uint8_t kc=src[srcp++];
        if (!kc) break;
        if (srcp>=srcc) break;
        uint8_t vc=src[srcp++];
        if (srcp>srcc-vc-kc) break;
        const char *k=(char*)(src+srcp); srcp+=kc;
        const char *v=(char*)(src+srcp); srcp+=vc;
        
        if ((kc==5)&&!memcmp(k,"title",5)) {
          ctx->title=v;
          ctx->titlec=vc;
        
        } else if ((kc==9)&&!memcmp(k,"iconImage",9)) {
          int rid=0;
          if ((sr_int_eval(&rid,v,vc)>=2)&&(rid>0)&&(rid<=0xffff)) {
            if ((resp=eggdev_rom_search(&ctx->rom,EGG_TID_image,rid))>0) {
              ctx->icon=ctx->rom.resv[resp].serial;
              ctx->iconc=ctx->rom.resv[resp].serialc;
            }
          }
          
        } else if ((kc==2)&&!memcmp(k,"fb",2)) {
          int i=0; for (;i<vc;i++) {
            if (v[i]=='x') {
              if (sr_int_eval(&ctx->fbw,v,i)<2) ctx->fbw=0;
              if (sr_int_eval(&ctx->fbh,v+i+1,vc-i-1)<2) ctx->fbh=0;
              break;
            }
          }
          if ((ctx->fbw<1)||(ctx->fbh<1)) {
            fprintf(stderr,"%s: Invalid value '%.*s' for 'fb' in metadata:1, must be 'WIDTHxHEIGHT'\n",ctx->rompath,vc,v);
            return -2;
          }
        }
      }
    }
  }
  if ((ctx->fbw<1)||(ctx->fbh<1)) {
    ctx->fbw=640;
    ctx->fbh=360;
    fprintf(stderr,"%s:WARNING: metadata:1 did not contain 'fb'. Using the default %dx%d\n",ctx->rompath,ctx->fbw,ctx->fbh);
  }
  return 0;
}

/* Emit HTML plain text with escapes.
 */
 
static int eggdev_bundle_html_emit_text(struct sr_encoder *dst,const char *src,int srcc) {
  int srcp=0;
  while (srcp<srcc) {
    if (src[srcp]=='<') {
      if (sr_encode_raw(dst,"&lt;",4)<0) return -1;
      srcp++;
    } else if (src[srcp]=='&') {
      if (sr_encode_raw(dst,"&amp;",5)<0) return -1;
      srcp++;
    } else {
      const char *raw=src+srcp;
      int rawc=0;
      while ((srcp<srcc)&&(src[srcp]!='<')&&(src[srcp]!='&')) { srcp++; rawc++; }
      if (sr_encode_raw(dst,raw,rawc)<0) return -1;
    }
  }
  return 0;
}

/* Emit base64 text.
 * Optionally split lines at tasteful intervals, in which case we promise to end with a newline.
 */
 
static int eggdev_bundle_html_emit_base64(struct sr_encoder *dst,const uint8_t *src,int srcc,int multiline) {
  int linelen=srcc; // Line length in input bytes; output is (n*4)/3.
  if (multiline) {
    linelen=84; // Use a multiple of 12 to avoid slicing units.
  }
  int srcp=0;
  while (srcp<srcc) {
    int inlen=srcc-srcp;
    if (inlen>linelen) inlen=linelen;
    for (;;) {
      int err=sr_base64_encode((char*)dst->v+dst->c,dst->a-dst->c,src+srcp,inlen);
      if (err<0) return -1;
      if (dst->c<=dst->a-err) {
        dst->c+=err;
        break;
      }
      if (sr_encoder_require(dst,err)<0) return -1;
    }
    srcp+=inlen;
    if (multiline) {
      if (sr_encode_u8(dst,0x0a)<0) return -1;
    }
  }
  return 0;
}

/* Acquire, minify, and emit the platform javascript.
 */
 
static int eggdev_bundle_html_js(struct eggdev_bundle_html *ctx) {
  if (!eggdev_buildcfg.EGG_SDK[0]) {
    fprintf(stderr,"%s was built with EGG_SDK undefined. Can't produce HTML bundles.\n",eggdev.exename);
    return -2;
  }
  char path[1024];
  int pathc=snprintf(path,sizeof(path),"%s/src/www/bootstrap.js",eggdev_buildcfg.EGG_SDK);
  if ((pathc<1)||(pathc>=sizeof(path))) return -1;
  char *src=0;
  int srcc=file_read(&src,path);
  if (srcc<0) {
    fprintf(stderr,"%s: Failed to read file\n",path);
    return -2;
  }
  struct jst_context jst={0};
  int err=jst_minify(&ctx->dst,&jst,src,srcc,path);
  free(src);
  jst_context_cleanup(&jst);
  if (err<0) {
    if (err!=-2) fprintf(stderr,"%s: Unspecified error minifying platform javascript.\n",path);
    return -2;
  }
  return 0;
}

/* Generate the inline CSS.
 * Caller makes the fences.
 */
 
static int eggdev_bundle_html_css(struct eggdev_bundle_html *ctx) {
  if (sr_encode_raw(&ctx->dst,
    "html{background-color:#000;color:#fff}\n"
    "body{margin:0;overflow:hidden;cursor:none;display:flex;align-items:center;justify-content:center}\n"
    "#egg-canvas{width:100vw;height:100vh;object-fit:contain;background-color:#000;image-rendering:pixelated}\n"
  ,-1)<0) return -1;
  return 0;
}

/* Generate inner HTML of the <body> tag.
 * Caller makes the fences.
 */
 
static int eggdev_bundle_html_body(struct eggdev_bundle_html *ctx) {
  if (sr_encode_fmt(&ctx->dst,"<canvas id=\"egg-canvas\" width=\"%d\" height=\"%d\"></canvas>",ctx->fbw,ctx->fbh)<0) return -1;
  return 0;
}

/* Generate HTML in (ctx->dst).
 */
 
static int eggdev_bundle_html_generate_text(struct eggdev_bundle_html *ctx) {

  // Request 500 kB initially; these tend to be very large, and we want to avoid allocation churn.
  if (sr_encoder_require(&ctx->dst,1<<19)<0) return -1;

  if (sr_encode_raw(&ctx->dst,
    "<!DOCTYPE html>\n"
    "<html><head>\n"
  ,-1)<0) return -1;
  
  if (ctx->titlec) {
    if (sr_encode_raw(&ctx->dst,"<title>",-1)<0) return -1;
    if (eggdev_bundle_html_emit_text(&ctx->dst,ctx->title,ctx->titlec)<0) return -1;
    if (sr_encode_raw(&ctx->dst,"</title>\n",-1)<0) return -1;
  }
  
  if (ctx->iconc) {
    const char *mimetype=eggdev_guess_mime_type(0,ctx->icon,ctx->iconc);
    if (sr_encode_fmt(&ctx->dst,"<link rel=\"icon\" href=\"data:%s;base64,",mimetype)<0) return -1;
    if (eggdev_bundle_html_emit_base64(&ctx->dst,ctx->icon,ctx->iconc,0)<0) return -1;
    if (sr_encode_raw(&ctx->dst,"\"/>\n",-1)<0) return -1;
  }
  
  if (sr_encode_raw(&ctx->dst,"<egg-rom style=\"display:none\">\n",-1)<0) return -1;
  if (eggdev_bundle_html_emit_base64(&ctx->dst,ctx->serial,ctx->serialc,1)<0) return -1;
  if (sr_encode_raw(&ctx->dst,"</egg-rom>\n",-1)<0) return -1;
  
  if (sr_encode_raw(&ctx->dst,"<style>\n",-1)<0) return -1;
  if (eggdev_bundle_html_css(ctx)<0) return -1;
  if (sr_encode_raw(&ctx->dst,"</style>\n",-1)<0) return -1;
  
  if (sr_encode_raw(&ctx->dst,"<script>\n",-1)<0) return -1;
  if (eggdev_bundle_html_js(ctx)<0) return -1;
  if (sr_encode_raw(&ctx->dst,"</script>\n",-1)<0) return -1;
  
  if (sr_encode_raw(&ctx->dst,"</head><body>",-1)<0) return -1;
  if (eggdev_bundle_html_body(ctx)<0) return -1;
  if (sr_encode_raw(&ctx->dst,"</body></html>\n",-1)<0) return -1;

  return 0;
}

/* Write output file.
 */
 
static int eggdev_bundle_html_write_output(struct eggdev_bundle_html *ctx) {
  if (file_write(ctx->dstpath,ctx->dst.v,ctx->dst.c)<0) {
    fprintf(stderr,"%s: Failed to write file, %d bytes\n",ctx->dstpath,ctx->dst.c);
    return -2;
  }
  return 0;
}

/* Bundle HTML, main entry point.
 * (eggdev.dstpath) has been examined. (srcpathv) has not.
 */
 
int eggdev_bundle_html() {
  struct eggdev_bundle_html ctx={0};
  int err;
  if ((err=eggdev_bundle_html_acquire_rom(&ctx))<0) goto _done_;
  if ((err=eggdev_bundle_html_extract_metadata(&ctx))<0) goto _done_;
  if ((err=eggdev_bundle_html_generate_text(&ctx))<0) goto _done_;
  if ((err=eggdev_bundle_html_write_output(&ctx))<0) goto _done_;
 _done_:
  eggdev_bundle_html_cleanup(&ctx);
  return err;
}
