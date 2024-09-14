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
   * This is the bulk of what we do.
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
  
  /* Sounds have an extra quirk for packing WAV or similar into grouped resources.
   * Locate sounds with the same lower 16 bits of rid and combine them.
   * The members of one resource will sort naturally by index, but they won't be adjacent.
   */
  int changed_sounds=0;
  for (res=rom->resv,i=0;i<rom->resc;res++,i++) {
    if (res->tid!=EGG_TID_sounds) continue;
    if (res->rid<=0xffff) continue;
    if (!res->serialc) continue;
    struct synth_sounds_writer writer={0};
    if (synth_sounds_writer_init(&writer)<0) {
      synth_sounds_writer_cleanup(&writer);
      return -1;
    }
    int err=synth_sounds_writer_add(&writer,res->serial,res->serialc,res->rid>>16,res->path);
    if (err<0) {
      synth_sounds_writer_cleanup(&writer);
      return err;
    }
    struct eggdev_res *b=res+1;
    int bi=i+1;
    for (;bi<rom->resc;b++,bi++) {
      if (b->tid!=EGG_TID_sounds) continue;
      if (b->rid<=0xffff) continue;
      if (!b->serialc) continue;
      if ((err=synth_sounds_writer_add(&writer,b->serial,b->serialc,b->rid>>16,b->path))<0) {
        synth_sounds_writer_cleanup(&writer);
        return err;
      }
      b->serialc=0;
    }
    eggdev_res_handoff_serial(res,writer.dst.v,writer.dst.c); // HANDOFF writer.dst.v, do not cleanup.
    res->rid&=0xffff;
    changed_sounds=1;
  }
  if (changed_sounds) {
    qsort(rom->resv,rom->resc,sizeof(struct eggdev_res),eggdev_res_cmp);
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
