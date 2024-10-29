#include "eggdev_internal.h"
#include <signal.h>
#include <unistd.h>

/* Signal handler.
 */
 
static void eggdev_http_rcvsig(int sigid) {
  switch (sigid) {
    case SIGINT: if (++(eggdev.terminate)>=3) {
        fprintf(stderr,"%s: Too many unprocessed signals\n",eggdev.exename);
        exit(1);
      } break;
  }
}

/* Combine and validate paths.
 * Combining only succeeds if it fits with the terminator.
 * If (pfx) at combine contains a match prefix, we take care of it.
 * Validating just confirms that it begins with (pfx) and doesn't contain any double-dot entries.
 * Validate returns (srcc) on success, as a convenience.
 */
 
static int eggdev_serve_combine_path(char *dst,int dsta,const char *pfx,const char *src,int srcc) {
  if (!src||(srcc<0)) return -1;
  int pfxc=0,pfxsepp=-1; while (pfx[pfxc]) {
    if ((pfx[pfxc]==':')&&(pfxsepp<0)) pfxsepp=pfxc;
    pfxc++;
  }
  if (pfxsepp>=0) { // (pfx) has a matching prefix.
    if ((pfxsepp>srcc)||memcmp(pfx,src,pfxsepp)) return -1;
    pfx+=pfxsepp+1;
    pfxc-=pfxsepp+1;
    src+=pfxsepp;
    srcc-=pfxsepp;
  }
  while (pfxc&&(pfx[pfxc-1]=='/')) pfxc--;
  while (srcc&&(src[0]=='/')) { src++; srcc--; }
  int dstc=pfxc+1+srcc;
  if (dstc>=dsta) return -1;
  memcpy(dst,pfx,pfxc);
  dst[pfxc]='/';
  memcpy(dst+pfxc+1,src,srcc);
  dst[dstc]=0;
  return dstc;
}

static int eggdev_serve_validate_path(const char *src,int srcc,const char *pfx) {
  int pfxc=0,pfxsepp=-1; while (pfx[pfxc]) {
    if ((pfx[pfxc]==':')&&(pfxsepp<0)) pfxsepp=pfxc;
    pfxc++;
  }
  if (pfxsepp>=0) {
    pfx+=pfxsepp+1;
    pfxc-=pfxsepp+1;
  }
  if (pfxc>srcc) return -1;
  if (memcmp(src,pfx,pfxc)) return -1;
  if ((pfxc<srcc)&&(src[pfxc]!='/')) return -1;
  int srcp=pfxc;
  while (srcp<srcc) {
    if (src[srcp]=='/') { srcp++; continue; }
    const char *bit=src+srcp;
    int bitc=0;
    while ((srcp<srcc)&&(src[srcp++]!='/')) bitc++;
    if ((bitc==2)&&(bit[0]=='.')&&(bit[1]=='.')) return -1;
  }
  return srcc;
}

/* Resolve path.
 * Never returns zero or >dsta; those are errors.
 * (*ftype) is populated in read cases, since we need it and you probably do too.
 */
 
static int eggdev_serve_resolve_path(char *ftype,char *dst,int dsta,const char *src,int srcc,int writing) {
  if (!src||(srcc<0)) return -1;
  while (srcc&&(src[0]=='/')) { src++; srcc--; }
  while (srcc&&(src[srcc-1]=='/')) srcc--;
  int dstc=0;
  
  /* When writing, prepend eggdev.writepath and validate, and that's it.
   * PUT and DELETE can happen only under this one directory, and files don't have to exist yet.
   */
  if (writing) {
    if (!eggdev.writepath||!eggdev.writepath[0]) return -1;
    int pfxc=0; while (eggdev.writepath[pfxc]) pfxc++;
    if (eggdev.writepath[pfxc-1]=='/') {
      dstc=pfxc+srcc;
    } else {
      dstc=pfxc+1+srcc;
    }
    if (dstc>=dsta) return -1;
    memcpy(dst,eggdev.writepath,pfxc);
    if (eggdev.writepath[pfxc-1]=='/') {
      memcpy(dst+pfxc,src,srcc);
    } else {
      dst[pfxc]='/';
      memcpy(dst+pfxc+1,src,srcc);
    }
    dst[dstc]=0;
    *ftype=0;
    return eggdev_serve_validate_path(dst,dstc,eggdev.writepath);
  }
  
  /* For GET, try all of eggdev.htdocsv in reverse order and return the first file that actually exists.
   */
  int i=eggdev.htdocsc;
  while (i-->0) {
    int dstc=eggdev_serve_combine_path(dst,dsta,eggdev.htdocsv[i],src,srcc);
    if (dstc<1) continue;
    if (eggdev_serve_validate_path(dst,dstc,eggdev.htdocsv[i])<0) continue;
    if (!(*ftype=file_get_type(dst))) continue;
    return dstc;
  }
  
  return -1;
}

