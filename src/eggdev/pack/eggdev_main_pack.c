#include "eggdev/eggdev_internal.h"
#include "opt/synth/synth_formats.h"

static int eggdev_res_cmp(const void *a,const void *b) {
  const struct eggdev_res *A=a,*B=b;
  if (A->tid<B->tid) return -1;
  if (A->tid>B->tid) return 1;
  if (A->rid<B->rid) return -1;
  if (A->rid>B->rid) return 1;
  return 0;
}

/* Compile a generic command-list resource.
 */
 
static int eggdev_pack_command_list(struct eggdev_res *res,struct eggdev_ns *ns) {
  struct sr_encoder dst={0};
  int err=eggdev_command_list_compile(&dst,res->serial,res->serialc,res->path,1,ns);
  if (err<0) {
    if (err!=-2) fprintf(stderr,"%s: Unspecified error compiling command-list resource.\n",res->path);
    sr_encoder_cleanup(&dst);
    return -2;
  }
  eggdev_res_handoff_serial(res,dst.v,dst.c);
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
      #define _(tag) case EGG_TID_##tag: err=eggdev_compile_##tag(res); break;
      EGG_TID_FOR_EACH
      #undef _
      default: {
          struct eggdev_ns *ns=eggdev_ns_by_tid(res->tid);
          if (ns) {
            err=eggdev_pack_command_list(res,ns);
          }
        }
    }
    if (err<0) {
      if (err!=-2) fprintf(stderr,"%s:%d: Failed to compile resource\n",eggdev_tid_repr(res->tid),res->rid);
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
  if (!(eggdev.rom=calloc(1,sizeof(struct eggdev_rom)))) return -1;
  int i=0; for (;i<eggdev.srcpathc;i++) {
    int err=eggdev_rom_add_path(eggdev.rom,eggdev.srcpathv[i]);
    if (err<0) {
      if (err!=-2) fprintf(stderr,"%s: Unspecified error adding ROM input\n",eggdev.srcpathv[i]);
      return -2;
    }
  }
  if ((err=eggdev_pack_convert(eggdev.rom))<0) {
    if (err!=-2) fprintf(stderr,"%s: Unspecified error compiling resources\n",eggdev.exename);
    return -2;
  }
  if ((err=eggdev_rom_validate(eggdev.rom))<0) {
    if (err!=-2) fprintf(stderr,"%s: Unspecified error validating ROM\n",eggdev.exename);
    return -2;
  }
  struct sr_encoder dst={0};
  if ((err=eggdev_rom_encode(&dst,eggdev.rom))<0) {
    if (err!=-2) fprintf(stderr,"%s: Unspecified error encoding ROM\n",eggdev.exename);
    sr_encoder_cleanup(&dst);
    return -2;
  }
  if (file_write(eggdev.dstpath,dst.v,dst.c)<0) {
    fprintf(stderr,"%s: Failed to write ROM file, %d bytes\n",eggdev.dstpath,dst.c);
    sr_encoder_cleanup(&dst);
    return -2;
  }
  sr_encoder_cleanup(&dst);
  return 0;
}
