#include "eggdev_internal.h"
#include <unistd.h>
#include <fcntl.h>
#include <time.h>

/* Context.
 */
 
struct eggdev_project_context {
  char *projname;
  char *title;
  char *author;
  struct sr_encoder scratch;
};

static void eggdev_project_context_cleanup(struct eggdev_project_context *ctx) {
  if (ctx->projname) free(ctx->projname);
  if (ctx->title) free(ctx->title);
  if (ctx->author) free(ctx->author);
  sr_encoder_cleanup(&ctx->scratch);
}

/* Prompt on stdout and read response on stdin.
 * Empty response is not an error, and we return a new empty string.
 * (prompt) should end with a space.
 */
 
static char *eggdev_project_prompt(const char *prompt) {
  int promptc=0;
  if (prompt) { while (prompt[promptc]) promptc++; }
  if (promptc) {
    if (write(STDOUT_FILENO,prompt,promptc)!=promptc) return 0;
  }
  int dsta=256;
  char *dst=malloc(dsta);
  if (!dst) return 0;
  int dstc=read(STDIN_FILENO,dst,dsta);
  if ((dstc<0)||(dstc>=dsta)) { free(dst); return 0; }
  if (dstc&&(dst[dstc-1]==0x0a)) dstc--;
  dst[dstc]=0;
  return dst;
}

/* Validate project name, lexically only.
 * Letters, digits, underscore, and dash.
 * There's no technical reason to forbid spaces and other punctuation (except slash),
 * but I'm not going to encourage that kind of bad behavior.
 */
 
static int eggdev_project_name_valid(const char *src) {
  if (!src||!src[0]) return 0;
  for (;*src;src++) {
    if ((*src>='a')&&(*src<='z')) continue;
    if ((*src>='A')&&(*src<='Z')) continue;
    if ((*src>='0')&&(*src<='9')) continue;
    if (*src=='_') continue;
    if (*src=='-') continue;
    return 0;
  }
  return 1;
}

/* Generate .gitignore.
 * Overwrites (ctx->scratch).
 */
 
static int eggdev_project_generate_gitignore(struct eggdev_project_context *ctx) {
  ctx->scratch.c=0;
  if (sr_encode_raw(&ctx->scratch,
    "*~\n"
    "~*\n"
    "/mid\n"
    "/out\n"
    ".DS_Store\n"
  ,-1)<0) return -1;
  return 0;
}

/* Generate Makefile.
 * Overwrites (ctx->scratch).
 */
 
static int eggdev_project_generate_makefile(struct eggdev_project_context *ctx) {
  ctx->scratch.c=0;
  
  /* Makefile is by far the largest and most complex thing we generate.
   * So we're going to cheat a little: The Egg SDK has an example Makefile at etc/tool/game.mk.
   * We can copy that, just changing a few instances of "game" into (ctx->projname).
   */
  char path[1024];
  int pathc=snprintf(path,sizeof(path),"%s/etc/tool/game.mk",eggdev_buildcfg.EGG_SDK);
  if ((pathc<1)||(pathc>=sizeof(path))) return -1;
  char *src=0;
  int srcc=file_read(&src,path);
  if (srcc<0) {
    fprintf(stderr,"%s: Failed to read Makefile template.\n",path);
    return -2;
  }
  
  struct sr_decoder decoder={.v=src,.c=srcc};
  const char *line;
  int linec,lineno=1,err;
  for (;(linec=sr_decode_line(&line,&decoder))>0;lineno++) {
    if ((linec>=5)&&!memcmp(line,"ROM:=",5)) {
      if (sr_encode_fmt(&ctx->scratch,"ROM:=out/%s.egg\n",ctx->projname)<0) { free(src); return -1; }
    } else if ((linec>=6)&&!memcmp(line,"HTML:=",6)) {
      if (sr_encode_fmt(&ctx->scratch,"HTML:=out/%s.html\n",ctx->projname)<0) { free(src); return -1; }
    } else if ((linec>=14)&&!memcmp(line,"  NATIVE_EXE:=",14)) {
      if (sr_encode_fmt(&ctx->scratch,"  NATIVE_EXE:=out/%s.$(NATIVE_TARGET)\n",ctx->projname)<0) { free(src); return -1; }
    } else {
      if (sr_encode_raw(&ctx->scratch,line,linec)<0) { free(src); return -1; }
    }
  }
  
  free(src);
  return 0;
}

