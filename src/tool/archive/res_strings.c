#include "archive_internal.h"

/* Nonzero if this looks like a multi-resource strings file.
 */
 
int res_strings_check_path(const char *path,int pathc) {
  if (pathc!=4) return 0;
  if (path[0]!='s') return 0;
  if (path[1]!='/') return 0;
  if ((path[2]<'a')||(path[2]>'z')) return 0;
  if ((path[3]<'a')||(path[3]>'z')) return 0;
  return 1;
}

/* Decoder context.
 */
 
struct strings {
  struct arw_res *res;
  struct string {
    int id; // zero if named and not assigned yet
    const char *name;
    int namec;
    char *v;
    int c;
    int lineno;
  } *v;
  int c,a;
  int maxid; // we don't look for holes; new ids always go at the end
};

static void string_cleanup(struct string *string) {
  if (string->v) free(string->v);
}

static void strings_cleanup(struct strings *strings) {
  if (strings->v) {
    while (strings->c-->0) string_cleanup(strings->v+strings->c);
    free(strings->v);
  }
}

/* Validate id or name, and add to list.
 */
 
static struct string *strings_add(struct strings *strings,const char *src,int srcc) {
  if (srcc<1) return 0;
  if (strings->c>=strings->a) {
    int na=strings->a+256;
    if (na>INT_MAX/sizeof(struct string)) return 0;
    void *nv=realloc(strings->v,sizeof(struct string)*na);
    if (!nv) return 0;
    strings->v=nv;
    strings->a=na;
  }
  struct string *string=strings->v+strings->c++;
  memset(string,0,sizeof(struct string));
  if (sr_int_eval(&string->id,src,srcc)>=2) {
    if ((string->id<1)||(string->id>0xffff)) return 0;
    if (string->id>strings->maxid) strings->maxid=string->id;
  } else if ((src[0]>='0')&&(src[0]<='9')) {
    return 0;
  } else {
    // We could assert that it's a C identifier.
    // Right now we only know it has no spaces and doesn't start with a digit.
    // But whatever.
    string->id=0;
    string->name=src;
    string->namec=srcc;
  }
  return string;
}

/* Set content.
 */
 
static int string_set_json(struct string *string,const char *src,int srcc) {
  int dsta=srcc+1; // Decoding JSON string can only make it smaller.
  char *dst=malloc(dsta);
  if (!dst) return -1;
  int dstc=sr_string_eval(dst,dsta,src,srcc);
  if ((dstc<0)||(dstc>=dsta)) {
    free(dst);
    return -1;
  }
  string->v=dst;
  string->c=dstc;
  return 0;
}

static int string_set_verbatim(struct string *string,const char *src,int srcc) {
  if (!(string->v=malloc(srcc+1))) return -1;
  memcpy(string->v,src,srcc);
  string->v[srcc]=0;
  string->c=srcc;
  return 0;
}

/* If a name has been used by some other resource (outside this file), reuse it.
 */
 
static int strings_lookup_name(const char *name,int namec) {
  const struct arw_res *res=archive.arw.resv;
  int i=archive.arw.resc;
  for (;i-->0;res++) {
    if (res->format!=ARCHIVE_FORMAT_string) continue;
    if (res->namec!=namec) continue;
    if (memcmp(res->name,name,namec)) continue;
    int id=0,p=res->pathc,coef=1; // path must end "/ID"
    while (p-->0) {
      char ch=res->path[p];
      if (ch=='/') break;
      if ((ch<'0')||(ch>'9')) { id=0; break; }
      id+=(ch-'0')*coef;
      coef*=10;
    }
    if (id) return id;
  }
  return 0;
}

/* Get a numeric ID for every string and check for duplicates.
 */
 
