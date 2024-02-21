/* fs.h
 */
 
#ifndef FS_H
#define FS_H

int file_read(void *dstpp,const char *path);
int file_write(const char *path,const void *src,int srcc);
int dir_read(const char *path,int (*cb)(const char *path,const char *base,char type,void *userdata),void *userdata);
char file_get_type(const char *path);
int dir_mkdir(const char *path);
int dir_mkdirp(const char *path);
int dir_mkdirp_parent(const char *path);
int file_unlink(const char *path);

// Returns position of last separator, or -1.
int path_split(const char *path,int pathc);

char path_separator();

#endif
