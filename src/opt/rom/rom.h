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

#endif
