/* rom.h
 * Helper for reading Egg ROM files.
 * This is designed to work in both the native tools and the WebAssembly games.
 * So it doesn't use libc at all, or anything else.
 */
 
#ifndef ROM_H
#define ROM_H

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

#endif