/* GET for a directory.
 */
 
static int eggdev_cb_get_directory_cb(const char *path,const char *base,char ftype,void *userdata) {
  if (!ftype) ftype=file_get_type(path);
  if (ftype=='d') {
    int basec=0;
    while (base[basec]) basec++;
    char zbase[256];
    if (basec>sizeof(zbase)) return -1;
    memcpy(zbase,base,basec);
    zbase[basec]='/';
    if (sr_encode_json_string(userdata,0,0,zbase,basec+1)<0) return -1;
  } else {
    if (sr_encode_json_string(userdata,0,0,base,-1)<0) return -1;
  }
  return 0;
}
 
static int eggdev_cb_get_directory(struct http_xfer *req,struct http_xfer *rsp,const char *path) {
  struct sr_encoder *body=http_xfer_get_body(rsp);
  if (sr_encode_json_array_start(body,0,0)<0) return -1;
  if (dir_read(path,eggdev_cb_get_directory_cb,body)<0) return http_xfer_set_status(rsp,500,"Failed to read directory");
  if (sr_encode_json_end(body,0)<0) return -1;
  http_xfer_set_header(rsp,"Content-Type",12,"application/json",16);
  return http_xfer_set_status(rsp,200,"OK");
}

/* For bootstrap.js, replace the string in this line: const DEFAULT_ROM_PATH = "/demo.egg";
 */
 
static void eggdev_find_rom_insertion_point(int *ap,int *bp,const char *src,int srcc) {
  *ap=*bp=-1;
  int srcp=0;
  while (srcp<srcc) {
    const char *token=src+srcp;
    int tokenc=0;
    while ((srcp<srcc)&&((unsigned char)src[srcp++]>0x20)) tokenc++;
    if (tokenc!=16) continue;
    if (memcmp(token,"DEFAULT_ROM_PATH",16)) continue;
    
    while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
    if ((srcp>=srcc)||(src[srcp++]!='=')) break;
    while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
    if ((srcp>=srcc)||(src[srcp++]!='"')) break;
    *ap=srcp;
    while (srcp<srcc) {
      if (src[srcp]=='"') {
        *bp=srcp;
        return;
      }
      srcp++;
    }
  }
}
 
static int eggdev_serve_insert_default_rom_path(struct sr_encoder *dst,const char *src,int srcc,const char *rompath) {
  int ap=-1,bp=-1;
  eggdev_find_rom_insertion_point(&ap,&bp,src,srcc);
  if ((ap<0)||(bp<ap)) return sr_encode_raw(dst,src,srcc);
  if (sr_encode_raw(dst,src,ap)<0) return -1;
  if (sr_encode_raw(dst,rompath,-1)<0) return -1;
  if (sr_encode_raw(dst,src+bp,srcc-bp)<0) return -1;
  return 0;
}

/* GET for a regular file.
 */
 
static int eggdev_cb_get_file(struct http_xfer *req,struct http_xfer *rsp,const char *path) {
  void *serial=0;
  int serialc=file_read(&serial,path);
  if (serialc<0) return http_xfer_set_status(rsp,404,"Not found");
  int err=-1;
  
  // Usually we return files verbatim, but there's an opportunity here to massage the content.
  const char *rpath=0;
  int rpathc=http_xfer_get_path(&rpath,req);
  if (eggdev.default_rom_path&&(rpathc==13)&&!memcmp(rpath,"/bootstrap.js",13)) {
    err=eggdev_serve_insert_default_rom_path(http_xfer_get_body(rsp),serial,serialc,eggdev.default_rom_path);
  } else {
    err=sr_encode_raw(http_xfer_get_body(rsp),serial,serialc);
  }
  if (err<0) {
    free(serial);
    return http_xfer_set_status(rsp,500,"Internal error");
  }
  
  const char *mimetype=eggdev_guess_mime_type(path,serial,serialc);
  free(serial);
  http_xfer_set_header(rsp,"Content-Type",12,mimetype,-1);
  return http_xfer_set_status(rsp,200,"OK");
}