/* Generate src/data/metadata.
 * Overwrites (ctx->scratch).
 * May set other fields in ctx eg author, title.
 */
 
static int eggdev_project_generate_metadata(struct eggdev_project_context *ctx) {
  ctx->scratch.c=0;
  
  char *fbsize=eggdev_project_prompt("Framebuffer size 'WIDTHxHEIGHT': ");
  if (!fbsize) return -1;
  int fbw=0,fbh=0,fbsizep=0,sep=0;
  for (;fbsize[fbsizep];fbsizep++) {
    if (fbsize[fbsizep]=='x') {
      if (sep) { fbw=-1; break; }
      sep=1;
    } else if ((fbsize[fbsizep]<'0')||(fbsize[fbsizep]>'9')) {
      fprintf(stderr,"%s: Unexpected character '%c' in framebuffer size.\n",eggdev.exename,fbsize[fbsizep]);
      free(fbsize);
      return -2;
    } else if (sep) {
      fbh*=10;
      fbh+=fbsize[fbsizep]-'0';
    } else {
      fbw*=10;
      fbw+=fbsize[fbsizep]-'0';
    }
  }
  free(fbsize);
  if ((fbw<1)||(fbw>4096)||(fbh<1)||(fbh>4096)) {
    fprintf(stderr,"%s: Invalid framebuffer size. Expected 'WIDTHxHEIGHT' in 1..4096.\n",eggdev.exename);
    return -2;
  }
  if (sr_encode_fmt(&ctx->scratch,"fb=%dx%d\n",fbw,fbh)<0) return -1;
  
  if (!(ctx->title=eggdev_project_prompt("Proper title: "))) return -1;
  if (ctx->title[0]) {
    if (sr_encode_fmt(&ctx->scratch,"title=%s\n",ctx->title)<0) return -1;
    if (sr_encode_fmt(&ctx->scratch,"title$=2\n")<0) return -1;
  }
  
  if (!(ctx->author=eggdev_project_prompt("Your name (optional): "))) return -1;
  if (ctx->author[0]) {
    if (sr_encode_fmt(&ctx->scratch,"author=%s\n",ctx->author)<0) return -1;
  }
  
  time_t now=time(0);
  struct tm *tm=localtime(&now);
  if (tm) {
    if (sr_encode_fmt(&ctx->scratch,"time=%04d-%02d-%02d\n",1900+tm->tm_year,1+tm->tm_mon,tm->tm_mday)<0) return -1;
  }
  
  if (sr_encode_raw(&ctx->scratch,"version=0.0.1\n",-1)<0) return -1;
  if (sr_encode_raw(&ctx->scratch,"lang=en\n",-1)<0) return -1;
  if (sr_encode_raw(&ctx->scratch,"players=1\n",-1)<0) return -1;
  if (sr_encode_raw(&ctx->scratch,"# desc= Short description.\n",-1)<0) return -1;
  if (sr_encode_raw(&ctx->scratch,"# freedom= restricted | limited | intact | free\n",-1)<0) return -1;
  if (sr_encode_raw(&ctx->scratch,"# copyright=(c) 2024 Your Name Here\n",-1)<0) return -1;
  if (sr_encode_raw(&ctx->scratch,"# advisory= Mention if there's gore, profanity, nudity, etc.\n",-1)<0) return -1;
  if (sr_encode_raw(&ctx->scratch,"# genre= Use itch.io for reference.\n",-1)<0) return -1;
  if (sr_encode_raw(&ctx->scratch,"# tags= Use itch.io for reference.\n",-1)<0) return -1;
  if (sr_encode_raw(&ctx->scratch,"# persistKey= Nonce. See EGG/etc/doc/metadata-format.md\n",-1)<0) return -1;
  if (sr_encode_raw(&ctx->scratch,"# iconImage= image id, should be 16x16\n",-1)<0) return -1;
  if (sr_encode_raw(&ctx->scratch,"# posterImage= image id, should have aspect 2:1 and no text\n",-1)<0) return -1;
  if (sr_encode_raw(&ctx->scratch,"# contact= Email, phone, URL. So users can yell at you.\n",-1)<0) return -1;
  
  return 0;
}

