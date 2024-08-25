#include "fs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>

#ifndef O_BINARY
  #define O_BINARY 0
#endif

#if USE_mswin
  const char path_separator='\\';
#else
  const char path_separator='/';
#endif

/* Read entire file.
 */

int file_read(void *dstpp,const char *path) {
  if (!dstpp||!path||!path[0]) return -1;
  int fd=open(path,O_RDONLY|O_BINARY);
  if (fd<0) return -1;
  off_t flen=lseek(fd,0,SEEK_END);
  if ((flen<0)||(flen>INT_MAX)||lseek(fd,0,SEEK_SET)) {
    close(fd);
    return -1;
  }
  char *dst=malloc(flen);
  if (!dst) {
    close(fd);
    return -1;
  }
  int dstc=0;
  while (dstc<flen) {
    int err=read(fd,dst+dstc,flen-dstc);
    if (err<=0) {
      close(fd);
      free(dst);
      return -1;
    }
    dstc+=err;
  }
  close(fd);
  *(void**)dstpp=dst;
  return dstc;
}

/* Read entire file without seeking.
 */
 
int file_read_seekless(void *dstpp,const char *path) {
  if (!dstpp||!path||!path[0]) return -1;
  int fd=open(path,O_RDONLY|O_BINARY);
  if (fd<0) return -1;
  int dstc=0,dsta=4096;
  char *dst=malloc(dsta);
  if (!dst) {
    close(fd);
    return -1;
  }
  while (1) {
    int err=read(fd,dst+dstc,dsta-dstc);
    if (err<=0) break;
    dstc+=err;
    if (dstc<dsta) break; // assume any short read signals EOF.
    if (dsta>=0x10000000) { // safety size limit
      free(dst);
      close(fd);
      return -1;
    }
    dsta<<=1;
    void *nv=realloc(dst,dsta);
    if (!nv) {
      free(dst);
      close(fd);
      return -1;
    }
  }
  close(fd);
  *(void**)dstpp=dst;
  return dstc;
}

/* Write entire file.
 */

int file_write(const char *path,const void *src,int srcc) {
  if (!path||!path[0]||(srcc<0)||(srcc&&!src)) return -1;
  int fd=open(path,O_WRONLY|O_CREAT|O_TRUNC|O_BINARY,0666);
  if (fd<0) return -1;
  int srcp=0;
  while (srcp<srcc) {
    int err=write(fd,(char*)src+srcp,srcc-srcp);
    if (err<=0) {
      close(fd);
      unlink(path);
      return -1;
    }
    srcp+=err;
  }
  close(fd);
  return 0;
}

/* Read directory.
 */

int dir_read(
  const char *path,
  int (*cb)(const char *path,const char *base,char type,void *userdata),
  void *userdata
) {
  if (!path||!path[0]||!cb) return -1;
  char subpath[1024];
  int pathc=0;
  while (path[pathc]) pathc++;
  if (pathc>=sizeof(subpath)) return -1;
  DIR *dir=opendir(path);
  if (!dir) return -1;
  memcpy(subpath,path,pathc);
  subpath[pathc++]=path_separator;
  struct dirent *de;
  while (de=readdir(dir)) {
  
    const char *base=de->d_name;
    int basec=0;
    while (base[basec]) basec++;
    if ((basec==1)&&(base[0]=='.')) continue;
    if ((basec==2)&&(base[0]=='.')&&(base[1]=='.')) continue;
    if (pathc>=sizeof(subpath)-basec) {
      closedir(dir);
      return -1;
    }
    memcpy(subpath+pathc,base,basec+1);
    
    char type=0;
    #if USE_mswin
    #else
    switch (de->d_type) {
      case DT_REG: type='f'; break;
      case DT_DIR: type='d'; break;
      case DT_CHR: type='c'; break;
      case DT_BLK: type='b'; break;
      case DT_LNK: type='l'; break;
      case DT_SOCK: type='s'; break;
      default: type='?'; // do this only if d_type is provided and unknown. Zero means "not available".
    }
    #endif
    
    int err=cb(subpath,base,type,userdata);
    if (err) {
      closedir(dir);
      return err;
    }
  }
  closedir(dir);
  return 0;
}

