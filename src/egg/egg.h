/* egg.h
 * Defines the Egg Platform API, and also the client's required entry points.
 * An Egg game gets this API and nothing else, not even libc. You have to bring your own.
 */
 
#ifndef EGG_H
#define EGG_H

#include <stdint.h>

/* Client entry points.
 * Each game must implement all of these.
 ***********************************************************************/

/* Last opportunity to save game, etc.
 * Typically you do nothing here.
 * (status) is nonzero if we're terminating abnormally.
 * Will be called exactly once if egg_client_init() was called, even if init failed.
 * We guarantee that no other egg_client_ call will be made after.
 */
void egg_client_quit(int status);

/* First client call, guaranteed to happen exactly once, before any other.
 * Return <0 to abort.
 * The platform API is already initialized and ready to use.
 */
int egg_client_init();

/* Called repeatedly.
 * (elapsed) is the time in seconds since the last update, clamped to a sensible range.
 */
void egg_client_update(double elapsed);

/* Produce your video output.
 * This is normally called once after each egg_client_update(), though the platform has some leeway.
 * You should not change any model state during this call.
 * eg headless automation might never call it.
 */
void egg_client_render();

/* System odds and ends.
 ******************************************************************/

/* Log to the developers' console.
 * This content is generally not visible to users, but not secret either.
 * Do not include a terminal newline.
 * Most clients will want to wrap this in a client-side helper with variadic arguments.
 * We've chosen not to provide that at the platform level because passing varargs across the wasm gap gets messy.
 */
void egg_log(const char *msg);

/* Request termination at the next opportunity.
 * This does not necessarily stop your execution immediately (but it is allowed to).
 * It's not necessary to call this to abort during egg_client_init().
 */
void egg_terminate(int status);

/* Current real time in seconds, from some undefined epoch.
 * This clock may skew against the sum of elapsed times reported to egg_client_update().
 */
double egg_time_real();

/* Populate (dstv) with the local time, big-endianly, ready for human consumption.
 * eg year is 2024, month is 1 for January, hour in 0..23.
 * Up to seven fields can be reported: [year,month,day,hour,minute,second,millisecond].
 */
void egg_time_local(int *dstv,int dsta);

/* Platform maintains an idea of the user's language.
 * This is a ten-bit integer directly corresponding to two-letter ISO 639 language codes.
 * Platform will set the language initially based on system config, and possibly from your recommendations in metadata.
 * You are free to change it, but don't be a jerk: Change it only when the user asks you to.
 */
int egg_get_language();
void egg_set_language(int lang);
#define EGG_LANG_FROM_STRING(str) ((((str)[0]-'a')<<5)|((str)[1]-'a'))
#define EGG_STRING_FROM_LANG(dst,lang) { \
  (dst)[0]='a'+(((lang)>>5)&31); \
  (dst)[1]='a'+((lang)&31); \
}

/* Storage.
 *******************************************************************/

#define EGG_TID_metadata 1
#define EGG_TID_code 2
#define EGG_TID_strings 3
#define EGG_TID_image 4
#define EGG_TID_sounds 5
#define EGG_TID_song 6
#define EGG_TID_FOR_EACH \
  _(metadata) \
  _(code) \
  _(strings) \
  _(image) \
  _(sounds) \
  _(song)

/* Copy the entire ROM file into (dst) and return its length.
 * If too long, we return the correct length and don't touch (dst).
 * We do not provide piecemeal access to the ROM assets; you must implement that client-side.
 * TODO (distant future): When multi-memory becomes commonplace, expose the ROM directly as a read-only memory, so it's only resident in memory once.
 */
int egg_get_rom(void *dst,int dsta);

/* Access to the global persisted key=value store.
 * Lengths are required, you can't say <0 for nul-terminated like most of my APIs.
 * (That allows WAMR to validate buffer lengths automatically).
 * To delete a field, set it empty. Empty and absent are indistinguishable.
 * Keys must be 1..255 bytes of 0x20..0x7e, and must not contain backslash or quote (0x5c,0x22).
 * Values must be UTF-8.
 */
int egg_store_get(char *v,int va,const char *k,int kc);
int egg_store_set(const char *k,int kc,const char *v,int vc);
int egg_store_key_by_index(char *k,int ka,int p);

/* Input.
 * We only support gamepad input, and only two-state buttons.
 * Platform has some leeway to present keyboards, and maybe touch events, as gamepads.
 * Hardware devices get assigned a positive 'playerid'.
 * There may be more than one physical device associated with a playerid; they merge transparently.
 * Each player's state is expressed in 16 bits.
 * It's the Standard Mapping Gamepad, minus analogue sticks.
 * If it helps, the Evercade gamepad matches our buttons exactly.
 * And SNES comes close, only we have L2, R2, and AUX3, which SNES did not.
 **********************************************************************/

#define EGG_BTN_SOUTH 0x0001
#define EGG_BTN_EAST  0x0002
#define EGG_BTN_WEST  0x0004
#define EGG_BTN_NORTH 0x0008
#define EGG_BTN_L1    0x0010
#define EGG_BTN_R1    0x0020
#define EGG_BTN_L2    0x0040
#define EGG_BTN_R2    0x0080
#define EGG_BTN_AUX2  0x0100
#define EGG_BTN_AUX1  0x0200
#define EGG_BTN_UP    0x0400
#define EGG_BTN_DOWN  0x0800
#define EGG_BTN_LEFT  0x1000
#define EGG_BTN_RIGHT 0x2000
#define EGG_BTN_AUX3  0x4000
#define EGG_BTN_CD    0x8000 /* Fake button for reporting device connect/disconnect. */
 
