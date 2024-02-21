#include "fs.h"
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <sys/stat.h>

#ifndef O_BINARY /* for MS Windows */
  #define O_BINARY 0
#endif

#define PATH_SEPARATOR_CHAR '/'

/* Read file.
 */
 
int file_read(void *dstpp,const char *path) {
  int fd=open(path,O_RDONLY|O_BINARY);
  if (fd<0) return -1;
  off_t flen=lseek(fd,0,SEEK_END);
  if ((flen<0)||(flen>INT_MAX)||lseek(fd,0,SEEK_SET)) {
    close(fd);
    return -1;
  }
  char *dst=flen?malloc(flen):malloc(1);
  if (!dst) {
    close(fd);
    return -1;
  }
  int dstc=0,err;
  while (dstc<flen) {
    if ((err=read(fd,dst+dstc,flen-dstc))<=0) {
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

/* Write file.
 */
 
int file_write(const char *path,const void *src,int srcc) {
  if (!path||(srcc<0)||(srcc&&!src)) return -1;
  int fd=open(path,O_WRONLY|O_CREAT|O_TRUNC|O_BINARY,0666);
  if (fd<0) return -1;
  int srcp=0,err;
  while (srcp<srcc) {
    if ((err=write(fd,(char*)src+srcp,srcc-srcp))<=0) {
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
 
int dir_read(const char *path,int (*cb)(const char *path,const char *base,char type,void *userdata),void *userdata) {
  int pathc=0;
  while (path[pathc]) pathc++;
  char subpath[1024];
  if (pathc>=sizeof(subpath)) return -1;
  DIR *dir=opendir(path);
  if (!dir) return -1;
  memcpy(subpath,path,pathc);
  subpath[pathc++]=PATH_SEPARATOR_CHAR;
  struct dirent *de;
  while (de=readdir(dir)) {
    
    const char *base=de->d_name;
    if ((base[0]=='.')||!base[0]) continue;
    int basec=0;
    while (base[basec]) basec++;
    if (pathc>=sizeof(subpath)-basec) {
      closedir(dir);
      return -1;
    }
    memcpy(subpath+pathc,base,basec+1);
    
    char type=0;
    #if !USE_mswin
      switch (de->d_type) {
        case DT_DIR: type='d'; break;
        case DT_REG: type='f'; break;
        default: type='?';
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
  struct stat st={0};
  if (stat(path,&st)<0) return 0;
  if (S_ISREG(st.st_mode)) return 'f';
  if (S_ISDIR(st.st_mode)) return 'd';
  if (S_ISCHR(st.st_mode)) return 'c';
  if (S_ISBLK(st.st_mode)) return 'b';
  return '?';
}

/* Make directory.
 */
 
#if USE_mswin
  #define mkdir(path,mode) mkdir(path)
#endif
 
int dir_mkdir(const char *path) {
  if (mkdir(path,0775)<0) return -1;
  return 0;
}

int dir_mkdirp(const char *path) {
  if (mkdir(path,0775)>=0) return 0;
  if (errno==EEXIST) return 0;
  if (errno!=ENOENT) return -1;
  char subpath[1024];
  int pathc=0;
  while (path[pathc]) pathc++;
  if (pathc>=sizeof(subpath)) return -1;
  memcpy(subpath,path,pathc+1);
  int sepp=pathc-1;
  while (1) {
    while ((sepp>=0)&&(subpath[sepp]!=PATH_SEPARATOR_CHAR)) sepp--;
    if (sepp<0) return -1;
    subpath[sepp]=0;
    if (mkdir(subpath,0775)>=0) break;
    if (errno!=ENOENT) return -1;
  }
  while (sepp<pathc) {
    subpath[sepp]=PATH_SEPARATOR_CHAR;
    if (mkdir(subpath,0775)<0) return -1;
    while ((sepp<pathc)&&subpath[sepp]) sepp++;
  }
  return 0;
}

int dir_mkdirp_parent(const char *path) {
  if (!path) return -1;
  int pathc=0,sepp=-1;
  while (path[pathc]) {
    if (path[pathc]=='/') sepp=pathc;
    pathc++;
  }
  if (sepp<=0) return -1;
  char parent[1024];
  if (sepp>=sizeof(parent)) return -1;
  memcpy(parent,path,sepp);
  parent[sepp]=0;
  return dir_mkdirp(parent);
}

/* Delete file.
 */
 
int file_unlink(const char *path) {
  if (file_get_type(path)=='d') {
    if (rmdir(path)<0) return -1;
  } else {
    if (unlink(path)<0) return -1;
  }
  return 0;
}

/* Split path.
 */
 
int path_split(const char *path,int pathc) {
  if (!path) return -1;
  if (pathc<0) { pathc=0; while (path[pathc]) pathc++; }
  while (pathc&&(path[pathc-1]!=PATH_SEPARATOR_CHAR)) pathc--;
  return pathc-1;
}

char path_separator() {
  return PATH_SEPARATOR_CHAR;
}
