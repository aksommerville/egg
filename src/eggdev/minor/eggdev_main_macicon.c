#include "eggdev/eggdev_internal.h"
#include "opt/image/image.h"

/* With the intermediate directory populated, ask iconutil to finish the job.
 */

static int eggdev_macicon_finish(const char *dstpath,const char *tmppath) {
  char cmd[1024];
  int cmdc=snprintf(cmd,sizeof(cmd),"iconutil -c icns -o %s %s",dstpath,tmppath);
  if ((cmdc<1)||(cmdc>=sizeof(cmd))) return -1;
  int err=system(cmd);
  if (err) return -1;
  return 0;
}

/* Default icon, only comes into play where there were no inputs.
 */

static int eggdev_macicon_synthesize_default(const char *tmppath) {
  char path[1024];
  int pathc=snprintf(path,sizeof(path),"%s/icon_8x8.png",tmppath);
  if ((pathc<1)||(pathc>=sizeof(path))) return -1;
  uint8_t rawimg[8*8*4]={
  #define _ 0,0,0,0,
  #define K 0,0,0,255,
  #define W 255,255,255,255,
    _ K K K K K K _
    K W W W W W W K
    K W W W W W W K
    K W W W W W W K
    K W W W W W W K
    K W W W W W W K
    K W W W W W W K
    _ K K K K K K _
  #undef _
  #undef K
  #undef W
  };
  struct image image={
    .v=rawimg,
    .w=8,
    .h=8,
    .stride=8*4,
    .pixelsize=32,
  };
  struct sr_encoder encoder={0};
  if (image_encode(&encoder,&image,IMAGE_FORMAT_png)<0) {
    fprintf(stderr,"%s: Failed to synthesize default icon.\n",path);
    sr_encoder_cleanup(&encoder);
    return -2;
  }
  int err=file_write(path,encoder.v,encoder.c);
  sr_encoder_cleanup(&encoder);
  if (err<0) {
    fprintf(stderr,"%s: Failed to write %d-byte file for default icon.\n",path,encoder.c);
    return -2;
  }
  eggdev.macicon|=1<<3;
  return 0;
}

/* Scale one RGBA image into another.
 * Images are always square, with power-of-two sizes, so we can cheat a little.
 */

static void eggdev_macicon_scale_down(struct image *dst,const struct image *src) {
  int factor=src->w/dst->w;
  if (factor<1) return;
  int srclongstride=src->stride*factor;
  uint8_t *dstrow=dst->v;
  const uint8_t *srcrow=src->v;
  int yi=dst->h;
  for (;yi-->0;dstrow+=dst->stride,srcrow+=srclongstride) {
    uint32_t *dstp=(uint32_t*)dstrow;
    const uint32_t *srcp=(uint32_t*)srcrow;
    int xi=dst->w;
    for (;xi-->0;dstp++,srcp+=factor) *dstp=*srcp;
  }
}

static void eggdev_macicon_scale_up(struct image *dst,const struct image *src) {
  int factor=dst->w/src->w;
  if (factor<1) return;
  int rptc=factor-1;
  int cpc=dst->w<<2;
  uint8_t *dstrow=dst->v;
  const uint8_t *srcrow=src->v;
  int yi=src->h;
  for (;yi-->0;srcrow+=src->stride) {
    uint32_t *dstp=(uint32_t*)dstrow;
    const uint32_t *srcp=(uint32_t*)srcrow;
    int xi=src->w;
    for (;xi-->0;srcp++) {
      int i=factor; while (i-->0) *(dstp++)=*srcp;
    }
    uint8_t *okrow=dstrow;
    dstrow+=dst->stride;
    int i=rptc; while (i-->0) {
      memcpy(dstrow,okrow,cpc);
      dstrow+=dst->stride;
    }
  }
}

static void eggdev_macicon_copy(struct image *dst,const struct image *src) {
  uint8_t *dstrow=dst->v;
  const uint8_t *srcrow=src->v;
  int cpc=dst->w<<2;
  int yi=dst->h;
  for (;yi-->0;dstrow+=dst->stride,srcrow+=src->stride) {
    memcpy(dstrow,srcrow,cpc);
  }
}

static void eggdev_macicon_scale_inner(struct image *dst,const struct image *src) {
  if ((dst->pixelsize!=32)||(src->pixelsize!=32)) return;
  if ((dst->stride&3)||(src->stride&3)) return;
  if ((dst->w!=dst->h)||(src->w!=src->h)) return;
  if (dst->w<src->w) eggdev_macicon_scale_down(dst,src);
  else if (dst->w>src->w) eggdev_macicon_scale_up(dst,src);
  else eggdev_macicon_copy(dst,src);
}

