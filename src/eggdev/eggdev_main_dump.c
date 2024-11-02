#include "eggdev_internal.h"

/* Generic hex dump.
 */
 
static void eggdev_hexdump_stdout(const uint8_t *src,int srcc) {
  int srcp=0,i;
  for (;srcp<srcc;srcp+=16) {
    fprintf(stdout,"%08x |",srcp);
    for (i=0;i<16;i++) {
      if (srcp+i>=srcc) fprintf(stdout,"   ");
      else fprintf(stdout," %02x",src[srcp+i]);
    }
    fprintf(stdout," | ");
    for (i=0;i<16;i++) {
      if (srcp+i>=srcc) break;
      char ch=src[srcp+i];
      if ((ch>=0x20)&&(ch<=0x7e)) fprintf(stdout,"%c",ch);
      else fprintf(stdout,".");
    }
    fprintf(stdout,"\n");
  }
  fprintf(stdout,"%08x\n",srcc);
}

/* Dump, main entry point.
 */
 
int eggdev_main_dump() {
  if (eggdev.srcpathc!=2) {
    fprintf(stderr,"%s: Expected 'ROM' and 'TYPE:ID' at command line.\n",eggdev.exename);
    return -2;
  }
  const char *path=eggdev.srcpathv[0];
  const char *rname=eggdev.srcpathv[1];
  struct eggdev_rom rom={0};
  int err=eggdev_rom_add_path(&rom,path);
  if (err<0) {
    if (err!=-2) fprintf(stderr,"%s: Unspecified error reading ROM file.\n",path);
    eggdev_rom_cleanup(&rom);
    return -2;
  }
  struct eggdev_res *res=eggdev_rom_res_by_string(&rom,rname,-1);
  if (!res) {
    fprintf(stderr,"%s: Invalid resource identifier '%s'\n",eggdev.exename,rname);
    eggdev_rom_cleanup(&rom);
    return -2;
  }
  eggdev_hexdump_stdout(res->serial,res->serialc);
  eggdev_rom_cleanup(&rom);
  return 0;
}
