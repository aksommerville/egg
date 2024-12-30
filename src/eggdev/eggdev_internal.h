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

#if USE_mswin
  #define PATH_SEP_STR "\\"
  #define PATH_SEP_CHAR '\\'
#else
  #define PATH_SEP_STR "/"
  #define PATH_SEP_CHAR '/'
#endif

#define EGGDEV_NS_MODE_CMD 1 /* Command list opcodes, associated with one resource type. */
#define EGGDEV_NS_MODE_NS  2 /* Arbitrary symbols. */

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
  const char *gamehtml;
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
  const char **schemasrcv; // Holds --schema paths until we need them.
  int schemasrcc,schemasrca;
  int schema_volatile; // If nonzero, namespaces will keep (schemasrcv) populated and refresh its lists on demand. For server.
  
  struct http_context *http;
  struct hostio *hostio; // (serve). We only use audio, and (hostio) is null if not in use.
  struct synth *synth;
  volatile int terminate;
  
  /* Namespaces for command list and arbitrary user symbols.
   * Symbols are not sorted and collisions are allowed.
   */
  struct eggdev_ns {
    char *name; // Resource type or user-chosen name of namespace.
    int namec;
    int mode; // EGGDEV_NS_MODE_*
    struct eggdev_ns_entry {
      char *name;
      int namec;
      int id;
      char *args;
      int argsc;
    } *v;
    int c,a;
  } *nsv;
  int nsc,nsa;
  int ns_acquisition_in_progress;
  
  struct eggdev_rom *rom;

  uint32_t macicon;
  
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

/* Uses the global rom if needed.
 */
const char *eggdev_tid_repr(int tid);
int eggdev_tid_eval(const char *src,int srcc);

const char *eggdev_guess_mime_type(const char *path,const void *src,int srcc);
int eggdev_lineno(const char *src,int srcc);

int eggdev_require_rom(const char *path);

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
int eggdev_main_macicon();

int eggdev_compile_metadata(struct eggdev_res *res);
int eggdev_uncompile_metadata(struct eggdev_res *res);
static inline int eggdev_compile_code(struct eggdev_res *res) { return 0; }
static inline int eggdev_uncompile_code(struct eggdev_res *res) { return 0; }
int eggdev_compile_strings(struct eggdev_res *res);
int eggdev_uncompile_strings(struct eggdev_res *res);
int eggdev_compile_image(struct eggdev_res *res);
int eggdev_uncompile_image(struct eggdev_res *res);
int eggdev_compile_sound(struct eggdev_res *res); // eggdev_compile_song.c
int eggdev_uncompile_sound(struct eggdev_res *res); // ''
int eggdev_compile_song(struct eggdev_res *res);
int eggdev_uncompile_song(struct eggdev_res *res);
int eggdev_compile_map(struct eggdev_res *res);
int eggdev_uncompile_map(struct eggdev_res *res);
int eggdev_compile_tilesheet(struct eggdev_res *res);
int eggdev_uncompile_tilesheet(struct eggdev_res *res);
int eggdev_compile_sprite(struct eggdev_res *res);
int eggdev_uncompile_sprite(struct eggdev_res *res);

/* Generic anything-to-anything conversion for anything serializable that doesn't depend on external context.
 */
int eggdev_cvta2a(
  struct sr_encoder *dst,
  const void *src,int srcc,
  const char *refname,
  const char *dstfmt,int dstfmtc,
  const char *srcfmt,int srcfmtc
);

/* (src,srcc) zero to check the global rom if present.
 */
int eggdev_metadata_get(void *dstpp,const void *src,int srcc,const char *k,int kc);
int eggdev_strings_get(void *dstpp,int rid,int index);

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

/* Never returns negative or >dsta, and output is lowercase.
 */
int eggdev_normalize_suffix(char *dst,int dsta,const char *src,int srcc);

int eggdev_command_list_compile(
  struct sr_encoder *dst,
  const char *src,int srcc,
  const char *refname,int lineno0,
  const struct eggdev_ns *ns
);
int eggdev_command_list_uncompile(
  struct sr_encoder *dst,
  const uint8_t *src,int srcc,
  const char *refname,
  const struct eggdev_ns *ns
);

/* These lazy-load the cache.
 * When searching, (mode) zero matches all.
 */
int eggdev_lookup_value_from_name(int *v,int mode,const char *ns,int nsc,const char *name,int namec);
int eggdev_lookup_name_from_value(const char **dstpp,int mode,const char *ns,int nsc,int v);
struct eggdev_ns *eggdev_ns_by_name(int mode,const char *name,int namec);
struct eggdev_ns *eggdev_ns_by_tid(int tid);
struct eggdev_ns_entry *eggdev_ns_entry_by_name(const struct eggdev_ns *ns,const char *name,int namec);
struct eggdev_ns_entry *eggdev_ns_entry_by_value(const struct eggdev_ns *ns,int v);
int eggdev_ns_value_from_name(int *v,const struct eggdev_ns *ns,const char *name,int namec);
int eggdev_ns_name_from_value(const char **dstpp,const struct eggdev_ns *ns,int v);
void eggdev_ns_require();
void eggdev_ns_flush(); // Clear cached state. You must have set (eggdev.schema_volatile) before, otherwise we can't recover it.

#endif
