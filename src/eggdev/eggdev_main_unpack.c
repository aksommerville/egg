#include "eggdev_internal.h"

/* Uncompile a generic command-list resource.
 */
 
static int eggdev_unpack_command_list(struct eggdev_res *res,struct eggdev_ns *ns) {
  struct sr_encoder dst={0};
  int err=eggdev_command_list_uncompile(&dst,res->serial,res->serialc,res->path,ns);
  if (err<0) {
    if (err!=-2) fprintf(stderr,"%s:%d: Unspecified error uncompiling command-list resource.\n",eggdev_tid_repr(res->tid),res->rid);
    sr_encoder_cleanup(&dst);
    return -2;
  }
  eggdev_res_handoff_serial(res,dst.v,dst.c);
  return 0;
}

/* unpack, convert resources.
 */
 
static int eggdev_unpack_convert(struct eggdev_rom *rom,const char *srcpath) {
  if (eggdev.raw) return 0;
  struct eggdev_res *res=rom->resv;
  int i=rom->resc;
  for (;i-->0;res++) {
    int err=0;
    switch (res->tid) {
      #define _(tag) case EGG_TID_##tag: err=eggdev_uncompile_##tag(res); break;
      EGG_TID_FOR_EACH
      #undef _
      default: {
          struct eggdev_ns *ns=eggdev_ns_by_tid(res->tid);
          if (ns) {
            err=eggdev_unpack_command_list(res,ns);
          }
        }
    }
    if (err<0) {
      if (err!=-2) fprintf(stderr,"%s:%d: Failed to convert resource\n",eggdev_tid_repr(res->tid),res->rid);
      return -2;
    }
  }
  return 0;
}

/* unpack, main operation.
 * Caller creates the directory first and deletes it recursively on errors.
 */
 
static int eggdev_unpack_write(const char *rootpath,const struct eggdev_rom *rom) {
  int tid=0;
  char subpath[1024]; // "rootpath/type/"
  int subpathc=0;
  int rootpathc=0;
  while (rootpath[rootpathc]) rootpathc++;
  if (rootpathc>=sizeof(subpath)) return -1;
  const struct eggdev_res *res=rom->resv;
  int i=rom->resc;
  for (;i-->0;res++) {
  
    if (res->tid<1) return -1;
    if (res->tid!=tid) {
      tid=res->tid;
      const char *tname=eggdev_tid_repr(tid);
      if (!tname||!tname[0]) return -1;
      int tnamec=0;
      while (tname[tnamec]) tnamec++;
      if (rootpathc+1+tnamec+1>=sizeof(subpath)) return -1;
      memcpy(subpath,rootpath,rootpathc);
      subpath[rootpathc]='/';
      memcpy(subpath+rootpathc+1,tname,tnamec);
      subpath[rootpathc+1+tnamec]=0;
      if (dir_mkdir(subpath)<0) return -1;
      subpath[rootpathc+1+tnamec]='/';
      subpathc=rootpathc+1+tnamec+1;
    }
    
    int addc=eggdev_synthesize_basename(subpath+subpathc,sizeof(subpath)-subpathc,res);
    if ((addc<1)||(subpathc>=sizeof(subpath)-addc)) return -1;
    if (file_write(subpath,res->serial,res->serialc)<0) {
      fprintf(stderr,"%s: Failed to write %d bytes for resource %s:%d\n",subpath,res->serialc,eggdev_tid_repr(res->tid),res->rid);
      return -2;
    }
  }
  return 0;
}

/* unpack, main entry point.
 */
 
int eggdev_main_unpack() {
  if (!eggdev.dstpath) {
    fprintf(stderr,"%s: Please specify output directory as '-oPATH', it must not exist yet.\n",eggdev.exename);
    return -2;
  }
  if (eggdev.srcpathc!=1) {
    fprintf(stderr,"%s: 'unpack' expects exactly one input file, have %d.\n",eggdev.exename,eggdev.srcpathc);
    return -2;
  }
  int err=eggdev_require_rom(eggdev.srcpathv[0]);
  if (err<0) return err;
  if ((err=eggdev_unpack_convert(eggdev.rom,eggdev.srcpathv[0]))<0) {
    if (err!=-2) fprintf(stderr,"%s: Unspecified error reformatting resources.\n",eggdev.srcpathv[0]);
    return -2;
  }
  if (dir_mkdir(eggdev.dstpath)<0) {
    fprintf(stderr,"%s: mkdir failed\n",eggdev.dstpath);
    return -2;
  }
  if ((err=eggdev_unpack_write(eggdev.dstpath,eggdev.rom))<0) {
    if (err!=-2) fprintf(stderr,"%s: Unspecified error writing out resource files.\n",eggdev.dstpath);
    dir_rmrf(eggdev.dstpath);
    return -2;
  }
  return 0;
}
