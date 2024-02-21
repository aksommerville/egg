/* arw.h
 * Archive writer.
 * This should only be of interest to build tooling.
 * The runtime should use the read-only "arr" instead.
 */
 
#ifndef ARW_H
#define ARW_H

struct sr_encoder;

struct arw {
  struct arw_res {
    char *path;
    int pathc;
    void *serial;
    int serialc;
    char *name;
    int namec;
    
    /* (format,userdata) are not encoded, and not used at all by arw.
     * Owner may place some token here to aid its own processing.
     */
    int format;
    void *userdata;
    
  } *resv;
  int resc,resa;
};

void arw_cleanup(struct arw *arw);

struct arw_res *arw_add_handoff(struct arw *arw,const char *path,int pathc,void *serial,int serialc);
struct arw_res *arw_add_copy(struct arw *arw,const char *path,int pathc,const void *serial,int serialc);

struct arw_res *arw_find(const struct arw *arw,const char *path,int pathc);
int arw_replace_handoff(struct arw *arw,const char *path,int pathc,void *serial,int serialc);
int arw_replace_copy(struct arw *arw,const char *path,int pathc,const void *serial,int serialc);
void arw_remove(struct arw *arw,const char *path,int pathc);

int arw_res_set_name(struct arw_res *res,const char *src,int srcc);

/* Validate everything we can, and force "details" to first position if present.
 * Beyond "details", archives don't have a sort requirement. If that changes, we should do it here.
 * We always populate (msg) with a NUL-terminated string. Could be a warning on success.
 */
int arw_finalize(char *msg,int msga,struct arw *arw);

int arw_encode(struct sr_encoder *dst,const struct arw *arw);

#endif
