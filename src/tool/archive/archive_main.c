#include "archive_internal.h"

struct archive archive={0};

static int archive_receive_dir(const char *path);

int res_details_process(struct arw_res *res);
int res_strings_check_path(const char *path,int pathc);
int res_strings_process(struct arw_res *res);
int res_image_process(struct arw_res *res);

/* If the path is standard form, return its type (a lowercase letter).
 */
 
static char archive_check_standard_form_path(const char *src,int srcc) {
  if (srcc<3) return 0;
  if (src[1]!='/') return 0;
  if ((src[0]<'a')||(src[0]>'z')) return 0;
  int i=2; for (;i<srcc;i++) {
    if ((src[i]<'0')||(src[i]>'9')) return 0;
  }
  return src[0];
}

/* Digest archive.
 * At this point, all input files are stored verbatim in (archive.arw).
 */
 
static int archive_digest() {
  struct arw_res *res;
  int i,err;

  /* Any processing that might add or remove resources must come first.
   */
  for (i=archive.arw.resc;i-->0;) {
    res=archive.arw.resv+i;
    
    if ((res->pathc==7)&&!memcmp(res->path,"details",7)) {
      if ((err=res_details_process(res))<0) return err;
      continue;
    }
    
    if (res_strings_check_path(res->path,res->pathc)) {
      if ((err=res_strings_process(res))<0) return err;
      continue;
    }
    
    // Add TOC-modifying processors here.
  }
  
  /* Any 1:1 compilation should happen now; the TOC is final.
   */
  for (res=archive.arw.resv,i=archive.arw.resc;i-->0;res++) {
  
    // An optimization for standard paths: lowercase letter, slash, decimal integer.
    char type=archive_check_standard_form_path(res->path,res->pathc);
    switch (type) {
      case 'i': if ((err=res_image_process(res))<0) return err; continue;
    }
  
    // Add normal 1:1 processors here.
  }
  
  /* Compilers that depend on their references being compiled first, can go here.
   * Try not to need this.
   */
  //TODO
  
  /* Let arw validate, sort, and whatever.
   */
  char msg[256]={0};
  if (arw_finalize(msg,sizeof(msg),&archive.arw)<0) {
    fprintf(stderr,"%s:ERROR: %s\n",archive.exename,msg);
    return -2;
  }
  if (msg[0]) {
    fprintf(stderr,"%s:WARNING: %s\n",archive.exename,msg);
  }
  
  return 0;
}

/* Format identifier from lowercase suffix.
 * Zero if unknown, and don't treat that as an error.
 *
 * Feel free to add anything here. Define in archive_internal.h too.
 */
 
static int archive_format_for_suffix(const char *src,int srcc) {
  switch (srcc) {
    #define _(tag) if (!memcmp(src,#tag,srcc)) return ARCHIVE_FORMAT_##tag;
    #define r(sfx,tag) if (!memcmp(src,#sfx,srcc)) return ARCHIVE_FORMAT_##tag;
    case 2: {
        _(js)
      } break;
    case 3: {
        _(png)
        _(qoi)
        _(gif)
        _(bmp)
        _(ico)
        r(jpg,jpeg)
        _(mid)
        _(txt)
        r(htm,html)
        _(wav)
      } break;
    case 4: {
        _(html)
        _(wasm)
        _(jpeg)
      } break;
    #undef _
    #undef r
  }
  return 0;
}

/* Digest path of regular file.
 */
 
struct archive_meta {
  const char *dstpath;
  int dstpathc;
  const char *name;
  int namec;
  int format; // from suffix
};

static int archive_meta_parse(struct archive_meta *meta,const char *path) {
  volatile int pathc=0; // I get a false `stringop-overread` on the memcmp below from gcc with -O3. Can dodge it by -O0 or, mysteriously, with "volatile" here.
  while (path[pathc]) pathc++;
  
  // Trim the root directory, then trim any leading slashes.
  if ((pathc>=archive.rootc)&&!memcmp(path,archive.root,archive.rootc)) {
    path+=archive.rootc;
    pathc-=archive.rootc;
  }
  while (pathc&&(path[0]=='/')) { path++; pathc--; }
  
  // Locate final separator.
  int sepp=-1;
  int i=pathc;
  while (i-->0) {
    if (path[i]=='/') {
      sepp=i;
      break;
    }
  }
  const char *base=path+sepp+1;
  int basec=pathc-sepp-1;
  
  // If the basename begins with digits, include those in dstpath, and trim one optional dash.
  // Otherwise include the entire basename.
  meta->dstpath=path;
  meta->dstpathc=sepp+1;
  if (basec&&(base[0]>='0')&&(base[0]<='9')) {
    while (basec&&(base[0]>='0')&&(base[0]<='9')) {
      meta->dstpathc++;
      base++;
      basec--;
    }
    if (basec&&(base[0]=='-')) { base++; basec--; }
  } else {
    meta->dstpathc+=basec;
  }
  
  // Everything until the first dot in (base) is (name).
  int basep=0;
  meta->name=base;
  meta->namec=0;
  while ((basep<basec)&&(base[basep]!='.')) { basep++; meta->namec++; }
  
  // If we're looking at a dot, check known suffixes after it and populate (format). Unknown is ok.
  meta->format=0;
  if ((basep<basec)&&(base[basep]=='.')) {
    basep++;
    char sfx[8];
    int sfxc=basec-basep;
    if (sfxc<=sizeof(sfx)) {
      int i=sfxc; while (i-->0) {
        sfx[i]=base[basep+i];
        if ((sfx[i]>='A')&&(sfx[i]<='Z')) sfx[i]+=0x20;
      }
      meta->format=archive_format_for_suffix(sfx,sfxc);
    }
  }
  
  return 0;
}