/* Get file type.
 */

char file_get_type(const char *path) {
  if (!path||!path[0]) return 0;
  struct stat st={0};
  if (stat(path,&st)<0) return 0;
  if (S_ISREG(st.st_mode)) return 'f';
  if (S_ISDIR(st.st_mode)) return 'd';
  if (S_ISCHR(st.st_mode)) return 'c';
  if (S_ISBLK(st.st_mode)) return 'b';
  #ifdef S_ISLNK
    if (S_ISLNK(st.st_mode)) return 'l';
  #endif
  #ifdef S_ISSOCK
    if (S_ISSOCK(st.st_mode)) return 's';
  #endif
  return '?';
}

/* Get modification time.
 */
 
int file_get_mtime(const char *path) {
  if (!path||!path[0]) return -1;
  struct stat st={0};
  if (stat(path,&st)<0) return -1;
  return st.st_mtime;
}

/* mkdir and friends.
 */

#if USE_mswin
  #define mkdir(path,mode) mkdir(path)
#endif

int dir_mkdir(const char *path) {
  if (!path||!path[0]) return -1;
  if (mkdir(path,0775)<0) return -1;
  return 0;
}

int dir_mkdirp(const char *path) {
  if (!path||!path[0]) return -1;
  if (mkdir(path,0775)>=0) return 0;
  if (errno==EEXIST) return 0;
  if (errno!=ENOENT) return -1;
  int sepp=path_split(path,-1);
  if (sepp<=0) return -1;
  char nextpath[1024];
  if (sepp>=sizeof(nextpath)) return -1;
  memcpy(nextpath,path,sepp);
  nextpath[sepp]=0;
  if (dir_mkdirp(nextpath)<0) return -1;
  if (mkdir(path,0775)<0) return -1;
  return 0;
}

int dir_mkdirp_parent(const char *path) {
  int sepp=path_split(path,-1);
  if (sepp<=0) return -1;
  char nextpath[1024];
  if (sepp>=sizeof(nextpath)) return -1;
  memcpy(nextpath,path,sepp);
  nextpath[sepp]=0;
  return dir_mkdirp(nextpath);
}

/* Recursive deletion.
 */
 
static int dir_rmrf_cb(const char *path,const char *base,char ftype,void *userdata) {
  if (!ftype) ftype=file_get_type(path);
  if (ftype=='d') {
    int err=dir_read(path,dir_rmrf_cb,0);
    if (err<0) return err;
    if (rmdir(path)<0) return -1;
  } else {
    if (unlink(path)<0) return -1;
  }
  return 0;
}
 
int dir_rmrf(const char *path) {
  if (!path||!path[0]) return 0;
  char ftype=file_get_type(path);
  if (!ftype) return 0;
  if (ftype=='d') {
    int err=dir_read(path,dir_rmrf_cb,0);
    if (err<0) return err;
    if (rmdir(path)<0) return -1;
  } else {
    if (unlink(path)<0) return -1;
  }
  return 0;
}

/* Split path.
 */

int path_split(const char *path,int pathc) {
  if (!path) pathc=0; else if (pathc<0) { pathc=0; while (path[pathc]) pathc++; }
  int p=pathc;
  while (p&&(path[p-1]==path_separator)) p--; // strip trailing separators
  while (p&&(path[p-1]!=path_separator)) p--; // strip basename
  return p-1; // index of separator, or -1 if there wasn't one, nice and neat.
}

/* Join paths.
 */
 
