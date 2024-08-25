/* fs.h
 * Filesystem conveniences for typical systems.
 */
 
#ifndef FS_H
#define FS_H

/* Read a file into a newly-allocated buffer.
 * Caller must free (*dstpp) on success.
 * Seekable regular files only.
 */
int file_read(void *dstpp,const char *path);

/* Same as file_read but operates incrementally without seeking.
 * Beware! If you give it a character device or something, this may block forever.
 */
int file_read_seekless(void *dstpp,const char *path);

/* Write entire regular file in one shot.
 */
int file_write(const char *path,const void *src,int srcc);

/* Call (cb) for each file directly under directory (path).
 * Stops when (cb) returns nonzero, and returns the same.
 * (type) may be zero if dirent doesn't provide it.
 * In that case, if you want the type, call file_get_type.
 */
int dir_read(
  const char *path,
  int (*cb)(const char *path,const char *base,char type,void *userdata),
  void *userdata
);

/* 0: Error eg file not found.
 * 'f': Regular file.
 * 'd': Directory.
 * 'b': Block device.
 * 'c': Character device.
 * 'l': Symlink (shouldn't ever happen here; we follow links).
 * 's': Socket.
 * '?': File exists but type unknown.
 */
char file_get_type(const char *path);

int file_get_mtime(const char *path);

/* Make a directory.
 * dir_mkdirp creates parent directories recursively, and does not report an error if already existing.
 * dir_mkdirp_parent is a convenience that splits the path and runs dir_mkdirp only on the directory portion.
 */
int dir_mkdir(const char *path);
int dir_mkdirp(const char *path);
int dir_mkdirp_parent(const char *path);

/* Delete directory and all of its content recursively, or delete a regular file.
 * No error if it already doesn't exist.
 * Equivalent to `rm -rf path`
 */
int dir_rmrf(const char *path);

/* Index of the last significant separator in (path).
 * ie length of the dirname, or (path+n+1) for the basename.
 */
int path_split(const char *path,int pathc);

/* Join two paths into the caller's buffer, adding a separator if needed.
 */
int path_join(char *dst,int dsta,const char *a,int ac,const char *b,int bc);

extern const char path_separator;

/* Resolve leading tilde and working directory.
 * This is a syntatic operation: We don't care whether the file actually exists.
 */
int path_resolve(char *dst,int dsta,const char *src,int srcc);

#endif