/* GET *
 * Check all (htdocs) backward and return the first one that matches.
 */
 
static int eggdev_cb_get(struct http_xfer *req,struct http_xfer *rsp) {
  const char *rpath=0;
  int rpathc=http_xfer_get_path(&rpath,req);
  if (rpathc<1) return -1;
  if ((rpathc==1)&&(rpath[0]=='/')) { // index.html only gets special treatment right here.
    rpath="/index.html";
    rpathc=11;
  }
  char ftype=0;
  char path[1024];
  int pathc=eggdev_serve_resolve_path(&ftype,path,sizeof(path),rpath,rpathc,0);
  if (pathc<0) return http_xfer_set_status(rsp,404,"Not found");
  if (!ftype) ftype=file_get_type(path);
  if (ftype=='d') return eggdev_cb_get_directory(req,rsp,path);
  if (ftype=='f') return eggdev_cb_get_file(req,rsp,path);
  return http_xfer_set_status(rsp,404,"Not found");
}

/* PUT *, DELETE *
 * Only valid under (writepath) if it exists.
 */
 
static int eggdev_cb_put(struct http_xfer *req,struct http_xfer *rsp) {
  if (!eggdev.writepath||!eggdev.writepath[0]) return http_xfer_set_status(rsp,405,"Launch with --write=PATH for PUT and DELETE support");
  const char *rpath=0;
  int rpathc=http_xfer_get_path(&rpath,req);
  if (rpathc<1) return -1;
  char ftype=0;
  char path[1024];
  int pathc=eggdev_serve_resolve_path(&ftype,path,sizeof(path),rpath,rpathc,1);
  if (pathc<0) return http_xfer_set_status(rsp,404,"Not found");
  const struct sr_encoder *src=http_xfer_get_body(req);
  if ((dir_mkdirp_parent(path)<0)||(file_write(path,src->v,src->c)<0)) {
    return http_xfer_set_status(rsp,500,"Failed to write file");
  }
  return http_xfer_set_status(rsp,200,"Written");
}
 
static int eggdev_cb_delete(struct http_xfer *req,struct http_xfer *rsp) {
  if (!eggdev.writepath||!eggdev.writepath[0]) return http_xfer_set_status(rsp,405,"Launch with --write=PATH for PUT and DELETE support");
  const char *rpath=0;
  int rpathc=http_xfer_get_path(&rpath,req);
  if (rpathc<1) return -1;
  char ftype=0;
  char path[1024];
  int pathc=eggdev_serve_resolve_path(&ftype,path,sizeof(path),rpath,rpathc,1);
  if (pathc<0) return http_xfer_set_status(rsp,404,"Not found");
  if (unlink(path)<0) {
    return http_xfer_set_status(rsp,500,"Failed to delete file");
  }
  return http_xfer_set_status(rsp,200,"Deleted");
}

/* GET /api/make/*
 */
 
