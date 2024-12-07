/* rom.h
 * Helper for reading Egg ROM files.
 * This is designed to work in both the native tools and the WebAssembly games.
 * So it doesn't use libc at all, or anything else.
 */
 
#ifndef ROM_H
#define ROM_H

/* Generic ROM reader.
 *****************************************************************************/

struct rom_res {
  int tid,rid;
  const void *v;
  int c;
};

struct rom_reader {
  const unsigned char *src;
  int srcc,srcp;
  int tid,rid;
  int status; // (-1,0,1)=(error,running,finished)
  struct rom_res storage;
};

/* Validate signature and prepare for reading.
 * You should treat (reader) as opaque.
 * It does not require cleanup.
 * (src) must remain allocated and unchanged as long as the reader is active.
 */
int rom_reader_init(struct rom_reader *reader,const void *src,int srcc);

/* Advance through the next resource and return it.
 * Null at EOF or error. (reader->status<0) if error.
 * It's safe to modify the returned struct. Mind that it gets reused at the next next.
 */
struct rom_res *rom_reader_next(struct rom_reader *reader);

/* Support for standard resource types.
 ******************************************************************************/

/* Given a binary-format strings resource, callback for each non-empty string until you return nonzero.
 * (p) starts at zero and advances monotonically, except it skips empties.
 */
int rom_read_strings(
  const void *src,int srcc,
  int (*cb)(int p,const char *v,int c,void *userdata),
  void *userdata
);

/* Probably more useful than rom_read_strings(); extract just one string at a given index.
 */
int rom_string_by_index(void *dstpp,const void *src,int srcc,int p);

/* Given a binary-format metadata resource, callback for each field until you return nonzero.
 * If there are string overrides, they show up as separate fields (ie exactly how they're stored).
 */
int rom_read_metadata(
  const void *src,int srcc,
  int (*cb)(const char *k,int kc,const char *v,int vc,void *userdata),
  void *userdata
);

/* Given an entire ROM (not the metadata resource),
 * look up one field in metadata:1, and if (lang) nonzero,
 * return the appropriate string.
 * Ask for the key without the '$' suffix; we'll figure it out from there.
 */
int rom_lookup_metadata(void *dstpp,const void *src,int srcc,const char *k,int kc,int lang);

/* Validate a map resource and split into the cells image (v) and command list (cmdv).
 * This is dirt cheap, and it doesn't validate the command list structure.
 */
struct rom_map {
  int w,h;
  const unsigned char *v;
  const unsigned char *cmdv;
  int cmdc;
};
int rom_map_decode(struct rom_map *map,const void *src,int srcc);

/* Validate signature and identify command list in a sprite resource.
 * This is trivial, but providing it so its interface matches map.
 */
struct rom_sprite {
  const unsigned char *cmdv;
  int cmdc;
};
int rom_sprite_decode(struct rom_sprite *sprite,const void *src,int srcc);

/* Generic reader for command lists.
 * This can be used for both map and sprite, and devs are encouraged to use for custom resources if it fits.
 * Initialize (v,c) and zero (p).
 * rom_command_reader_next() returns >0 if (command) is populated, 0 if complete, or <0 if resource malformed.
 * Most opcodes have an intrinsic payload length, and for those you do not need to validate.
 */
struct rom_command_reader {
  const unsigned char *v;
  int c;
  int p;
};
struct rom_command {
  unsigned char opcode;
  const unsigned char *argv; // Payload, excluding opcode and length byte if applicable.
  int argc; // For (opcode<0xc0), this is knowable from (opcode) and can safely be ignored.
};
int rom_command_reader_next(struct rom_command *command,struct rom_command_reader *reader);

/* Reader for tilesheet resources.
 * These are stored as contiguous runs, not necessarily entire sheets.
 * Sheets should be initialized to zero, and overwritten in order from the returned entries.
 * Doing that is up to you.
 * We will never return (tableid) zero, or (tileid+c>256).
 * >0 if entry populated, 0 if complete, <0 if malformed.
 */
struct rom_tilesheet_reader {
  const unsigned char *v;
  int c;
  int p;
};
struct rom_tilesheet_entry {
  unsigned char tableid;
  unsigned char tileid;
  const unsigned char *v;
  int c;
};
int rom_tilesheet_reader_init(struct rom_tilesheet_reader *reader,const void *src,int srcc);
int rom_tilesheet_reader_next(struct rom_tilesheet_entry *entry,struct rom_tilesheet_reader *reader);

#endif