/* Generate src/data/strings/en-1.
 * Overwrites (ctx->scratch).
 */
 
static int eggdev_project_generate_strings(struct eggdev_project_context *ctx) {
  ctx->scratch.c=0;
  
  // By convention, 1:1 in every language should be that language's own name, in itself.
  if (sr_encode_raw(&ctx->scratch,"1 English\n",-1)<0) return -1;
  
  // If a title is present, our metadata will have referenced it as 1:2.
  // We'll emit exactly the same thing in the metadata title field.
  if (ctx->title&&ctx->title[0]) {
    if (sr_encode_fmt(&ctx->scratch,"2 %s\n",ctx->title)<0) return -1;
  }
  
  return 0;
}

/* Generate README.md.
 * Overwrites (ctx->scratch).
 */
 
static int eggdev_project_generate_readme(struct eggdev_project_context *ctx) {
  ctx->scratch.c=0;
  if (sr_encode_fmt(&ctx->scratch,"# %s\n\n",(ctx->title&&ctx->title[0])?ctx->title:ctx->projname)<0) return -1;
  if (sr_encode_raw(&ctx->scratch,"Requires [Egg](https://github.com/aksommerville/egg) to build.\n",-1)<0) return -1;
  return 0;
}

/* Generate src/game/main.c.
 * Overwrites (ctx->scratch).
 */
 
static int eggdev_project_generate_main(struct eggdev_project_context *ctx) {
  ctx->scratch.c=0;
  
  /* Same idea as Makefile, we're going to copy a prewritten template from Egg SDK.
   * But we're an edge case: We actually don't need to change anything, and can just copy it verbatim.
   */
  char path[1024];
  int pathc=snprintf(path,sizeof(path),"%s/etc/tool/game.c",eggdev_buildcfg.EGG_SDK);
  if ((pathc<1)||(pathc>=sizeof(path))) return -1;
  char *src=0;
  int srcc=file_read(&src,path);
  if (srcc<0) {
    fprintf(stderr,"%s: Failed to read game template.\n",path);
    return -2;
  }
  // Bodysnatch into (ctx->scratch).
  if (ctx->scratch.v) free(ctx->scratch.v);
  ctx->scratch.v=src;
  ctx->scratch.c=srcc;
  ctx->scratch.a=srcc;
  return 0;
}

/* Generate src/game/shared_symbols.h
 * Overwrites (ctx->scratch).
 */
 
static int eggdev_project_generate_shared_symbols(struct eggdev_project_context *ctx) {
  ctx->scratch.c=0;
  if (sr_encode_raw(&ctx->scratch,
    "/* shared_symbols.h\n"
    " * Consumed by both the game and the tools.\n"
    " */\n"
    "\n"
    "#ifndef SHARED_SYMBOLS_H\n"
    "#define SHARED_SYMBOLS_H\n"
    "\n"
    "#define NS_sys_tilesize 16\n"
    "#define NS_sys_mapw 20\n"
    "#define NS_sys_maph 15\n"
    "\n"
    "#define CMD_map_image     0x20 /* u16:imageid */\n"
    "#define CMD_map_neighbors 0x60 /* u16:left u16:right u16:up u16:down */\n"
    "#define CMD_map_sprite    0x61 /* u16:pos u16:spriteid u32:reserved */\n"
    "#define CMD_map_door      0x62 /* u16:pos u16:mapid u16:dstpos u16:reserved */\n"
    "\n"
    "#define CMD_sprite_image  0x20 /* u16:imageid */\n"
    "#define CMD_sprite_tile   0x21 /* u8:tileid u8:xform */\n"
    "\n"
    "#define NS_tilesheet_physics     1\n"
    "#define NS_tilesheet_neighbors   0\n"
    "#define NS_tilesheet_family      0\n"
    "#define NS_tilesheet_weight      0\n"
    "\n"
    "#endif\n"
  ,-1)<0) return -1;
  return 0;
}