static int eggdev_cb_get_api_make(struct http_xfer *req,struct http_xfer *rsp) {
  const char *rpath=0;
  int rpathc=http_xfer_get_path(&rpath,req);
  if ((rpathc<9)||memcmp(rpath,"/api/make",9)) return -1;
  rpath+=9;
  rpathc-=9;
  if (!rpathc||((rpathc==1)&&(rpath[0]=='/'))) {
    rpath="/index.html";
    rpathc=11;
  }
  char ftype=0;
  char path[1024];
  int pathc=eggdev_serve_resolve_path(&ftype,path,sizeof(path),rpath,rpathc,0);
  if (pathc<0) return http_xfer_set_status(rsp,404,"Not found");
  if (!ftype) ftype=file_get_type(path);
  if ((ftype!='d')&&(ftype!='f')) return http_xfer_set_status(rsp,404,"Not found");
  
  /* Now the fancy part: Run `make` synchronously, and if it fails, return its output instead.
   */
  FILE *pipe=popen("make 2>&1","r");
  if (!pipe) return http_xfer_set_status(rsp,500,"Failed to invoke 'make'");
  struct sr_encoder *log=http_xfer_get_body(rsp);
  int logc0=log->c;
  for (;;) {
    int err;
    if (
      (sr_encoder_require(log,1024)<0)||
      ((err=fread((char*)log->v+log->c,1,log->a-log->c,pipe))<0)
    ) {
      pclose(pipe);
      return http_xfer_set_status(rsp,500,"Internal error");
    }
    if (!err) break;
    log->c+=err;
  }
  int status=pclose(pipe);
  if (!WIFEXITED(status)) status=1;
  else status=WEXITSTATUS(status);
  if (status) {
    http_xfer_set_header(rsp,"Content-Type",12,"text/plain",10);
    return http_xfer_set_status(rsp,599,"make failed, status %d",status);
  }
  log->c=logc0;
  
  // `make` succeeded, so we finish just like regular GET.
  if (ftype=='d') return eggdev_cb_get_directory(req,rsp,path);
  return eggdev_cb_get_file(req,rsp,path);
}

/* GET /api/resources/*
 */
 
struct eggdev_serve_resources_ctx {
  struct sr_encoder *dst;
  const char *root;
  int rootc;
};
 
static int eggdev_serve_resources_file(struct sr_encoder *dst,const char *path,const char *root,int rootc) {
  const char *rptpath=path;
  if (!memcmp(rptpath,root,rootc)) rptpath+=rootc;
  while (*rptpath=='/') rptpath++;
  void *serial=0;
  int serialc=file_read(&serial,path);
  if (serialc<0) return -1;
  int jsonctx=sr_encode_json_object_start(dst,0,0);
  sr_encode_json_string(dst,"path",4,rptpath,-1);
  sr_encode_json_base64(dst,"serial",6,serial,serialc);
  free(serial);
  if (sr_encode_json_end(dst,jsonctx)<0) return -1;
  return 0;
}

static int eggdev_serve_resources_dircb(const char *path,const char *base,char ftype,void *userdata) {
  struct eggdev_serve_resources_ctx *ctx=userdata;
  if (!ftype) ftype=file_get_type(path);
  if (ftype=='f') return eggdev_serve_resources_file(ctx->dst,path,ctx->root,ctx->rootc);
  if (ftype=='d') return dir_read(path,eggdev_serve_resources_dircb,ctx);
  return 0;
}
 
static int eggdev_cb_get_api_resources(struct http_xfer *req,struct http_xfer *rsp) {
  const char *rpath=0;
  int rpathc=http_xfer_get_path(&rpath,req);
  if ((rpathc<14)||memcmp(rpath,"/api/resources",14)) return -1;
  rpath+=14;
  rpathc-=14;
  char ftype=0;
  char path[1024];
  int pathc=eggdev_serve_resolve_path(&ftype,path,sizeof(path),rpath,rpathc,0);
  if (pathc<0) return http_xfer_set_status(rsp,404,"Not found");
  if (!ftype) ftype=file_get_type(path);
  if (ftype!='d') return http_xfer_set_status(rsp,404,"Not found");
  
  struct sr_encoder *dst=http_xfer_get_body(rsp);
  struct eggdev_serve_resources_ctx ctx={
    .dst=dst,
    .root=path,
    .rootc=pathc,
  };
  if (sr_encode_json_array_start(dst,0,0)<0) return -1;
  if (dir_read(path,eggdev_serve_resources_dircb,&ctx)<0) return -1;
  if (sr_encode_json_end(dst,0)<0) return -1;
  http_xfer_set_header(rsp,"Content-Type",12,"application/json",16);
  return http_xfer_set_status(rsp,200,"OK");
}

/* POST /api/sound
 */
 