/* Generate a new image of one size from an existing one of another.
 * Not bothering to keep the source image in memory across outputs, tho that would certainly spare some effort.
 */

static int eggdev_macicon_scale(int dstsize,int srcsize,const char *tmppath) {
  
  char path[1024];
  int pathc=snprintf(path,sizeof(path),"%s/icon_%dx%d.png",tmppath,srcsize,srcsize);
  if ((pathc<1)||(pathc>=sizeof(path))) return -1;
  void *src=0;
  int srcc=file_read(&src,path);
  if (srcc<0) {
    fprintf(stderr,"%s: Failed to read file for scaling to size %d\n",path,dstsize);
    return -2;
  }
  struct image *srcimage=image_decode(src,srcc);
  free(src);
  if (!srcimage||(srcimage->pixelsize!=32)) {
    fprintf(stderr,"%s: Failed to decode image for scaling to size %d\n",path,dstsize);
    image_del(srcimage);
    return -2;
  }

  struct image *dstimage=image_new_alloc(32,dstsize,dstsize);
  if (!dstimage) {
    image_del(srcimage);
    return -1;
  }
  eggdev_macicon_scale_inner(dstimage,srcimage);
  image_del(srcimage);

  struct sr_encoder encoder={0};
  if (image_encode(&encoder,dstimage,IMAGE_FORMAT_png)<0) {
    image_del(dstimage);
    sr_encoder_cleanup(&encoder);
    return -1;
  }
  image_del(dstimage);

  pathc=snprintf(path,sizeof(path),"%s/icon_%dx%d.png",tmppath,dstsize,dstsize);
  if ((pathc<1)||(pathc>=sizeof(path))) {
    sr_encoder_cleanup(&encoder);
    return -1;
  }
  if (file_write(path,encoder.v,encoder.c)<0) {
    sr_encoder_cleanup(&encoder);
    return -1;
  }
  sr_encoder_cleanup(&encoder);
  
  eggdev.macicon|=dstsize;
  return 0;
}

/* With the intermediate directory populated, look for missing files and synthesize them.
 */

static int eggdev_macicon_sanitize(const char *tmppath) {

  /* If there were no inputs, generate a useless 8x8 placeholder.
   * This is never what you want, so we issue a warning.
   */
  if (!eggdev.macicon) {
    fprintf(stderr,"%s:WARNING: Synthesizing app icon as none was provided.\n",eggdev.exename);
    int err=eggdev_macicon_synthesize_default(tmppath);
    if (err<0) return err;
  }

  /* Find the largest extant input. We'll scale from that for everything else.
   * They're all power-of-two sizes.
   */
  uint32_t largest_size=0x40000000;
  while (!(largest_size&eggdev.macicon)) if (!(largest_size>>=1)) return -1;

  /* Any sizes we're supposed to have but don't, scale something we do have to it.
   */
  const int reqv[]={16,32,64,128,256,512,0};
  const int *size=reqv;
  for (;*size;size++) {
    if (eggdev.macicon&*size) continue;
    int err=eggdev_macicon_scale(*size,largest_size,tmppath);
    if (err<0) return err;
  }
  
  return 0;
}

/* Add image to icon set, from ROM or image file.
 */

static int eggdev_macicon_add_serial(const char *tmppath,const void *src,int srcc,const char *srcpath) {
  struct image *image=image_decode(src,srcc);
  if (!image) {
    fprintf(stderr,"%s: Failed to decode image\n",srcpath);
    return -2;
  }
  if ((image->w!=image->h)||(image_reformat_in_place(image,32,0,0,0,0)<0)) {
    fprintf(stderr,"%s: Image not square, or failed converting to 32-bit.\n",srcpath);
    image_del(image);
    return -2;
  }
  int size=1;
  while (size!=image->w) {
    size<<=1;
    if (size>=0x10000000) {
      fprintf(stderr,"%s: Image size must be a power of two (have %d)\n",srcpath,image->w);
      image_del(image);
      return -2;
    }
  }
  if (eggdev.macicon&size) {
    fprintf(stderr,"%s: Multiple inputs of size %d\n",srcpath,size);
    image_del(image);
    return -2;
  }
  char dstpath[1024];
  int dstpathc=snprintf(dstpath,sizeof(dstpath),"%s/icon_%dx%d.png",tmppath,size,size);
  if ((dstpathc<1)||(dstpathc>=sizeof(dstpath))) {
    image_del(image);
    return -2;
  }
  struct sr_encoder encoder={0};
  if (image_encode(&encoder,image,IMAGE_FORMAT_png)<0) {
    fprintf(stderr,"%s: Failed to reencode image.\n",srcpath);
    image_del(image);
    sr_encoder_cleanup(&encoder);
    return -2;
  }
  if (file_write(dstpath,encoder.v,encoder.c)<0) {
    fprintf(stderr,"%s: Failed to write reencoded icon from '%s', %d bytes\n",dstpath,srcpath,encoder.c);
    image_del(image);
    sr_encoder_cleanup(&encoder);
    return -2;
  }
  sr_encoder_cleanup(&encoder);
  image_del(image);
  eggdev.macicon|=size;
  return 0;
}

