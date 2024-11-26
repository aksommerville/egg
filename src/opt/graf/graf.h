/* graf.h
 * Client-side conveniences wrapping Egg Render API.
 */
 
#ifndef GRAF_H
#define GRAF_H

#include <stdint.h>

/* (struct graf) is a client rendering context.
 * You should create one of them, and use it everywhere in your app.
 ***********************************************************************************/

/* Global vertex buffer size in bytes.
 * Must be large enough to hold at least one of the largest vertex type (struct egg_draw_mode7).
 */
#define GRAF_BUFFER_SIZE 4096

struct graf {
  int gtint,galpha; // What we assume the global tint and alpha are, to the Platform.
  int tint,alpha; // Requested.
  int dsttexid,srctexid;
  void *mode;
  uint8_t vtxv[GRAF_BUFFER_SIZE];
  int vtxc; // bytes, not vertices
  int interpolate; // mode7 only
};

/* Reset to drop any unflushed commands and return to the default state.
 * Flush to explicitly end the current set of commands.
 * Normally, you reset at the start of egg_client_render() and flush at the end of it, and never elsewhere.
 */
void graf_reset(struct graf *graf);
void graf_flush(struct graf *graf);

void graf_set_output(struct graf *graf,int dsttexid);
void graf_set_tint(struct graf *graf,uint32_t rgba);
void graf_set_alpha(struct graf *graf,uint8_t a);

/* All remaining calls are exactly equivalent to the egg_draw_* call of the same name.
 */

void graf_draw_line(struct graf *graf,int16_t ax,int16_t ay,int16_t bx,int16_t by,uint32_t rgba);

void graf_draw_rect(struct graf *graf,int16_t x,int16_t y,int16_t w,int16_t h,uint32_t rgba);

void graf_draw_trig(struct graf *graf,int16_t ax,int16_t ay,int16_t bx,int16_t by,int16_t cx,int16_t cy,uint32_t rgba);

/* Top-left of output at (dstx,dsty) regardless of (xform).
 * Output dimensions swap with EGG_XFORM_SWAP; input dimensions are constant regardless.
 */
void graf_draw_decal(struct graf *graf,int srctexid,int16_t dstx,int16_t dsty,int16_t srcx,int16_t srcy,int16_t w,int16_t h,uint8_t xform);

/* (dstx,dsty) is the center of output.
 * (tileid) is 0..255, reading 256 equal-sized tiles from the input LRTB.
 */
void graf_draw_tile(struct graf *graf,int srctexid,int16_t dstx,int16_t dsty,uint8_t tileid,uint8_t xform);

/* Draw multiple tiles from a uniform LRTB buffer.
 * (dstx,dsty) is the center of the top-left tile.
 */
void graf_draw_tile_buffer(
  struct graf *graf,
  int srctexid,
  int16_t dstx,int16_t dsty,
  const uint8_t *tileidv,
  int colc,int rowc,int stride
);

/* (dstx,dsty) is the center of output.
 * (srcx,srcy) is the top-left of input.
 * (w,h) is the exact size in pixels in input.
 * (rotate) in radians clockwise.
 */
void graf_draw_mode7(
  struct graf *graf,
  int srctexid,
  int16_t dstx,int16_t dsty,
  int16_t srcx,int16_t srcy,
  int16_t w,int16_t h,
  float xscale,float yscale,
  float rotate,
  int interpolate
);

/* (struct texcache) is a helper for loading textures from image resources.
 * The host surely has some limit to how many textures can be loaded at once, but Egg won't help you discover it.
 * Instead, for all your read-only textures, make a texcache with a reasonable size limit, say 8?
 * The limit should be at least the maximum count of images you'll use simultaneously in one frame.
 * Textures that you modify at runtime should not be included here.
 * Initialize to zero.
 *******************************************************************************/
 
#define TEXCACHE_LIMIT 8
 
struct texcache {
  struct texcache_entry {
    int imageid;
    int texid;
    int seq;
  } entryv[TEXCACHE_LIMIT];
  int entryc;
  int evictc; // Monitor for performance reporting. If it increases every frame, you likely need to increase the limit.
  struct graf *graf; // WEAK. Caller may set directly, and we'll flush it before any eviction.
};

int texcache_get_image(struct texcache *tc,int imageid);

#endif
