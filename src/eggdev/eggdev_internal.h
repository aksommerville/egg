#ifndef EGGDEV_INTERNAL_H
#define EGGDEV_INTERNAL_H

#include "egg/egg.h"
#include "eggdev_rom.h"
#include "opt/serial/serial.h"
#include "opt/fs/fs.h"
#include "opt/rom/rom.h"
#include "opt/http/http.h"
#include "opt/hostio/hostio.h"
#include "opt/synth/synth.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>

extern struct eggdev {

  // Populated by eggdev_configure:
  const char *exename;
  const char *command;
  const char *helptopic;
  const char *dstpath;
  const char **srcpathv;
  int srcpathc,srcpatha;
  int raw;
  int recompile;
  const char *format;
  const char **htdocsv;
  int htdocsc,htdocsa;
  const char *writepath;
  int port;
  int external;
  const char *lang; // null, "all", or a two-letter code.
  int iconImage;
  int posterImage;
  const char *default_rom_path;
  const char *audio_drivers;
  int audio_rate;
  int audio_chanc;
  int audio_buffer;
  const char *audio_device;
  int repeat;
  
  struct http_context *http;
  struct hostio *hostio; // (serve). We only use audio, and (hostio) is null if not in use.
  struct synth *synth;
  volatile int terminate;
  
} eggdev;

/* Things that we acquire from the environment at build time.
 * These may be empty but will never be null.
 */
#define EGGDEV_BUILDCFG_FOR_EACH \
  _(CC) \
  _(AS) \
  _(LD) \
  _(LDPOST) \
  _(WEB_CC) \
  _(WEB_LD) \
  _(WEB_LDPOST) \
  _(EGG_SDK) \
  _(NATIVE_TARGET) \
  _(WABT_SDK) \
  _(WAMR_SDK)
extern const struct eggdev_buildcfg {
  #define _(tag) const char *tag;
  EGGDEV_BUILDCFG_FOR_EACH
  #undef _
} eggdev_buildcfg;
int _eggdev_buildcfg_assert(const char *k,const char *v);
#define eggdev_buildcfg_assert(tag) _eggdev_buildcfg_assert(#tag,eggdev_buildcfg.tag)

// eggdev_configure won't print help on its own; it comes back as a command.
int eggdev_configure(int argc,char **argv);
void eggdev_print_help(const char *topic);

// (rom) is optional, for its manifest. Never fails to return a string.
const char *eggdev_tid_repr(const struct eggdev_rom *rom,int tid);
int eggdev_tid_eval(const struct eggdev_rom *rom,const char *src,int srcc);

const char *eggdev_guess_mime_type(const char *path,const void *src,int srcc);
int eggdev_lineno(const char *src,int srcc);

int eggdev_main_pack();
int eggdev_main_unpack();
int eggdev_main_bundle();
int eggdev_main_list();
int eggdev_main_validate();
int eggdev_main_serve();
int eggdev_main_config();
int eggdev_main_dump();
int eggdev_main_project();
int eggdev_main_metadata();
int eggdev_main_sound();

int eggdev_compile_metadata(struct eggdev_res *res,struct eggdev_rom *rom);
int eggdev_uncompile_metadata(struct eggdev_res *res,struct eggdev_rom *rom);
static inline int eggdev_compile_code(struct eggdev_res *res,struct eggdev_rom *rom) { return 0; }
static inline int eggdev_uncompile_code(struct eggdev_res *res,struct eggdev_rom *rom) { return 0; }
int eggdev_compile_strings(struct eggdev_res *res,struct eggdev_rom *rom);
int eggdev_uncompile_strings(struct eggdev_res *res,struct eggdev_rom *rom);
int eggdev_compile_image(struct eggdev_res *res,struct eggdev_rom *rom);
int eggdev_uncompile_image(struct eggdev_res *res,struct eggdev_rom *rom);
int eggdev_compile_sound(struct eggdev_res *res,struct eggdev_rom *rom); // eggdev_compile_song.c; (rom) optional
int eggdev_uncompile_sound(struct eggdev_res *res,struct eggdev_rom *rom); // ''
int eggdev_compile_song(struct eggdev_res *res,struct eggdev_rom *rom);
int eggdev_uncompile_song(struct eggdev_res *res,struct eggdev_rom *rom);

/* Generic anything-to-anything conversion for anything serializable that doesn't depend on external context.
 */
int eggdev_cvta2a(
  struct sr_encoder *dst,
  const void *src,int srcc,
  const char *refname,
  const char *dstfmt,int dstfmtc,
  const char *srcfmt,int srcfmtc
);

int eggdev_metadata_get(void *dstpp,const void *src,int srcc,const char *k,int kc);
int eggdev_strings_get(void *dstpp,const struct eggdev_rom *rom,int rid,int index);

/* Conversion entry points exposed only for eggdev_cvta2a.
 */
int eggdev_metadata_bin_from_text(struct sr_encoder *dst,const char *src,int srcc,const char *refname);
int eggdev_metadata_text_from_bin(struct sr_encoder *dst,const uint8_t *src,int srcc,const char *refname);
int eggdev_strings_bin_from_text(struct sr_encoder *dst,const char *src,int srcc,const char *refname);
int eggdev_strings_text_from_bin(struct sr_encoder *dst,const uint8_t *src,int srcc,const char *refname);
int eggdev_song_egs_from_midi(struct sr_encoder *dst,const void *src,int srcc,const char *refname);
int eggdev_song_midi_from_egs(struct sr_encoder *dst,const void *src,int srcc,const char *refname);
int eggdev_song_sanitize_wav(struct sr_encoder *dst,const uint8_t *src,int srcc,const char *refname);

/* Canned instruments used by eggdev_compile_song.
 * These emit a partial EGS Channel Header, beginning with `mode`.
 * Caller should emit `chid` and `master` first.
 */
int eggdev_encode_gm_instrument(struct sr_encoder *dst,int pid);
int eggdev_encode_gm_drums(struct sr_encoder *dst,const uint8_t *notebits/*16*/);

void eggdev_hexdump(const void *src,int srcc);

#endif