int path_join(char *dst,int dsta,const char *a,int ac,const char *b,int bc) {
  if (!a) ac=0; else if (ac<0) { ac=0; while (a[ac]) ac++; }
  if (!b) bc=0; else if (bc<0) { bc=0; while (b[bc]) bc++; }
  int need_sep=(
    ac&&bc&&
    (b[0]!=path_separator)&&
    (a[ac-1]!=path_separator)
  );
  int dstc=ac+bc+(need_sep?1:0);
  if (dstc>dsta) return dstc;
  memcpy(dst,a,ac);
  if (need_sep) {
    dst[ac]=path_separator;
    memcpy(dst+ac+1,b,bc);
  } else {
    memcpy(dst+ac,b,bc);
  }
  if (dstc<dsta) dst[dstc]=0;
  return dstc;
}

/* Find home directory.
 */
 
static int path_find_home(char *dst,int dsta) {
  const char *src;
  
  if (src=getenv("HOME")) {
    int srcc=0; while (src[srcc]) srcc++;
    if ((srcc>0)&&(src[0]=='/')) {
      if (srcc<=dsta) {
        memcpy(dst,src,srcc);
        if (srcc<dsta) dst[srcc]=0;
      }
      return srcc;
    }
  }
  
  if (src=getenv("USER")) {
    int srcc=0; while (src[srcc]) srcc++;
    if (srcc) {
      int dstc=6+srcc;
      if (dstc<=dsta) {
        memcpy(dst,"/home/",6);
        memcpy(dst+6,src,srcc);
        if (dstc<dsta) dst[dstc]=0;
      }
      return dstc;
    }
  }
  
  return -1;
}

/* Resolve empty, dot, and double-dot in place.
 */
 
static int path_canonicalize_ip(char *path,int pathc) {
  if (pathc<1) return 0;
  int wp=0,rp=0;
  if (path[0]=='/') wp=rp=1; // Leading slash is sacred, no touch.
  while (rp<pathc) {
    int elemc=0;
    while ((rp+elemc<pathc)&&(path[rp+elemc]!='/')) elemc++;
    if (!elemc) {
      rp++;
    } else if ((elemc==1)&&(path[rp]=='.')) {
      rp+=2;
    } else if ((elemc==2)&&(path[rp]=='.')&&(path[rp+1]=='.')) {
      rp+=3;
      if (wp>1) {
        wp--;
        while (wp&&(path[wp-1]!='/')) wp--;
      }
    } else if (wp==rp) {
      wp+=elemc+1;
      rp+=elemc+1;
    } else {
      memmove(path+wp,path+rp,elemc);
      rp+=elemc+1;
      wp+=elemc;
      path[wp++]='/';
    }
  }
  if (wp>1) wp--; // we'll have emitted an extra trailing slash
  path[wp]=0;
  return wp;
}

/* Resolve home and working directories.
 */
 
int path_resolve(char *dst,int dsta,const char *src,int srcc) {
  if (!dst||(dsta<0)) dsta=0;
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  
  // Stage 1: Absolute, relative, or home, depending on first character.
  int dstc=0;
  if (srcc) {
    if (src[0]=='/') {
      if (srcc<=dsta) memcpy(dst,src,srcc);
      dstc=srcc;
    } else if (src[0]=='~') {
      if ((dstc=path_find_home(dst,dsta))<0) return -1;
      if (dstc<=dsta-srcc-1) memcpy(dst+dstc,src+1,srcc-1);
      dstc+=srcc-1;
    } else {
      if (getcwd(dst,dsta)!=dst) return -1;
      while (dst[dstc]) dstc++;
      if (!dstc||(dst[dstc-1]!='/')) dst[dstc++]='/';
      if (dstc<=dsta-srcc) memcpy(dst+dstc,src,srcc);
      dstc+=srcc;
    }
  }
  
  // Stage 2: Resolve empty, dot, and double-dot in place.
  if (dstc<=dsta) dstc=path_canonicalize_ip(dst,dstc);
  
  if (dstc<dsta) dst[dstc]=0;
  return dstc;
}