/* Copy a few static resources from the Egg demo project.
 * Fonts, maybe other stuff?
 * This does not populate ctx->scratch; everything gets done immediately.
 */
 
static int eggdev_project_copy_res(struct eggdev_project_context *ctx,const char *srcsfx,const char *dstpath) {
  char srcpath[1024];
  int srcpathc=snprintf(srcpath,sizeof(srcpath),"%s/%s",eggdev_buildcfg.EGG_SDK,srcsfx);
  if ((srcpathc<1)||(srcpathc>=sizeof(srcpath))) return -1;
  void *src=0;
  int srcc=file_read(&src,srcpath);
  if (srcc<0) {
    fprintf(stderr,"%s: Failed to read file.\n",srcpath);
    return -2;
  }
  int err=file_write(dstpath,src,srcc);
  free(src);
  if (err<0) {
    fprintf(stderr,"%s: Failed to write file, %d bytes.\n",dstpath,srcc);
    return -2;
  }
  return 0;
}
 
static int eggdev_project_copy_resources(struct eggdev_project_context *ctx) {
  int err;
  // There are more fonts we could copy if we felt like it, and more pages for font9. Usually I end up deleting them, so just sticking to the basics here.
  if ((err=eggdev_project_copy_res(ctx,"src/demo/data/image/2-font9_0020.a1.rlead.png","src/data/image/1-font9_0020.a1.rlead.png"))<0) return err;
  //if ((err=eggdev_project_copy_res(ctx,"src/demo/data/image/3-font9_00a1.a1.rlead.png","src/data/image/2-font9_00a1.a1.rlead.png"))<0) return err;
  //if ((err=eggdev_project_copy_res(ctx,"src/demo/data/image/4-font9_0400.a1.rlead.png","src/data/image/3-font9_0400.a1.rlead.png"))<0) return err;
  //if ((err=eggdev_project_copy_res(ctx,"src/demo/data/image/8-font6_0020.a1.rlead.png","src/data/image/4-font6_0020.a1.rlead.png"))<0) return err;
  //if ((err=eggdev_project_copy_res(ctx,"src/demo/data/image/9-cursive_0020.a1.rlead.png","src/data/image/5-cursive_0020.a1.rlead.png"))<0) return err;
  //if ((err=eggdev_project_copy_res(ctx,"src/demo/data/image/10-witchy_0020.a1.rlead.png","src/data/image/6-witchy_0020.a1.rlead.png"))<0) return err;
  if ((err=eggdev_project_copy_res(ctx,"src/editor/override/Custom.js","src/editor/override/Custom.js"))<0) return err;
  return 0;
}

/* Generate project inner.
 * Caller makes the main directory, and deletes it on errors.
 * We modify the working directory and do not restore it.
 */
 