static int eggdev_cb_post_api_sound(struct http_xfer *req,struct http_xfer *rsp) {

  /* Not configured, must return 501.
   * Then empty body, end song and return 200.
   * These are for testing availability of audio.
   */
  if (!eggdev.hostio||!eggdev.hostio->audio||!eggdev.synth) return http_xfer_set_status(rsp,501,"Audio not available");
  struct sr_encoder *body=http_xfer_get_body(req);
  if (!body->c) {
    if (hostio_audio_lock(eggdev.hostio)>=0) {
      synth_play_song_borrow(eggdev.synth,0,0,0);
      hostio_audio_unlock(eggdev.hostio);
    }
    return http_xfer_set_status(rsp,200,"OK");
  }
  
  /* Eval query params.
   */
  int positionms=0,repeat=0;
  http_xfer_get_param_int(&positionms,req,"position",8);
  http_xfer_get_param_int(&repeat,req,"repeat",6);
  
  /* Pass it through the resource compiler, to get a format amenable to synth.
   */
  struct eggdev_res res={0};
  if (eggdev_res_set_serial(&res,body->v,body->c)<0) return -1;
  int err=eggdev_compile_sound(&res,0);
  if (err<0) {
    eggdev_res_cleanup(&res);
    return http_xfer_set_status(rsp,500,"Compilation failed");
  }
  
  /* Hand off to the synthesizer.
   */
  if (hostio_audio_lock(eggdev.hostio)<0) {
    eggdev_res_cleanup(&res);
    return http_xfer_set_status(rsp,500,"Failed to lock audio");
  }
  synth_play_song_handoff(eggdev.synth,res.serial,res.serialc,repeat);
  res.serial=0; // HANDOFF
  if (positionms) synth_set_playhead(eggdev.synth,(double)positionms/1000.0);
  hostio_audio_unlock(eggdev.hostio);
  
  eggdev_res_cleanup(&res);
  return http_xfer_set_status(rsp,200,"OK");
}

/* POST /api/compile
 */
 
static int eggdev_cb_post_api_compile(struct http_xfer *req,struct http_xfer *rsp) {
  char dstfmt[32],srcfmt[32];
  int dstfmtc=http_xfer_get_param(dstfmt,sizeof(dstfmt),req,"dstfmt",6);
  int srcfmtc=http_xfer_get_param(srcfmt,sizeof(srcfmt),req,"srcfmt",6);
  if ((dstfmtc<0)||(dstfmtc>sizeof(dstfmt))) dstfmtc=0;
  if ((srcfmtc<0)||(srcfmtc>sizeof(srcfmt))) srcfmtc=0;
  struct sr_encoder *src=http_xfer_get_body(req);
  struct sr_encoder *dst=http_xfer_get_body(rsp);
  if (!src||!dst) return -1;
  int dstc0=dst->c;
  int err=eggdev_cvta2a(dst,src->v,src->c,"(http)",dstfmt,dstfmtc,srcfmt,srcfmtc);
  if (err<0) {
    dst->c=dstc0;
    return http_xfer_set_status(rsp,400,"Failed to convert");
  }
  return http_xfer_set_status(rsp,200,"OK");
}

/* Serve HTTP request.
 * Note that errors returned here do not fall through http_update; http handles and eats them.
 */
 
static int eggdev_cb_serve(struct http_xfer *req,struct http_xfer *rsp,void *userdata) {
  return http_dispatch(req,rsp,
    HTTP_METHOD_GET,"/api/make/**",eggdev_cb_get_api_make,
    HTTP_METHOD_GET,"/api/make",eggdev_cb_get_api_make,
    HTTP_METHOD_GET,"/api/resources/**",eggdev_cb_get_api_resources,
    HTTP_METHOD_POST,"/api/sound",eggdev_cb_post_api_sound,
    HTTP_METHOD_POST,"/api/compile",eggdev_cb_post_api_compile,
    HTTP_METHOD_PUT,"",eggdev_cb_put,
    HTTP_METHOD_DELETE,"",eggdev_cb_delete,
    HTTP_METHOD_GET,"",eggdev_cb_get
  );
}

/* WebSocket connection.
 * TODO Are we going to use WebSocket?
 */
 
static int eggdev_cb_connect(struct http_websocket *ws,void *userdata) {
  fprintf(stderr,"%s\n",__func__);
  return 0;
}

