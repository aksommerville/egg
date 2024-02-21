/* rlead.h
 * It stands for "Run Length Encoding, Adaptive".
 * This is a format I made up for compressed black-and-white images.
 * PNG performs better, and you should prefer that if it's practical to include zlib.
 *
 * You don't need these format details to use this API, but documenting for posterity:
 * Image begins with a 7-byte header:
 *   0000 2 Signature: 0xbb,0xad
 *   0002 2 Width
 *   0004 2 Height
 *   0006 1 Flags:
 *           0x01 Initial color.
 *           0x02 Use rowwise XOR filter.
 *   0007
 * Followed by run lengths, packed bitwise big-endianly, and biased +1.
 * (you can't encode a zero, there would be no need for it).
 * When a token is fully saturated, ie all 1s encoded, read another token one bit longer and add them.
 */
 
#ifndef RLEAD_H
#define RLEAD_H

struct sr_encoder;

/* Produce an encoded image from raw 1-bit pixels.
 * Stride must be the minimum, ie (w+7)/8.
 * Both dimensions must be in (1..32767).
 */
int rlead_encode(struct sr_encoder *dst,const void *pixels,int w,int h);

/* Decode serial data.
 * On success, puts a new pointer to the pixels at (*dstpp), caller must free it.
 * And returns (w<<16)|h.
 * Pixels have the minimum stride, (w+7)/8.
 * Returns <0 for errors.
 */
int rlead_decode(void *dstpp,const void *src,int srcc);

#endif
