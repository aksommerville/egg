#include "archive_internal.h"
#include "opt/png/png.h"
#include "opt/qoi/qoi.h"
#include "opt/ico/ico.h"
#include "opt/rlead/rlead.h"

/* Nonzero if this PNG image uses no more than 2 colors.
 * I too often forget to force black-and-white images to actually save black-and-white.
 * So this examines the actual pixels to be sure.
 */
 
static int res_image_png_is_1bit(struct png_image *image) {
  if (image->depth==1) return 1;
  uint32_t cv[2];
  int cc=0;
  int y=image->h; while (y-->0) {
    int x=image->w; while (x-->0) {
      uint32_t pixel=png_image_read(image,x,y);
      if (!cc) cv[cc++]=pixel;
      else if (cv[0]==pixel) continue;
      else if (cc==1) cv[cc++]=pixel;
      else if (cv[1]==pixel) continue;
      else return 0;
    }
  }
  return 1;
}

/* rlead from png
 */
 
static int res_image_rlead_from_png(struct sr_encoder *dst,struct png_image *src) {
  if (!(src=png_image_reformat(src,0,0,0,0,1,0,0))) return -1;
  return rlead_encode(dst,src->pixels,src->w,src->h);
}

/* qoi from png
 */
 
static int res_image_qoi_from_png(struct sr_encoder *dst,struct png_image *src) {
  if (!(src=png_image_reformat(src,0,0,0,0,8,6,0))) return -1;
  struct qoi_image qoi={
    .w=src->w,
    .h=src->h,
    .v=src->pixels,
  };
  int err=qoi_encode(dst,&qoi);
  png_image_del(src);
  return err;
}

/* Process PNG file.
 * We do not output PNG, because I don't want to require runtime to include zlib.
 * Convert to QOI or rlead instead.
 */
 
static int res_image_process_png(struct arw_res *res) {
  struct png_image *image=png_decode(res->serial,res->serialc);
  if (!image) {
    fprintf(stderr,"%.*s: Failed to decode PNG\n",res->pathc,res->path);
    return -2;
  }
  struct sr_encoder dst={0};
  int err;
  
  if (res_image_png_is_1bit(image)) {
    err=res_image_rlead_from_png(&dst,image);
  } else {
    err=res_image_qoi_from_png(&dst,image);
  }
  png_image_del(image);
  if (err<0) {
    if (err!=-2) fprintf(stderr,"%.*s: Failed to reencode image\n",res->pathc,res->path);
    sr_encoder_cleanup(&dst);
    return -2;
  }
  if (arw_replace_handoff(&archive.arw,res->path,res->pathc,dst.v,dst.c)<0) {
    sr_encoder_cleanup(&dst);
    return -1;
  }
  return 0;
}

/* Process one image file.
 * NOT safe to add or remove resources.
 * Rewrite (res->serial) only.
 */
 
int res_image_process(struct arw_res *res) {
  switch (res->format) {
    case ARCHIVE_FORMAT_png: return res_image_process_png(res);
    case ARCHIVE_FORMAT_qoi: break; // keep qoi verbatim
    case ARCHIVE_FORMAT_ico: break; // keep ico verbatim (not clear what the intent would be for an ico)
    // And anything we don't recognize, keep verbatim.
  }
  return 0;
}
