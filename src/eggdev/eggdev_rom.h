/* eggdev_rom.h
 * Split-out view of a ROM file suitable for editing.
 * Resources in the live model are always sorted by (tid,rid) and duplicates are not permitted.
 */
 
#ifndef EGGDEV_ROM_H
#define EGGDEV_ROM_H

struct sr_encoder;

struct eggdev_res {
  int tid,rid;
  char *name,*comment,*format,*path;
  int namec,commentc,formatc,pathc;
  void *serial;
  int serialc;
  int seq;
};

struct eggdev_rom {
  struct eggdev_res *resv;
  int resc,resa;
  char **tnamev; // Type name by tid for custom types, may be sparse.
  int tnamec,tnamea;
  int seq;
  int totalsize; // Sum of original size of input files.
};

void eggdev_res_cleanup(struct eggdev_res *res);
void eggdev_rom_cleanup(struct eggdev_rom *rom);

/* Add a directory, ROM, executable, HTML file, or single-resource input.
 * ie anything we're able to add.
 * If it's a directory, we manage the manifest file.
 * We bump (rom->seq) at the start. ID conflicts against existing resources, the new ones win.
 */
int eggdev_rom_add_path(struct eggdev_rom *rom,const char *path);

/* Constituents of eggdev_rom_add_path().
 * These will treat (path) as the indicated type, and will not bump (rom->seq).
 * It's unusual to call these directly.
 */
int eggdev_rom_add_directory(struct eggdev_rom *rom,const char *path);
int eggdev_rom_add_rom_serial(struct eggdev_rom *rom,const void *src,int srcc,const char *path);
int eggdev_rom_add_rom(struct eggdev_rom *rom,const char *path);
int eggdev_rom_add_executable(struct eggdev_rom *rom,const char *path);
int eggdev_rom_add_html_text(struct eggdev_rom *rom,const char *src,int srcc,const char *path);
int eggdev_rom_add_html(struct eggdev_rom *rom,const char *path);
int eggdev_rom_add_file(struct eggdev_rom *rom,const char *path);
int eggdev_rom_add_manifest_file(struct eggdev_rom *rom,const char *path);

/* Extract the meaningful components from a loose resource file's path.
 * If we're in a directory, we'll use types from the manifest file.
 */
struct eggdev_path {
  int tid;
  int rid;
  const char *tname,*name,*comment,*format;
  int tnamec,namec,commentc,formatc;
};
int eggdev_rom_parse_path(
  struct eggdev_path *parsed,
  const struct eggdev_rom *rom,
  const char *path
);

int eggdev_synthesize_basename(char *dst,int dsta,const struct eggdev_res *res);

/* Direct access to the resource list.
 */
int eggdev_rom_search(const struct eggdev_rom *rom,int tid,int rid);
struct eggdev_res *eggdev_rom_insert(struct eggdev_rom *rom,int p,int tid,int rid);
int eggdev_res_set_name(struct eggdev_res *res,const char *src,int srcc);
int eggdev_res_set_comment(struct eggdev_res *res,const char *src,int srcc);
int eggdev_res_set_format(struct eggdev_res *res,const char *src,int srcc);
int eggdev_res_set_path(struct eggdev_res *res,const char *src,int srcc);
int eggdev_res_set_serial(struct eggdev_res *res,const void *src,int srcc);
void eggdev_res_handoff_serial(struct eggdev_res *res,void *src,int srcc);

/* Comments break on dots and whitespace.
 */
int eggdev_res_has_comment(const struct eggdev_res *res,const char *token,int tokenc);

/* Drop any zero-length resources and make any assertions we can.
 * Usually logs errors if it fails.
 */
int eggdev_rom_validate(struct eggdev_rom *rom);

/* Produce the encoded ROM file.
 * We'll perform sanity checks along the way where necessary but generally we won't log errors.
 * Strongly recommended to eggdev_rom_validate() first.
 */
int eggdev_rom_encode(struct sr_encoder *dst,const struct eggdev_rom *rom);

/* Brute force scan for ROM signature.
 * We validate the entire geometry of the ROM up to its terminator or EOF.
 * Signature with no resources doesn't count.
 */
int eggdev_locate_rom(void *dstpp,const void *src,int srcc);

#endif
