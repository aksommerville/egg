/* qoi.h
 * Encode and decode Quite OK Image format.
 */
 
#ifndef QOI_H
#define QOI_H

#include <stdint.h>

struct sr_encoder;

struct qoi_image {
  int w,h;
  uint32_t *v;
};

void qoi_image_del(struct qoi_image *image);

struct qoi_image *qoi_decode(const void *src,int srcc);

int qoi_encode(struct sr_encoder *dst,const struct qoi_image *image);

#endif
