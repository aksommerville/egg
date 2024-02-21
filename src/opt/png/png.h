/* png.h
 * Minimal PNG decoder and encoder.
 *
 * Deviations from spec:
 *   - We do not allow interlacing.
 *   - We do not validate CRCs at decode.
 *   - Hard-coded size sanity limit (8192 pixels per axis)
 * We offer universal reformatting, but it's wildly inefficient.
 * Encoding, we use the SUB filter on every row. It's fast and simple but produces suboptimal output.
 *
 * Link: -lz
 */

#ifndef PNG_H
#define PNG_H

#include <stdint.h>

#define PNG_CHUNKID_IHDR (('I'<<24)|('H'<<16)|('D'<<8)|'R')
#define PNG_CHUNKID_IDAT (('I'<<24)|('D'<<16)|('A'<<8)|'T')
#define PNG_CHUNKID_IEND (('I'<<24)|('E'<<16)|('N'<<8)|'D')
#define PNG_CHUNKID_PLTE (('P'<<24)|('L'<<16)|('T'<<8)|'E')
#define PNG_CHUNKID_tRNS (('t'<<24)|('R'<<16)|('N'<<8)|'S')

/* Image object.
 ****************************************************************************/

struct png_image {
  int refc;
  void *pixels;
  int ownpixels;
  int w,h;
  int stride; // bytes

  uint8_t depth;
  uint8_t colortype;

  struct png_chunk {
    uint32_t chunkid;
    void *v;
    int c;
  } *chunkv;
  int chunkc,chunka;
  
  struct png_image *keepalive;
};

void png_image_del(struct png_image *image);
int png_image_ref(struct png_image *image);

/* (0,0,0,0) is legal, we do not allocate pixels in that case.
 */
struct png_image *png_image_new(int w,int h,uint8_t depth,uint8_t colortype);

/* Crop, pad, or reformat an image.
 * Zeroes to use a default, eg png_image_reformat(image,0,0,0,0,0,0,0) is guaranteed to only retain or copy image.
 * (Note that zero is a legal colortype. We treat colortype==0 as "default" only when depth is also zero).
 * If (always_copy), we produce a fresh image even if retaining was possible.
 * In general, extra chunks are dropped here. We do copy or generate PLTE and tRNS, if needed.
 */
struct png_image *png_image_reformat(
  struct png_image *image,
  int x,int y,int w,int h,
  uint8_t depth,uint8_t colortype,
  int always_copy
);

/* Calls (cb) with each pixel in LRTB order until you return nonzero or we reach the end.
 * We convert to 32-bit RGBA for all valid input formats, and we do use PLTE and tRNS as warranted.
 * (image) is const to us.
 */
int png_image_iterate(
  struct png_image *image,
  int (*cb)(struct png_image *image,int x,int y,uint32_t rgba,void *userdata),
  void *userdata
);

/* Painstakingly read or write individual pixels as 32-bit RGBA.
 */
uint32_t png_image_read(const struct png_image *image,int x,int y);
void png_image_write(struct png_image *img,int x,int y,uint32_t rgba);

/* Conveniences to rewrite a (depth,colortype) pair in place, to meet a given criterion.
 * "8bit" means channels must be 8-bit. Pixels can be 8, 16, 24, or 32.
 * Typically you'll do a few of these just before "reformat". Don't do them in-place on the struct png_image!
 */
void png_depth_colortype_legal(uint8_t *depth,uint8_t *colortype);
void png_depth_colortype_8bit(uint8_t *depth,uint8_t *colortype);
void png_depth_colortype_luma(uint8_t *depth,uint8_t *colortype);
void png_depth_colortype_rgb(uint8_t *depth,uint8_t *colortype);
void png_depth_colortype_alpha(uint8_t *depth,uint8_t *colortype);
void png_depth_colortype_opaque(uint8_t *depth,uint8_t *colortype);
int png_channel_count_for_colortype(uint8_t colortype);

// Copies (v) and adds to the end of (image)'s chunk list.
int png_image_add_chunk(struct png_image *image,uint32_t chunkid,const void *v,int c);

/* Decode.
 ****************************************************************************/

/* Simple one-shot decode to a new image, if you have the entire file in memory.
 */
struct png_image *png_decode(const void *src,int srcc);

struct png_decoder;

void png_decoder_del(struct png_decoder *decoder);
struct png_decoder *png_decoder_new();

/* Call png_decode_more() with the input file, any sizes at all are valid, we use a private cache as needed.
 * Once all input is delivered, call png_decode_finish() to get the image.
 * Finishing *does not* delete the decoder, you have to do that too.
 */
int png_decode_more(struct png_decoder *decoder,const void *src,int srcc);
struct png_image *png_decode_finish(struct png_decoder *decoder); // => STRONG

/* Encode.
 * We only offer one-shot encoding, you need enough memory to hold the entire encoded image.
 *****************************************************************************/

int png_encode(void *dstpp,const struct png_image *image);

#endif