static int eggdev_project_in_dir(struct eggdev_project_context *ctx) {
  int err;

  if (chdir(ctx->projname)<0) return -1;
  if (dir_mkdir("src")<0) return -1;
  if (dir_mkdir("src/data")<0) return -1;
  if (dir_mkdir("src/data/image")<0) return -1;
  if (dir_mkdir("src/data/strings")<0) return -1;
  if (dir_mkdir("src/data/sound")<0) return -1;
  if (dir_mkdir("src/data/song")<0) return -1;
  if (dir_mkdir("src/game")<0) return -1;
  if (dir_mkdir("src/opt")<0) return -1;
  if (dir_mkdir("src/editor")<0) return -1;
  if (dir_mkdir("src/editor/override")<0) return -1;
  if (file_write("src/editor/override/editor.css",0,0)<0) return -1;
  
  if (
    ((err=eggdev_project_generate_gitignore(ctx))<0)||
    ((err=file_write(".gitignore",ctx->scratch.v,ctx->scratch.c))<0)
  ) {
    if (err!=-2) fprintf(stderr,"%s/.gitignore: Failed to generate.\n",ctx->projname);
    return -2;
  }
  
  if (
    ((err=eggdev_project_generate_makefile(ctx))<0)||
    ((err=file_write("Makefile",ctx->scratch.v,ctx->scratch.c))<0)
  ) {
    if (err!=-2) fprintf(stderr,"%s/Makefile: Failed to generate.\n",ctx->projname);
    return -2;
  }
  
  if (
    ((err=eggdev_project_generate_metadata(ctx))<0)||
    ((err=file_write("src/data/metadata",ctx->scratch.v,ctx->scratch.c))<0)
  ) {
    if (err!=-2) fprintf(stderr,"%s/src/data/metadata: Failed to generate.\n",ctx->projname);
    return -2;
  }
  
  if (
    ((err=eggdev_project_generate_strings(ctx))<0)||
    ((err=file_write("src/data/strings/en-1",ctx->scratch.v,ctx->scratch.c))<0)
  ) {
    if (err!=-2) fprintf(stderr,"%s/src/data/strings/en-1: Failed to generate.\n",ctx->projname);
    return -2;
  }
  
  if (
    ((err=eggdev_project_generate_readme(ctx))<0)||
    ((err=file_write("README.md",ctx->scratch.v,ctx->scratch.c))<0)
  ) {
    if (err!=-2) fprintf(stderr,"%s/README.md: Failed to generate.\n",ctx->projname);
    return -2;
  }
  
  if (
    ((err=eggdev_project_generate_main(ctx))<0)||
    ((err=file_write("src/game/main.c",ctx->scratch.v,ctx->scratch.c))<0)
  ) {
    if (err!=-2) fprintf(stderr,"%s/src/game/main.c: Failed to generate.\n",ctx->projname);
    return -2;
  }
  
  if (
    ((err=eggdev_project_generate_shared_symbols(ctx))<0)||
    ((err=file_write("src/game/shared_symbols.h",ctx->scratch.v,ctx->scratch.c))<0)
  ) {
    if (err!=-2) fprintf(stderr,"%s/src/game/shared_symbols.h: Failed to generate.\n",ctx->projname);
    return -2;
  }
  
  if ((err=eggdev_project_copy_resources(ctx))<0) {
    if (err!=-2) fprintf(stderr,"%s: Unspecified error copying resources.\n",ctx->projname);
    return -2;
  }
  
  //TODO Propose opt units and copy from EGG_SDK.
  
  return 0;
}

/* Project, main entry point.
 */
 
int eggdev_main_project() {
  int err;
  struct eggdev_project_context ctx={0};

  if ((err=eggdev_buildcfg_assert(EGG_SDK))<0) return err;
  
  // Collect and validate project name.
  if (!(ctx.projname=eggdev_project_prompt("Project name: "))) {
    fprintf(stderr,"%s: Abort.\n",eggdev.exename);
    return -2;
  }
  if (!eggdev_project_name_valid(ctx.projname)) {
    fprintf(stderr,"%s: Invalid project name. Must be 1..255 of [a-zA-Z0-9_-]\n",eggdev.exename);
    eggdev_project_context_cleanup(&ctx);
    return -2;
  }
  
  // Confirm no file yet exists in the working directory by that name, and mkdir.
  char ftype=file_get_type(ctx.projname);
  if (ftype) {
    fprintf(stderr,"%s: File already exists.\n",ctx.projname);
    eggdev_project_context_cleanup(&ctx);
    return -2;
  }
  if (dir_mkdir(ctx.projname)<0) {
    fprintf(stderr,"%s: mkdir failed\n",ctx.projname);
    eggdev_project_context_cleanup(&ctx);
    return -2;
  }
  
  // Call out for all the rest.
  if ((err=eggdev_project_in_dir(&ctx))<0) {
    if (err!=-2) fprintf(stderr,"%s: Unspecified error generating project.\n",ctx.projname);
    dir_rmrf(ctx.projname);
    eggdev_project_context_cleanup(&ctx);
    return -2;
  }
  
  // Success!
  fprintf(stdout,"%s: Generated project, now get to work.\n",ctx.projname);
  eggdev_project_context_cleanup(&ctx);
  return 0;
}
