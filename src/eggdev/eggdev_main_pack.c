#include "eggdev_internal.h"
#include "opt/synth/synth_formats.h"

static int eggdev_res_cmp(const void *a,const void *b) {
  const struct eggdev_res *A=a,*B=b;
  if (A->tid<B->tid) return -1;
  if (A->tid>B->tid) return 1;
  if (A->rid<B->rid) return -1;
  if (A->rid>B->rid) return 1;
  return 0;
}

/* pack, compile resources.
 */
 
static int eggdev_pack_convert(struct eggdev_rom *rom) {
  if (eggdev.raw) return 0;
  
  /* Reformat resources individually.
   */
  struct eggdev_res *res=rom->resv;
  int i=rom->resc;
  for (;i-->0;res++) {
    int err=0;
    if (eggdev_res_has_comment(res,"raw",3)) continue;
    switch (res->tid) {
      #define _(tag) case EGG_TID_##tag: err=eggdev_compile_##tag(res,rom); break;
      EGG_TID_FOR_EACH
      #undef _
    }
    if (err<0) {
      if (err!=-2) fprintf(stderr,"%s:%d: Failed to compile resource\n",eggdev_tid_repr(rom,res->tid),res->rid);
      return -2;
    }
  }
  
  return 0;
}

/* pack, main entry point.
 */
 
int eggdev_main_pack() {
  int err;
  if (!eggdev.dstpath) {
    fprintf(stderr,"%s: Please specify output path as '-oPATH'\n",eggdev.exename);
    return -2;
  }
  struct eggdev_rom rom={0};
  int i=0; for (;i<eggdev.srcpathc;i++) {
    int err=eggdev_rom_add_path(&rom,eggdev.srcpathv[i]);
    if (err<0) {
      if (err!=-2) fprintf(stderr,"%s: Unspecified error adding ROM input\n",eggdev.srcpathv[i]);
      eggdev_rom_cleanup(&rom);
      return -2;
    }
  }
  if ((err=eggdev_pack_convert(&rom))<0) {
    if (err!=-2) fprintf(stderr,"%s: Unspecified error compiling resources\n",eggdev.exename);
    eggdev_rom_cleanup(&rom);
    return -2;
  }
  if ((err=eggdev_rom_validate(&rom))<0) {
    if (err!=-2) fprintf(stderr,"%s: Unspecified error validating ROM\n",eggdev.exename);
    eggdev_rom_cleanup(&rom);
    return -2;
  }
  struct sr_encoder dst={0};
  if ((err=eggdev_rom_encode(&dst,&rom))<0) {
    if (err!=-2) fprintf(stderr,"%s: Unspecified error encoding ROM\n",eggdev.exename);
    eggdev_rom_cleanup(&rom);
    sr_encoder_cleanup(&dst);
    return -2;
  }
  eggdev_rom_cleanup(&rom);
  if (file_write(eggdev.dstpath,dst.v,dst.c)<0) {
    fprintf(stderr,"%s: Failed to write ROM file, %d bytes\n",eggdev.dstpath,dst.c);
    sr_encoder_cleanup(&dst);
    return -2;
  }
  sr_encoder_cleanup(&dst);
  return 0;
}
