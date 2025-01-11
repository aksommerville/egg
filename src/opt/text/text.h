/* text.h
 * Parse and cache string resources, and render with a simple font.
 * Requires stdlib, rom.
 */
 
#ifndef TEXT_H
#define TEXT_H

#include <stdint.h>

int text_utf8_decode(int *codepoint,const void *src,int srcc);

/* Global strings cache.
 *****************************************************************/

/* Tear down all global state and return to initial.
 * This includes the global ROM, we'll forget about it.
 */
void strings_cleanup();

/* Call this at startup to show us the ROM file.
 * You must hold it constant for as long as strings is in use.
 * Typically, you'll acquire the ROM once globally.
 */
void strings_set_rom(const void *src,int srcc);

/* Call any time the global language might have changed.
 * We'll ask Platform for it and drop cache if it did change.
 * This happens implicitly the first time, so you only need to call if you change language on the fly.
 */
void strings_check_language();

/* Look up one string.
 * If (rid<64), we apply the global language. Otherwise it's exact.
 * Returns length and puts a pointer to content at (*dstpp).
 * Beware that content is usually not terminated; it's a pointer directly into the ROM.
 * Never returns <0.
 */
int strings_get(void *dstpp,int rid,int index);

/* Take a format string (rid,index) and replace markers "%N" where N is '0'..'9' with the corresponding insertion.
 * Insertions can be a signed decimal integer 'i', a literal string 's', or another strings resource 'r'.
 * eg (rid,index) could be "%0 gave you %1 gold.", then insv could be [
 *   {'r',{.r={RID_strings_npc_names,20}}},
 *   {'i',{.i=123}},
 * ] to produce "So-and-so gave you 123 gold.".
 * Invalid insertion references produce no output.
 * "%%" produces a single '%'.
 * '%' followed by anything that isn't '%' or a digit, emits verbatim.
 */
struct strings_insertion {
  char mode; // [isr]
  union {
    int i;
    struct { const char *v; int c; } s;
    struct { int rid,ix; } r;
  };
};
int strings_format(char *dst,int dsta,int rid,int index,const struct strings_insertion *insv,int insc);

int strings_decsint_repr(char *dst,int dsta,int src);

/********************************************************************
 * Text rendering.
 *
 * Input images must be 1-bit alpha, stored big-endianly LRTB, rows padded to one byte.
 * ie 0x80 of the first byte is the top-left pixel, 0x40 is (1,0), ...
 * (this is PNG's format, and also in general the most common).
 *
 * The leftmost column of each image is a Control Column.
 * Zero in the Control Column indicates a Control Row and one indicates content.
 * Control Rows without the expected count of content rows are ignored.
 * So the first Control Column of the first image establishes the font's row height.
 *
 * Pixels in a Control Row alternate one and zero with each glyph.
 * The rightmost glyph is ignored if it has no content. No content anywhere else in a row is a valid glyph.
 * Glyphs should contain their spacing, both vertical and horizontal.
 *
 * To illustrate:
 *   . ! ! ! ! . . . ! ! ! . . . .
 *   ! . A . . B . . C C . . . . .
 *   ! A A A . B B . C . . . . . .
 *   ! A . A . B B . C C . . . . .
 *   ! . . . . . . . . . . . . . .
 *   . ! ! ! . . . . ! ! ! ! ! ! !
 *   ! . D . E E E . . . . . . . .
 *   ! D D . E E . . . . . . . . .
 *   ! D D . E E E . . . . . . . .
 *   ! . . . . . . . . . . . . . .
 * Dots are zero pixels.
 * Exclamations are ones in Control Rows and Control Columns.
 * Letters are ones in content.
 *
 * Each font has a fixed row height.
 *
 * Text must be encoded UTF-8.
 ******************************************************************/
 
struct font;

void font_del(struct font *font);

struct font *font_new();

/* Any missing glyph >0x20, we'll try replacing it with this.
 * <0x20, if the natural glyph is absent we assume empty.
 * ==0x20 is always a sensible space.
 * It's safe if there's no glyph for (codepoint), but why would you do that?
 * By default, the replacement is 0, and we quietly skip missing glyphs.
 */
void font_set_replacement(struct font *font,int codepoint);

/* Add a sheet of glyphs.
 * Fails if the new sheet overlaps an existing one.
 * (stride) may be zero to assume the minimum.
 */
int font_add_image(
  struct font *font,
  int codepoint,
  const void *pixels,
  int w,int h,int stride,
  int handoff
);

/* If you declared the ROM to strings (above),
 * we can handle looking up and decoding image resources for you.
 */
int font_add_image_resource(struct font *font,int codepoint,int imageid);

int font_get_line_height(struct font *font);

/* Width in pixels if we rendered (src) in one line.
 * If it contains newlines, we treat them like any other glyph.
 */
int font_measure_line(const struct font *font,const char *src,int srcc);

/* Populate (dst) with portions of (src), split such that each line fits within (wlimit).
 * Lines may exceed (wlimit) only if they are just one glyph.
 * Whitespace between lines gets removed.
 * Explicit newlines force a break.
 * Leading whitespace at the very start, and after a newline, is preserved.
 * May return >dsta. All entries up to (dsta) will be valid in this case.
 */
struct font_line {
  const char *v;
  int c;
  int w;
};
int font_break_lines(struct font_line *dst,int dsta,const struct font *font,const char *src,int srcc,int wlimit);

/* (dst) must be 32-bit RGBA.
 * We write alpha onto (dst) as if it were a color channel.
 * Pixels not covered by the glyph are not touched.
 * Returns horizontal advancement, typically the glyph's width plus 1.
 */
int font_render_glyph(
  void *dst,int dstw,int dsth,int dststride,int dstx,int dsty,
  const struct font *font,
  int codepoint,
  uint32_t rgba
);

/* Render a string as one line of text.
 * If there are newlines, we treat them just like any other codepoint.
 * Returns the total horizontal advancement.
 */
int font_render_string(
  void *dst,int dstw,int dsth,int dststride,int dstx,int dsty,
  const struct font *font,
  const char *src,int srcc,
  uint32_t rgba
);

/* Conveniences to measure text, allocate a temporary buffer, render it, and upload to a new texture.
 * If "oneline" text exceeds (wlimit), we truncate.
 * If "multiline" text exceeds (hlimit), we render as much as fits, possibly a partial bottom line.
 * Returned texture may have any width in (1..wlimit).
 * "tex" for a string you provide, or "texres" to look up a string resource (see above).
 * On success, returns a new texture ID which you must eventually delete with egg_texture_del().
 * <0 on any error.
 * Empty input is an error.
 */
int font_tex_oneline(const struct font *font,const char *src,int srcc,int wlimit,uint32_t rgba);
int font_tex_multiline(const struct font *font,const char *src,int srcc,int wlimit,int hlimit,uint32_t rgba);
int font_texres_oneline(const struct font *font,int rid,int p,int wlimit,uint32_t rgba);
int font_texres_multiline(const struct font *font,int rid,int p,int wlimit,int hlimit,uint32_t rgba);

#endif
