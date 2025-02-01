#include "eggdev/eggdev_internal.h"

/* Default.
 */
 
static int eggdev_list_default(const struct eggdev_rom *rom,const char *path) {
  fprintf(stdout,"%s: %d resources, total size %d b.\n",path,rom->resc,rom->totalsize);
  const struct eggdev_res *res=rom->resv;
  int i=rom->resc;
  for (;i-->0;res++) {
    fprintf(stdout,"%16s:%-6d %7d %.*s\n",eggdev_tid_repr(res->tid),res->rid,res->serialc,res->namec,res->name);
  }
  return 0;
}

/* TOC.
 * We may modify the rom.
 */
 
static int eggdev_rom_cmp(const void *a,const void *b) {
  const struct eggdev_res *A=a,*B=b;
  if (A->tid<B->tid) return -1;
  if (A->tid>B->tid) return 1;
  if (A->rid<B->rid) return -1;
  if (A->rid>B->rid) return 1;
  return 0;
}

// Given multiple resources with the same (tid,rid), drop (name) from all but one of them.
static void eggdev_toc_reduce_peers(struct eggdev_res *res,int c,const char *path) {
  int namep=-1,i;
  for (i=0;i<c;i++) {
    if (res[i].namec) {
      if ((namep>=0)&&((res[i].namec!=res[namep].namec)||memcmp(res[i].name,res[namep].name,res[i].namec))) {
        fprintf(stderr,
          "%s:WARNING: Conflicting names '%.*s' and '%.*s' for resource %s:%d\n",
          path,res[i].namec,res[i].name,res[namep].namec,res[namep].name,eggdev_tid_repr(res[i].tid),res[i].rid
        );
      } else if (namep<0) {
        namep=i;
      }
    }
  }
  if (namep<0) return;
  for (i=c;i-->0;) {
    if (i==namep) continue;
    res[i].namec=0;
  }
}
 
static int eggdev_list_toc(struct eggdev_rom *rom,const char *path) {
  fprintf(stdout,"#ifndef EGG_ROM_TOC_H\n#define EGG_ROM_TOC_H\n\n");
  
  int tid;
  for (tid=16;tid<128;tid++) {
    if (tid>=rom->tnamec) break;
    if (!rom->tnamev[tid]) continue;
    fprintf(stdout,"#define EGG_TID_%s %d\n",rom->tnamev[tid],tid);
  }
  fprintf(stdout,"#define EGG_TID_FOR_EACH_CUSTOM \\\n");
  for (tid=16;tid<128;tid++) {
    if (tid>=rom->tnamec) break;
    if (!rom->tnamev[tid]) continue;
    fprintf(stdout,"  _(%s) \\\n",rom->tnamev[tid]);
  }
  fprintf(stdout,"\n"); // This blank is important; the line before it is continued.
  
  /* Unmangle rid for resources with baked-in language code; we want just the 1..63 stem.
   */
  struct eggdev_res *res=rom->resv;
  int i=rom->resc,modified=0;
  for (;i-->0;res++) {
    if (res->lang&&(res->lang==(res->rid>>6))) {
      res->rid&=0x3f;
      modified=1;
    }
  }
  if (modified) {
    qsort(rom->resv,rom->resc,sizeof(struct eggdev_res),eggdev_rom_cmp);
    for (i=0,res=rom->resv;i<rom->resc;) {
      int ckc=1;
      while ((i+ckc<rom->resc)&&(res->tid==res[ckc].tid)&&(res->rid==res[ckc].rid)) ckc++;
      if (ckc>1) eggdev_toc_reduce_peers(res,ckc,path);
      i+=ckc;
      res+=ckc;
    }
  }
  
  for (res=rom->resv,i=rom->resc;i-->0;res++) {
    if (!res->namec) continue;
    fprintf(stdout,"#define RID_%s_%.*s %d\n",eggdev_tid_repr(res->tid),res->namec,res->name,res->rid);
  }
  
  fprintf(stdout,"\n#endif\n");
  return 0;
}

/* Summary.
 */
 
static int eggdev_list_summary(const struct eggdev_rom *rom,const char *path) {
  fprintf(stdout,"%s: Total size %d b.\n",path,rom->totalsize);
  int countv[256]={0};
  int sizev[256]={0};
  const struct eggdev_res *res=rom->resv;
  int i=rom->resc;
  int tidmax=0;
  for (;i-->0;res++) {
    if ((res->tid<0)||(res->tid>0xff)) continue;
    countv[res->tid]++;
    sizev[res->tid]+=res->serialc;
    if (res->tid>tidmax) tidmax=res->tid;
  }
  int tid=0; for (;tid<=tidmax;tid++) {
    if (!countv[tid]) continue;
    fprintf(stdout,"%16s %5d %7d\n",eggdev_tid_repr(tid),countv[tid],sizev[tid]);
  }
  return 0;
}

/* list, main entry point.
 */
 
int eggdev_main_list() {
  if (eggdev.srcpathc!=1) {
    fprintf(stderr,"%s: Exactly one input file is required for 'list'\n",eggdev.exename);
    return -2;
  }
  int err=eggdev_require_rom(eggdev.srcpathv[0]);
  if (err<0) return err;
  if (!eggdev.format) err=eggdev_list_default(eggdev.rom,eggdev.srcpathv[0]);
  else if (!strcmp(eggdev.format,"default")) err=eggdev_list_default(eggdev.rom,eggdev.srcpathv[0]);
  else if (!strcmp(eggdev.format,"toc")) err=eggdev_list_toc(eggdev.rom,eggdev.srcpathv[0]);
  else if (!strcmp(eggdev.format,"summary")) err=eggdev_list_summary(eggdev.rom,eggdev.srcpathv[0]);
  else {
    fprintf(stderr,"%s: Unknown list format '%s'\n",eggdev.exename,eggdev.format);
    err=-2;
  }
  return err;
}