/* Receive regular file as input.
 */
 
static int archive_receive_file(const char *path) {
  void *serial=0;
  int serialc=file_read(&serial,path);
  if (serialc<0) {
    fprintf(stderr,"%s: Failed to read file.\n",path);
    return -2;
  }
  struct archive_meta meta={0};
  int err=archive_meta_parse(&meta,path);
  if (err<0) {
    if (err!=-2) fprintf(stderr,"%s: Failed to parse file path.\n",path);
    return -2;
  }
  struct arw_res *res=arw_add_handoff(&archive.arw,meta.dstpath,meta.dstpathc,serial,serialc);
  if (!res) {
    free(serial);
    return -1;
  }
  arw_res_set_name(res,meta.name,meta.namec);
  res->format=meta.format;
  return 0;
}

/* Receive directory as input.
 */
 
static int archive_cb_dir(const char *path,const char *base,char ftype,void *userdata) {
  if (!base[0]) return 0;
  if (base[0]=='.') return 0;
  if (!ftype) ftype=file_get_type(path);
  if (ftype=='f') return archive_receive_file(path);
  if (ftype=='d') return archive_receive_dir(path);
  return 0;
}
 
static int archive_receive_dir(const char *path) {
  return dir_read(path,archive_cb_dir,0);
}

/* Receive input file or directory.
 */
 
static int archive_receive_input(const char *path) {
  char ftype=file_get_type(path);
  if (ftype=='f') return archive_receive_file(path);
  if (ftype=='d') {
    archive.root=path;
    archive.rootc=0;
    while (archive.root[archive.rootc]) archive.rootc++;
    int err=archive_receive_dir(path);
    archive.rootc=0;
    return err;
  }
  fprintf(stderr,"%s: Unexpected file type.\n",path);
  return -2;
}

/* Encode output.
 */
 
static int archive_encode() {
  return arw_encode(&archive.dst,&archive.arw);
}

/* --help
 */
 
static void archive_print_help() {
  fprintf(stderr,"Usage: %s -oOUTPUT [INPUT...]\n",archive.exename);
  fprintf(stderr,"INPUT may be individual files, or a directories to walk recursively.\n");
  fprintf(stderr,"See etc/doc/archive-format.md in our source repo for more details.\n");
}

/* Main.
 */
 
int main(int argc,char **argv) {
  int err;

  if ((argc>=1)&&argv[0]&&argv[0][0]) archive.exename=argv[0];
  else archive.exename="archive";
  int argi=1; while (argi<argc) {
    const char *arg=argv[argi++];
    if (!arg||!arg[0]) continue;
    
    if (!strcmp(arg,"--help")) {
      archive_print_help();
      return 0;
    }
    
    if (!memcmp(arg,"-o",2)) {
      if (archive.dstpath) {
        fprintf(stderr,"%s: Multiple output paths.\n",archive.exename);
        return 1;
      }
      archive.dstpath=arg+2;
      continue;
    }
    
    if (arg[0]=='-') {
      fprintf(stderr,"%s: Unexpected option '%s'.\n",archive.exename,arg);
      return 1;
    }
    if ((err=archive_receive_input(arg))<0) {
      if (err!=-2) fprintf(stderr,"%s: Unspecified error processing input.\n",arg);
      return 1;
    }
  }
  if (!archive.dstpath) {
    archive_print_help();
    return 1;
  }
  
  if ((err=archive_digest())<0) {
    if (err!=-2) fprintf(stderr,"%s: Unspecified error digesting resource set.\n",archive.exename);
    return 1;
  }
  
  if ((err=archive_encode())<0) {
    if (err!=-2) fprintf(stderr,"%s: Unspecified error encoding output.\n",archive.dstpath);
    return 1;
  }
  
  if (file_write(archive.dstpath,archive.dst.v,archive.dst.c)<0) {
    fprintf(stderr,"%s: Failed to write file, %d bytes.\n",archive.dstpath,archive.dst.c);
    return 1;
  }

  return 0;
}