static int strings_require_ids(struct strings *strings) {
  struct string *a=strings->v;
  int ai=0;
  for (;ai<strings->c;ai++,a++) {
    if (!a->id) {
      if (a->namec<1) { // shouldn't be possible
        fprintf(stderr,"%.*s:%d: Invalid ID\n",strings->res->pathc,strings->res->path,a->lineno);
        return -2;
      }
      const struct string *b=strings->v;
      int bi=0;
      for (;bi<ai;bi++,b++) {
        if ((b->namec==a->namec)&&!memcmp(b->name,a->name,a->namec)) {
          fprintf(stderr,
            "%.*s:%d: Duplicate string name '%.*s', also used on line %d\n",
            strings->res->pathc,strings->res->path,a->lineno,a->namec,a->name,b->lineno
          );
          return -2;
        }
      }
      if (a->id=strings_lookup_name(a->name,a->namec)) {
        // Some other file previously processed must have defined it, great, use the same id.
        if (a->id>strings->maxid) strings->maxid=a->id;
      } else {
        // Allocate a new ID.
        if (strings->maxid>=0xffff) {
          fprintf(stderr,"%.*s:%d: Exhausted string ID space.\n",strings->res->pathc,strings->res->path,a->lineno);
          return -2;
        }
        strings->maxid++;
        a->id=strings->maxid;
      }
    }
    { // Now that we have an ID, confirm we haven't seen it yet.
      const struct string *b=strings->v;
      int bi=0;
      for (;bi<ai;bi++,b++) {
        if (a->id==b->id) {
          fprintf(stderr,"%.*s:%d: Duplicate string ID %d, also used on line %d\n",strings->res->pathc,strings->res->path,a->lineno,a->id,b->lineno);
          return -2;
        }
      }
    }
  }
  return 0;
}

/* Process multi-resource strings file.
 * We will delete this resource and add smaller ones, one per string.
 */
 
int res_strings_process(struct arw_res *res) {
  if (res->namec!=2) return -1;
  struct strings strings={.res=res,0};
  struct sr_decoder decoder={.v=res->serial,.c=res->serialc};
  int lineno=0,linec;
  const char *line;
  while ((linec=sr_decode_line(&line,&decoder))>0) {
    lineno++;
    
    int linep=0;
    while ((linep<linec)&&((unsigned char)line[linep]<=0x20)) linep++;
    while (linec&&((unsigned char)line[linec-1]<=0x20)) linec--;
    if (linep>=linec) continue;
    if (line[linep]=='#') continue;
    const char *id=line+linep;
    int idc=0;
    while ((linep<linec)&&((unsigned char)line[linep++]>0x20)) idc++;
    while ((linep<linec)&&((unsigned char)line[linep]<=0x20)) linep++;
    
    struct string *string=strings_add(&strings,id,idc);
    if (!string) {
      strings_cleanup(&strings);
      fprintf(stderr,"%.*s:%d: Invalid string identifier '%.*s'\n",res->pathc,res->path,lineno,idc,id);
      return -2;
    }
    string->lineno=lineno;
    int err=-1;
    if ((linep<linec)&&(line[linep]=='"')) {
      err=string_set_json(string,line+linep,linec-linep);
    } else {
      err=string_set_verbatim(string,line+linep,linec-linep);
    }
    if (err<0) {
      strings_cleanup(&strings);
      fprintf(stderr,"%.*s:%d: Malformed string.\n",res->pathc,res->path,lineno);
      return -2;
    }
  }
  
  int err=strings_require_ids(&strings);
  if (err<0) {
    strings_cleanup(&strings);
    return err;
  }
  
  // Don't use (res) beyond this point; we're about to starting adding and removing things.
  const char *resname=res->name;
  const char *respath=res->path;
  int respathc=res->pathc;
  res=0;
  {
    struct string *string=strings.v;
    int i=strings.c;
    for (;i-->0;string++) {
      char path[32];
      int pathc=snprintf(path,sizeof(path),"s/%.2s/%d",resname,string->id);
      struct arw_res *child=arw_add_handoff(&archive.arw,path,pathc,string->v,string->c);
      if (!child) {
        strings_cleanup(&strings);
        return -1;
      }
      string->v=0; // HANDOFF to arw
      string->c=0;
      child->format=ARCHIVE_FORMAT_string;
      if (string->namec) arw_res_set_name(child,string->name,string->namec);
    }
  }
  
  strings_cleanup(&strings);
  arw_remove(&archive.arw,respath,respathc);
  return 0;
}
