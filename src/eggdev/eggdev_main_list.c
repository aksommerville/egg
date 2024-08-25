#include "eggdev_internal.h"

/* Default.
 */
 
static int eggdev_list_default(const struct eggdev_rom *rom,const char *path) {
  fprintf(stdout,"%s: %d resources, total size %d b.\n",path,rom->resc,rom->totalsize);
  const struct eggdev_res *res=rom->resv;
  int i=rom->resc;
  for (;i-->0;res++) {
    fprintf(stdout,"%16s:%-6d %7d %.*s\n",eggdev_tid_repr(rom,res->tid),res->rid,res->serialc,res->namec,res->name);
  }
  return 0;
}

/* TOC.
 */
 
static int eggdev_list_toc(const struct eggdev_rom *rom,const char *path) {
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
  
  const struct eggdev_res *res=rom->resv;
  int i=rom->resc;
  for (;i-->0;res++) {
    if (!res->namec) continue;
    fprintf(stdout,"#define RID_%s_%.*s %d\n",eggdev_tid_repr(rom,res->tid),res->namec,res->name,res->rid);
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
    fprintf(stdout,"%16s %5d %7d\n",eggdev_tid_repr(rom,tid),countv[tid],sizev[tid]);
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
  struct eggdev_rom rom={0};
  int err=eggdev_rom_add_path(&rom,eggdev.srcpathv[0]);
  if (err<0) {
    if (err!=-2) fprintf(stderr,"%s: Unspecified error decoding ROM\n",eggdev.srcpathv[0]);
    eggdev_rom_cleanup(&rom);
    return -2;
  }
  if (!eggdev.format) err=eggdev_list_default(&rom,eggdev.srcpathv[0]);
  else if (!strcmp(eggdev.format,"default")) err=eggdev_list_default(&rom,eggdev.srcpathv[0]);
  else if (!strcmp(eggdev.format,"toc")) err=eggdev_list_toc(&rom,eggdev.srcpathv[0]);
  else if (!strcmp(eggdev.format,"summary")) err=eggdev_list_summary(&rom,eggdev.srcpathv[0]);
  else {
    fprintf(stderr,"%s: Unknown list format '%s'\n",eggdev.exename,eggdev.format);
    err=-2;
  }
  eggdev_rom_cleanup(&rom);
  return err;
}
