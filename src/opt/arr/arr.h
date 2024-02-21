/* arr.h
 * Archive reader.
 * This is deliberately read-only; the main runtime never writes archives.
 */
 
#ifndef ARR_H
#define ARR_H

struct arr {
  const void *src;
  int srcc;
  struct arr_res {
    int hash,pathc,bodyc;
    const char *path;
    const void *body;
  } *resv;
  int resc,resa;
};

void arr_cleanup(struct arr *arr);

/* Drop any existing content and rebuild from this encoded archive.
 * Any content you previously read from (arr) is no longer valid.
 * *** Caller must keep (src) in scope and unchanged throughout (arr)'s life. ***
 */
int arr_init(struct arr *arr,const void *src,int srcc);

// Zero if invalid or not found; never negative.
int arr_get(void *dstpp,const struct arr *arr,const char *path,int pathc);

// (pathc) required, won't auto-detect.
int arr_hash(const char *path,int pathc);
int arr_search(const struct arr *arr,int hash,const char *path,int pathc);

#endif