/* Add loose PNG file to intermediate directory.
 */

static int eggdev_macicon_add_png(const char *tmppath,const char *srcpath) {
  void *src=0;
  int srcc=file_read(&src,srcpath);
  if (srcc<0) {
    fprintf(stderr,"%s: Failed to read file\n",srcpath);
    return -2;
  }
  int err=eggdev_macicon_add_serial(tmppath,src,srcc,srcpath);
  free(src);
  if (err<0) {
    if (err!=-2) fprintf(stderr,"%s: Unspecified error adding image file to icons\n",srcpath);
    return -2;
  }
  return 0;
}

/* Extract icon from ROM and add to intermediate directory.
 */

static int eggdev_macicon_add_rom(const char *tmppath,const char *srcpath) {
  if (eggdev.rom) {
    fprintf(stderr,"%s: Multiple ROM inputs. We can only do one.\n",srcpath);
    return -2;
  }
  if (!(eggdev.rom=calloc(1,sizeof(struct eggdev_rom)))) return -1;
  int err=eggdev_rom_add_path(eggdev.rom,srcpath);
  if (err<0) {
    if (err!=-2) fprintf(stderr,"%s: Unspecified error decoding ROM\n",srcpath);
    return -2;
  }
  const char *imageidsrc=0;
  int imageidsrcc=eggdev_metadata_get(&imageidsrc,0,0,"iconImage",9);
  if (imageidsrcc<1) {
    fprintf(stderr,"%s:WARNING: No 'iconImage' in ROM metadata.\n",srcpath);
    return 0;
  }
  int imageid=0;
  if ((sr_int_eval(&imageid,imageidsrc,imageidsrcc)<2)||(imageid<1)||(imageid>0xffff)) {
    fprintf(stderr,"%s: Invalid 'iconImage' = '%.*s'\n",srcpath,imageidsrcc,imageidsrc);
    return -2;
  }
  int resp=eggdev_rom_search(eggdev.rom,EGG_TID_image,imageid);
  if (resp<0) {
    fprintf(stderr,"%s: Metadata lists iconImage = %d, but image:%d not found.\n",srcpath,imageid,imageid);
    return -2;
  }
  const void *src=eggdev.rom->resv[resp].serial;
  int srcc=eggdev.rom->resv[resp].serialc;
  return eggdev_macicon_add_serial(tmppath,src,srcc,srcpath);
}

/* Generate icons, given output path, input paths, and an intermediate 'iconset' directory created.
 */

static int eggdev_macicon_inner(const char *dstpath,const char *tmppath,const char **srcpathv,int srcpathc) {

  /* (eggdev.macicon) is a little-endian bit mask, each bit corresponding to a square icon (2**n) pixels on each axis.
   * We'll fail if the user supplies more than one icon per size.
   */
  eggdev.macicon=0;
  
  int i=0; for (;i<srcpathc;i++) {
    const char *path=srcpathv[i];
    int pathc=0,err;
    while (path[pathc]) pathc++;
    if ((pathc>=4)&&!memcmp(path+pathc-4,".png",4)) {
      err=eggdev_macicon_add_png(tmppath,path);
    } else {
      err=eggdev_macicon_add_rom(tmppath,path);
    }
    if (err<0) {
      if (err!=-2) fprintf(stderr,"%s: Unspecified error adding icons.\n",path);
      return -2;
    }
  }
  int err=eggdev_macicon_sanitize(tmppath);
  if (err<0) {
    if (err!=-2) fprintf(stderr,"%s: Unspecified error digesting icons.\n",dstpath);
    return -2;
  }
  return eggdev_macicon_finish(dstpath,tmppath);
}

/* macicon, main entry point.
 */

int eggdev_main_macicon() {
  if (!eggdev.dstpath) {
    fprintf(stderr,"%s: Please specify output path as '-oPATH'\n",eggdev.exename);
    return -2;
  }
  const char *iconset_path="tmp-macicon.iconset";
  if (dir_mkdir(iconset_path)<0) return -1;
  int err=eggdev_macicon_inner(eggdev.dstpath,iconset_path,eggdev.srcpathv,eggdev.srcpathc);
  dir_rmrf(iconset_path);
  if (err<0) {
    if (err!=-2) fprintf(stderr,"%s: Unspecified error processing icons.\n",eggdev.dstpath);
    return -2;
  }
  return 0;
}