static int eggdev_cb_disconnect(struct http_websocket *ws,void *userdata) {
  fprintf(stderr,"%s\n",__func__);
  return 0;
}

/* Init HTTP context.
 */
 
static int eggdev_serve_init_http() {
  struct http_context_delegate delegate={
    .cb_serve=eggdev_cb_serve,
    .cb_connect=eggdev_cb_connect,
    .cb_disconnect=eggdev_cb_disconnect,
  };
  if (!(eggdev.http=http_context_new(&delegate))) {
    fprintf(stderr,"%s: Failed to create HTTP context.\n",eggdev.exename);
    return -2;
  }
  if (http_listen(eggdev.http,eggdev.external?0:1,eggdev.port)<0) {
    fprintf(stderr,"%s: Failed to open %s HTTP server on port %d\n",eggdev.exename,eggdev.external?"external":"local",eggdev.port);
    return -2;
  }
  fprintf(stderr,"%s: Serving HTTP on port %d %s\n",eggdev.exename,eggdev.port,eggdev.external?"*** ON ALL INTERFACES ***":"");
  return 0;
}

/* Pump synthesizer.
 */
 
static void eggdev_serve_cb_pcm_out(int16_t *v,int c,struct hostio_audio *driver) {
  synth_updatei(v,c,eggdev.synth);
}

/* Init audio driver and synthesizer, if configured.
 */
 
static int eggdev_serve_init_audio() {
  if (!eggdev.audio_drivers) return 0;
  if (!strcmp(eggdev.audio_drivers,"none")) return 0;
  struct hostio_audio_delegate delegate={
    .cb_pcm_out=eggdev_serve_cb_pcm_out,
  };
  struct hostio_audio_setup setup={
    .rate=eggdev.audio_rate,
    .chanc=eggdev.audio_chanc,
    .device=eggdev.audio_device,
    .buffer_size=eggdev.audio_buffer,
  };
  if (!(eggdev.hostio=hostio_new(0,&delegate,0))) return -1;
  if ((hostio_init_audio(eggdev.hostio,eggdev.audio_drivers,&setup)<0)||!eggdev.hostio->audio) {
    fprintf(stderr,"%s: Failed to initialize audio driver.\n",eggdev.exename);
    return -2;
  }
  struct hostio_audio *driver=eggdev.hostio->audio;
  if (!(eggdev.synth=synth_new(driver->rate,driver->chanc))) {
    fprintf(stderr,"%s: Failed to initialize synthesizer. rate=%d chanc=%d\n",eggdev.exename,driver->rate,driver->chanc);
    return -2;
  }
  fprintf(stderr,"%s: Native audio via '%s', rate=%d, chanc=%d\n",eggdev.exename,driver->type->name,driver->rate,driver->chanc);
  hostio_audio_play(eggdev.hostio,1);
  return 0;
}

/* Cleanup.
 */
 
static void eggdev_serve_cleanup() {
  if (eggdev.hostio) {
    hostio_audio_play(eggdev.hostio,0);
    hostio_del(eggdev.hostio);
    eggdev.hostio=0;
  }
  if (eggdev.synth) {
    synth_del(eggdev.synth);
    eggdev.synth=0;
  }
  if (eggdev.http) {
    http_context_del(eggdev.http);
    eggdev.http=0;
  }
}

/* serve, main entry point.
 */
 
int eggdev_main_serve() {
  int err=0;
  if (!eggdev.port) eggdev.port=8080;
  if (eggdev.http) return -1;
  signal(SIGINT,eggdev_http_rcvsig);
  if ((err=eggdev_serve_init_http())<0) goto _done_;
  if ((err=eggdev_serve_init_audio())<0) goto _done_;
  while (!eggdev.terminate) {
    if (eggdev.hostio) {
      if ((err=http_update(eggdev.http,50))<0) goto _done_;
      if ((err=hostio_update(eggdev.hostio))<0) goto _done_;
    } else {
      if ((err=http_update(eggdev.http,500))<0) goto _done_;
    }
  }
 _done_:;
  eggdev_serve_cleanup();
  if (err>=0) fprintf(stderr,"%s: Normal exit\n",eggdev.exename);
  return err;
}
