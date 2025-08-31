#include "inmgr_internal.h"

/* Compose path.
 */
 
int inmgr_compose_path(char *dst,int dsta,const char *app,int appc,const char *sub,int subc) {
  if (!dst||(dsta<0)) dsta=0;
  if (!app) appc=0; else if (appc<0) { appc=0; while (app[appc]) appc++; }
  if (!sub) subc=0; else if (subc<0) { subc=0; while (sub[subc]) subc++; }
  while (appc&&(app[0]=='/')) { app++; appc--; }
  while (appc&&(app[appc-1]=='/')) appc--;
  while (subc&&(sub[0]=='/')) { sub++; subc--; }
  while (subc&&(sub[subc-1]=='/')) subc--;
  
  /* Path begins "~/.config/aksomm/".
   * The ".config" is used by other Linux apps, maybe other OSes too.
   * If nothing else on the host uses it, whatever, no harm done.
   * All my games should get lumped together, that's what "aksomm" is for.
   */
  int dstc=0;
  const char *src;
  if ((src=getenv("HOME"))&&src[0]) {
    int srcc=0;
    while (src[srcc]) srcc++;
    if (dstc>=dsta-srcc) return -1;
    memcpy(dst+dstc,src,srcc);
    dstc+=srcc;
  } else if ((src=getenv("USER"))&&src[0]) {
    int srcc=0;
    while (src[srcc]) srcc++;
    if (dstc>=dsta-6) return -1;
    memcpy(dst+dstc,"/home/",6);
    dstc+=6;
    if (dstc>=dsta-srcc) return -1;
    memcpy(dst+dstc,src,srcc);
    dstc+=srcc;
  } else {
    return -1;
  }
  if (dstc>=dsta) return -1;
  if (dstc&&(dst[dstc-1]!='/')) dst[dstc++]='/';
  if (dstc>=dsta-15) return -1;
  memcpy(dst+dstc,".config/aksomm/",15);
  dstc+=15;
  
  if (appc) {
    if (dstc>=dsta-appc) return -1;
    memcpy(dst+dstc,app,appc);
    dstc+=appc;
    if (dstc&&(dst[dstc-1]!='/')) dst[dstc++]='/';
  }
  
  if (subc) {
    if (dstc>=dsta-subc) return -1;
    memcpy(dst+dstc,sub,subc);
    dstc+=subc;
  }
  
  if (dstc>=dsta) return -1;
  dst[dstc]=0;
  return dstc;
}

/* Wee conveniences.
 */

int inmgr_get_input_path(char *dst,int dsta) {
  return inmgr_compose_path(dst,dsta,0,0,"input",5);
}

int inmgr_config_read(void *dstpp,const char *app,int appc,const char *sub,int subc) {
  char path[1024];
  int pathc=inmgr_compose_path(path,sizeof(path),app,appc,sub,subc);
  if ((pathc<1)||(pathc>=sizeof(path))) return -1;
  return file_read(dstpp,path);
}

int inmgr_config_write(const char *app,int appc,const char *sub,int subc,const void *src,int srcc) {
  char path[1024];
  int pathc=inmgr_compose_path(path,sizeof(path),app,appc,sub,subc);
  if ((pathc<1)||(pathc>=sizeof(path))) return -1;
  if (dir_mkdirp_parent(path)<0) return -1;
  return file_write(path,src,srcc);
}
