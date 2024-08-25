#include "image.h"
#include "opt/serial/serial.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdint.h>

/* Decode.
 */

struct image *rawimg_decode(const void *_src,int srcc) {
  const uint8_t *src=_src;
  if (srcc<9) return 0;
  if (memcmp(src,"\x00rIm",4)) return 0;
  int w=(src[4]<<8)|src[5];
  int h=(src[6]<<8)|src[7];
  int pixelsize=src[8];
  // Requirements for image_new_alloc() are exactly the same as our own, so we lean on it for validation.
  struct image *image=image_new_alloc(pixelsize,w,h);
  if (!image) return 0;
  int available=srcc-9;
  if (image_get_pixels_length(image)!=available) {
    image_del(image);
    return 0;
  }
  memcpy(image->v,src+9,available);
  return image;
}

/* Encode.
 */
 
int rawimg_encode(struct sr_encoder *dst,struct image *image) {
  if (sr_encode_raw(dst,"\0rIm",4)<0) return -1;
  if (sr_encode_intbe(dst,image->w,2)<0) return -1;
  if (sr_encode_intbe(dst,image->h,2)<0) return -1;
  if (sr_encode_u8(dst,image->pixelsize)<0) return -1;
  if (sr_encode_raw(dst,image->v,image->stride*image->h)<0) return -1;
  return 0;
}
