#include "eggdev_internal.h"
#include "opt/image/image.h"

/* Conversion context.
 */
 
struct eggdev_imgcvt {
  struct eggdev_res *res;
  struct image *image;
  int format,pixelsize,hint; // Zero for default.
};

static void eggdev_imgcvt_cleanup(struct eggdev_imgcvt *ctx) {
  image_del(ctx->image);
}

/* Read this comment and apply to the context.
 */
 
static int eggdev_imgcvt_apply_comment_1(struct eggdev_imgcvt *ctx,const char *src,int srcc) {
  if (srcc<1) return -1;
  int format=image_format_eval(src,srcc);
  if (format>0) {
    ctx->format=format;
    return 1;
  }
  switch (srcc) {
    case 1: switch (src[0]) {
        case '1': ctx->pixelsize=1; return 1;
        case '2': ctx->pixelsize=2; return 1;
        case '4': ctx->pixelsize=4; return 1;
        case '8': ctx->pixelsize=8; return 1;
        case 'a': ctx->hint|=IMAGE_HINT_ALPHA; return 1;
        case 'y': ctx->hint|=IMAGE_HINT_LUMA; return 1;
      } break;
    case 2: {
        if (!memcmp(src,"a1",2)) { ctx->pixelsize=1; ctx->hint|=IMAGE_HINT_ALPHA; return 1; }
        if (!memcmp(src,"y1",2)) { ctx->pixelsize=1; ctx->hint|=IMAGE_HINT_LUMA; return 1; }
        if (!memcmp(src,"a2",2)) { ctx->pixelsize=2; ctx->hint|=IMAGE_HINT_ALPHA; return 1; }
        if (!memcmp(src,"y2",2)) { ctx->pixelsize=2; ctx->hint|=IMAGE_HINT_LUMA; return 1; }
        if (!memcmp(src,"a4",2)) { ctx->pixelsize=4; ctx->hint|=IMAGE_HINT_ALPHA; return 1; }
        if (!memcmp(src,"y4",2)) { ctx->pixelsize=4; ctx->hint|=IMAGE_HINT_LUMA; return 1; }
        if (!memcmp(src,"a8",2)) { ctx->pixelsize=8; ctx->hint|=IMAGE_HINT_ALPHA; return 1; }
        if (!memcmp(src,"y8",2)) { ctx->pixelsize=8; ctx->hint|=IMAGE_HINT_LUMA; return 1; }
        if (!memcmp(src,"16",2)) { ctx->pixelsize=16; return 1; }
        if (!memcmp(src,"24",2)) { ctx->pixelsize=24; return 1; }
        if (!memcmp(src,"32",2)) { ctx->pixelsize=32; return 1; }
      } break;
  }
  fprintf(stderr,"%s: Unknown image hint '%.*s'\n",ctx->res->path,srcc,src);
  return -2;
}
 
static int eggdev_imgcvt_apply_comment(struct eggdev_imgcvt *ctx,const char *src,int srcc) {
  int srcp=0,err;
  while (srcp<srcc) {
    if (src[srcp]=='.') { srcp++; continue; }
    const char *token=src+srcp;
    int tokenc=0;
    while ((srcp<srcc)&&(src[srcp++]!='.')) tokenc++;
    if ((err=eggdev_imgcvt_apply_comment_1(ctx,token,tokenc))<0) return err;
  }
  return 0;
}

/* Convert image in context.
 */
 
static int eggdev_imgcvt_inner(struct eggdev_imgcvt *ctx,int to_rom) {

  /* Default behavior is to do nothing.
   * If the comment has some instructions in it, we'll proceed.
   */
  int err=eggdev_imgcvt_apply_comment(ctx,ctx->res->comment,ctx->res->commentc);
  if (err<0) {
    if (err!=-2) fprintf(stderr,"%s: Invalid image comment '%.*s'\n",ctx->res->path,ctx->res->commentc,ctx->res->comment);
    return -2;
  }
  if (!ctx->format&&!ctx->pixelsize&&!ctx->hint) return 0;
  
  if (!(ctx->image=image_decode(ctx->res->serial,ctx->res->serialc))) {
    fprintf(stderr,"%s: Failed to decode %d-byte image.\n",ctx->res->path,ctx->res->serialc);
    return -2;
  }
  if (image_reformat_in_place(ctx->image,ctx->pixelsize,0,0,0,0)<0) {
    fprintf(stderr,"%s: Failed to convert pixels from %d to %d bit\n",ctx->res->path,ctx->image->pixelsize,ctx->pixelsize);
    return -2;
  }
  ctx->image->hint=ctx->hint;
  
  /* If they didn't request an output format, use the input format.
   */
  if (!ctx->format) {
    if ((ctx->format=image_format_eval(ctx->res->format,ctx->res->formatc))<1) {
      if ((ctx->format=image_format_guess(ctx->res->serial,ctx->res->serialc))<1) {
        fprintf(stderr,"%s: Unable to determine output format.\n",ctx->res->path);
        return -2;
      }
    }
  }
  
  struct sr_encoder dst={0};
  if (image_encode(&dst,ctx->image,ctx->format)<0) {
    fprintf(stderr,"%s: Failed to reencode image to '%s'\n",ctx->res->path,image_format_repr(ctx->format));
    sr_encoder_cleanup(&dst);
    return -2;
  }
  eggdev_res_handoff_serial(ctx->res,dst.v,dst.c);
  
  return 0;
}

/* Main entry points.
 */
 
int eggdev_compile_image(struct eggdev_res *res) {
  struct eggdev_imgcvt ctx={
    .res=res,
  };
  int err=eggdev_imgcvt_inner(&ctx,1);
  eggdev_imgcvt_cleanup(&ctx);
  if (err>=0) return 0;
  return err;
}

int eggdev_uncompile_image(struct eggdev_res *res) {
  struct eggdev_imgcvt ctx={
    .res=res,
  };
  int err=eggdev_imgcvt_inner(&ctx,0);
  eggdev_imgcvt_cleanup(&ctx);
  if (err>=0) return 0;
  return err;
}
