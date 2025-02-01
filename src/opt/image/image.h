/* image.h
 * Image decoders and encoders.
 */
 
#ifndef IMAGE_H
#define IMAGE_H

struct sr_encoder;

/* Originally there were four image formats.
 * But I've eliminated runtime image decoding, so now we can only support formats that browsers also support.
 * If we ever feel like supporting GIF, BMP, JPEG, etc, we could add them here.
 * But I think the recommendation will be to use PNG for everything.
 */
#define IMAGE_FORMAT_png 1

#define IMAGE_FORMAT_FOR_EACH \
  _(png)
  
#define IMAGE_HINT_ALPHA   0x01
#define IMAGE_HINT_LUMA    0x02

struct image {
  void *v;
  int w,h;
  int stride; // Distance row-to-row in bytes.
  
  /* Format is determined entirely by (pixelsize).
   * Decoders must convert from whatever their format provides, to one of our standard options.
   *  - 1: Luma or alpha. Big-endian. Rows pad to one byte minimum.
   *  - 2: Index (ie undefined). Big-endian. Rows pad to one byte minimum.
   *  - 4: Index (ie undefined). Big-endian. Rows pad to one byte minimum.
   *  - 8: Luma or alpha. Big-endian. Rows pad to one byte minimum.
   *  - 16: RGBA5551 big-endian.
   *  - 24: RGB888.
   *  - 32: RGBA8888.
   */
  int pixelsize;
  
  int hint;
};

void image_del(struct image *image);

struct image *image_decode(const void *src,int srcc);

/* Decode just the header, populating (w,h,stride,pixelsize,hint) but not (v).
 * Returns the total length, ie (stride*h).
 */
int image_decode_header(struct image *dst,const void *src,int srcc);

struct image *image_new_alloc(int pixelsize,int w,int h);

/* Generically rewrite an image in place.
 * (pixelsize,w,h) zero for no change.
 * (cvt) zero for default conversion based on pixelsize.
 * We force stride to its minimum.
 * May replace (image->v) on success, not guaranteed to.
 */
int image_reformat_in_place(
  struct image *image,
  int pixelsize,int w,int h,
  int (*cvt)(int pixel,void *userdata),
  void *userdata
);

/* Conveniences around image_reformat_in_place().
 * "canonicalize" means either 32 or 1 bit.
 * Normally, image_force_rgba is what you want. The image is then ready for upload to gfx.
 */
int image_force_rgba(struct image *image);
int image_canonicalize(struct image *image);

static inline int image_get_pixels_length(const struct image *image) {
  return image->stride*image->h;
}

int image_encode(struct sr_encoder *dst,struct image *image,int format);

#define _(tag) \
  int tag##_decode_header(struct image *dst,const void *src,int srcc); \
  struct image *tag##_decode(const void *src,int srcc); \
  int tag##_encode(struct sr_encoder *dst,struct image *image);
IMAGE_FORMAT_FOR_EACH
#undef _

int image_format_eval(const char *src,int srcc);
const char *image_format_repr(int format); // null if unknown

int image_format_guess(const void *src,int srcc);

#endif
