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

/* Produce so many samples of audio and return a pointer to them.
 * "samples": not frames, not bytes.
 * Each sample is 16 bits signed.
 * The rate and channel count were provided to you at egg_client_init().
 * Return zero to skip this update and emit silence.
 * You must implement this even if using the recommended built-in synthesizer; just return zero.
 * This is usually called from a separate thread. Platform guarantees that only one client hook is running at a time.
 */
void *egg_client_synth(int samplec);

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
 **********************************************************************/

#define EGG_BTN_CD 0 /* Fake button for reporting device connect/disconnect. */
#define EGG_BTN_SOUTH 1
#define EGG_BTN_EAST 2
#define EGG_BTN_WEST 3
#define EGG_BTN_NORTH 4
#define EGG_BTN_L1 5
#define EGG_BTN_R1 6
#define EGG_BTN_L2 7
#define EGG_BTN_R2 8
#define EGG_BTN_AUX2 9
#define EGG_BTN_AUX1 10
#define EGG_BTN_LP 11
#define EGG_BTN_RP 12
#define EGG_BTN_UP 13
#define EGG_BTN_DOWN 14
#define EGG_BTN_LEFT 15
#define EGG_BTN_RIGHT 16
#define EGG_BTN_AUX3 17
#define EGG_BTN_LX 18 /* analogue -128..127 */
#define EGG_BTN_LY 19
#define EGG_BTN_RX 20
#define EGG_BTN_RY 21

#define EGG_EVENT_RAW 1
#define EGG_EVENT_GAMEPAD 2
#define EGG_EVENT_KEYBOARD 3
#define EGG_EVENT_TEXT 4
#define EGG_EVENT_MMOTION 5
#define EGG_EVENT_MBUTTON 6
#define EGG_EVENT_MWHEEL 7
#define EGG_EVENT_TOUCH 8
#define EGG_EVENT_ACCEL 9
#define EGG_EVENT_IMAGE 10 /* Sent when an async image load completes. */

struct egg_event_raw {
  uint8_t type; // EGG_EVENT_RAW
  int devid;
  int btnid; // 0 for connect/disconnect; other values straight off the driver.
  int value;
};

struct egg_event_gamepad {
  uint8_t type; // EGG_EVENT_GAMEPAD
  int playerid; // Zero is the aggregate of player states.
  int btnid; // EGG_BTN_*
  int value; // (0,1) for CD..AUX3; (-128..127) for LX..RY.
  int state; // Bits, (1<<(EGG_BTN_-1)) for SOUTH..AUX3.
  int devid; // Same namespace as RAW.
};

struct egg_event_keyboard {
  uint8_t type; // EGG_EVENT_KEYBOARD
  int keycode; // USB-HID page 7; high 16 bits are always 0x00070000.
  int value; // (0,1,2)=(release,press,repeat)
};

struct egg_event_text {
  uint8_t type; // EGG_EVENT_TEXT
  int codepoint; // Unicode.
};

struct egg_event_mmotion {
  uint8_t type; // EGG_EVENT_MMOTION
  int x,y;
};

struct egg_event_mbutton {
  uint8_t type; // EGG_EVENT_MBUTTON
  int btnid,value;
  int x,y;
};

struct egg_event_mwheel {
  uint8_t type; // EGG_EVENT_MWHEEL
  int dx,dy;
  int x,y;
};

struct egg_event_touch {
  uint8_t type; // EGG_EVENT_TOUCH
  int touchid,x,y;
  int state; // (0,1,2)=(end,begin,move)
};

struct egg_event_accel {
  uint8_t type; // EGG_EVENT_ACCEL
  float x,y,z;
};

struct egg_event_image {
  uint8_t type; // EGG_EVENT_IMAGE
  int texid;
};

union egg_event {
  uint8_t type;
  struct egg_event_raw raw;
  struct egg_event_gamepad gamepad;
  struct egg_event_keyboard keyboard;
  struct egg_event_mmotion mmotion;
  struct egg_event_mbutton mbutton;
  struct egg_event_mwheel mwheel;
  struct egg_event_touch touch;
  struct egg_event_accel accel;
  struct egg_event_image image;
};

/* Event mask is bits, (1<<EGG_EVENT_*).
 * Don't enable events you don't need! There can be some processing cost.
 */
uint32_t egg_get_event_mask();
void egg_set_event_mask(uint32_t mask);

/* Show or lock system cursor.
 * Request <0 to only query the current state.
 * Both return the new state: 0 or 1.
 * Platform won't fake it. If there's no system cursor, it is always "hidden" and "unlocked".
 * When the cursor is locked, it will report relative motion only.
 * Strongly recommend that you hide whenever locked.
 */
int egg_show_cursor(int show);
int egg_lock_cursor(int lock);

/* Get properties for devices reportable via EGG_EVENT_RAW and EGG_EVENT_GAMEPAD.
 */
int egg_input_device_get_name(char *dst,int dsta,int devid);
void egg_input_device_get_ids(int *vid,int *pid,int *version,int devid);
int egg_input_device_devid_by_index(int p);

/* Audio.
 *********************************************************************/

/* If you're using egg_client_synth(), you can call this during egg_client_init() to
 * establish a maximum buffer length.
 * Platform guarantees never to call egg_client_synth() with a buffer longer than this.
 * <=0 is legal; that will disable audio.
 */
void egg_audio_set_limit(int samplec);

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