/* Input states are indexed by a nonzero "playerid".
 * There is a special playerid zero which is the aggregate of all states.
 * If your metadata declares "players" >0, you will not see a playerid greater than that.
 * States are a combination of (1<<EGG_BTN_*).
 * Check EGG_BTN_CD to test whether at least one physical device is associated with a player.
 * Games should assume that input states will not change during one update cycle. But platform does not strictly guarantee it.
 */
int egg_input_get_all(int *dstv,int dsta);
int egg_input_get_one(int playerid);

/* Enter interactive input configuration.
 * On success, the game will stop updating, and will resume at some time in the future.
 * Equivalent to launching with --configure-input.
 */
int egg_input_configure();

/* Audio.
 *********************************************************************/

/* Entry points to the built-in synthesizer.
 * Sounds and songs are usually pulled from the ROM file; you just supply their ID.
 * You may also call the "_binary" versions to play something generated client-side.
 * egg_audio_event() allows you to send raw MIDI events into the bus.
 * Beware that they may conflict with the song; avoiding that is up to you.
 */
void egg_play_sound(int rid,int index);
void egg_play_song(int rid,int force,int repeat);
void egg_play_sound_binary(const void *src,int srcc);
void egg_play_song_binary(const void *src,int srcc,int force,int repeat);
void egg_audio_event(int chid,int opcode,int a,int b,int durms);

/* Get the current song playhead in seconds, or move it to a given time.
 * Platform will try to guess how far along the last delivered buffer is,
 * and report the actual song time coming out the speaker right now.
 * Obviously that will never be perfect.
 */
double egg_audio_get_playhead();
void egg_audio_set_playhead(double s);

/* Video.
 ************************************************************************/
 
#define EGG_TEXTURE_SIZE_LIMIT 4096

#define EGG_TEX_FMT_RGBA 1 /* Red first, bytewise. */
#define EGG_TEX_FMT_A8 2
#define EGG_TEX_FMT_A1 3 /* 0x80 first. */
 
#define EGG_XFORM_XREV 1
#define EGG_XFORM_YREV 2
#define EGG_XFORM_SWAP 4

/* Valid texid are >0.
 * You must delete all textures you create.
 * A newly created texture has zero size.
 */
void egg_texture_del(int texid);
int egg_texture_new();

/* Returns one of:
 *  <0: Not allocated.
 *   0: Load pending.
 *  >0: Loaded.
 * If loaded we populate (*w,*h), if you provide them.
 * Rendering to or from a texture in "pending" status will noop.
 * That status happens in web browsers, if we use the browser's async image loading facilities.
 */
int egg_texture_get_status(int *w,int *h,int texid);

/* Copy raw 32-bit RGBA pixels from a loaded texture.
 * Textures are always RGBA and always have minimal stride (ie w*4).
 * If (dsta) is short, we return the length without touching (dst).
 */
int egg_texture_get_pixels(void *dst,int dsta,int texid);

/* Replace texture with an image resource.
 * Returns status (-1,0,1).
 */
int egg_texture_load_image(int texid,int rid);

/* Replace texture with an encoded image.
 * Same as egg_texture_load_image() but sourced from client memory instead of a ROM asset.
 */
int egg_texture_load_serial(int texid,const void *src,int srcc);

/* Replace texture with raw pixels.
 * You must supply full geometry, and also the buffer's length as validation.
 */
int egg_texture_load_raw(int texid,int fmt,int w,int h,int stride,const void *src,int srcc);

/* (tint) is RGBA, default zero. If A is zero, no effect.
 * (alpha) is 0..255, default 255.
 * Globals reset to (0,255) at the start of each render cycle.
 */
void egg_draw_globals(int tint,int alpha);

void egg_draw_clear(int dsttexid,uint32_t rgba);

struct egg_draw_line {
  int16_t ax,ay,bx,by;
  uint8_t r,g,b,a;
};
void egg_draw_line(int dsttexid,const struct egg_draw_line *v,int c);

struct egg_draw_rect {
  int16_t x,y,w,h;
  uint8_t r,g,b,a;
};
void egg_draw_rect(int dsttexid,const struct egg_draw_rect *v,int c);

struct egg_draw_trig {
  int16_t ax,ay,bx,by,cx,cy;
  uint8_t r,g,b,a;
};
void egg_draw_trig(int dsttexid,const struct egg_draw_trig *v,int c);

struct egg_draw_decal {
  int16_t dstx,dsty; // Top-left (regardless of xform).
  int16_t srcx,srcy; // Top-left.
  int16_t w,h; // Exact size in (src). Output may be swapped per xform.
  uint8_t xform;
};
void egg_draw_decal(int dsttexid,int srctexid,const struct egg_draw_decal *v,int c);

struct egg_draw_tile {
  int16_t dstx,dsty; // Center.
  uint8_t tileid; // 0..255, reading LRTB in rows of 16.
  uint8_t xform;
};
void egg_draw_tile(int dsttexid,int srctexid,const struct egg_draw_tile *v,int c);

struct egg_draw_mode7 {
  int16_t dstx,dsty; // Center.
  int16_t srcx,srcy; // Top-left.
  int16_t w,h; // Exact size in (src).
  float xscale,yscale; // (1.0,1.0) for natural size.
  float rotate; // Radians clockwise.
};
void egg_draw_mode7(int dsttexid,int srctexid,const struct egg_draw_mode7 *v,int c);

#endif
