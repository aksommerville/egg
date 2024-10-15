#include "eggdev_internal.h"
#include "eggdev_html.h"
#include "opt/synth/synth_formats.h"

/* Cleanup.
 */

void eggdev_res_cleanup(struct eggdev_res *res) {
  if (res->name) free(res->name);
  if (res->comment) free(res->comment);
  if (res->format) free(res->format);
  if (res->path) free(res->path);
  if (res->serial) free(res->serial);
}

void eggdev_rom_cleanup(struct eggdev_rom *rom) {
  if (rom->resv) {
    while (rom->resc-->0) eggdev_res_cleanup(rom->resv+rom->resc);
    free(rom->resv);
  }
  if (rom->tnamev) {
    while (rom->tnamec-->0) {
      if (rom->tnamev[rom->tnamec]) free(rom->tnamev[rom->tnamec]);
    }
    free(rom->tnamev);
  }
  if (rom->instrumentv) {
    while (rom->instrumentc-->0) free(rom->instrumentv[rom->instrumentc].v);
    free(rom->instrumentv);
  }
  memset(rom,0,sizeof(struct eggdev_rom));
}

/* Add path.
 */

int eggdev_rom_add_path(struct eggdev_rom *rom,const char *path) {
  if (!rom||!path) return -1;
  rom->seq++;
  char ftype=file_get_type(path);
  if (!ftype) {
    fprintf(stderr,"%s: File not found or can't stat.\n",path);
    return -2;
  }
  if (ftype=='d') return eggdev_rom_add_directory(rom,path);
  if (ftype!='f') {
    fprintf(stderr,"%s:WARNING: Ignoring file due to unexpected type '%c'\n",path,ftype);
    return 0;
  }
  /* Regular files are a little tricky.
   * It must be one of: ROM, HTML, Executable, Resource.
   *  - If path ends ".html" or ".htm", assume HTML.
   *  - If path ends ".exe" assume Executable.
   *  - If path ends ".egg" assume ROM.
   *  - If we get a valid tid and rid by analyzing the path, assume Resource.
   *  - And finally call it Executable.
   * It's worth noting that Executable will also successfully decode ROM files.
   */
  char sfx[16];
  const char *sfxsrc=0;
  int sfxsrcc=0,pathp=0;
  for (;path[pathp];pathp++) {
    if (path[pathp]=='/') {
      sfxsrc=0;
      sfxsrcc=0;
    } else if (path[pathp]=='.') {
      sfxsrc=path+pathp+1;
      sfxsrcc=0;
    } else if (sfxsrc) {
      sfxsrcc++;
    }
  }
  if ((sfxsrcc>0)&&(sfxsrcc<=sizeof(sfx))) {
    int i=sfxsrcc;
    while (i-->0) {
      if ((sfxsrc[i]>='A')&&(sfxsrc[i]<='Z')) sfx[i]=sfxsrc[i]+0x20;
      else sfx[i]=sfxsrc[i];
    }
    if ((sfxsrcc==4)&&!memcmp(sfx,"html",4)) return eggdev_rom_add_html(rom,path);
    if ((sfxsrcc==3)&&!memcmp(sfx,"htm",3)) return eggdev_rom_add_html(rom,path);
    if ((sfxsrcc==3)&&!memcmp(sfx,"exe",3)) return eggdev_rom_add_executable(rom,path);
    if ((sfxsrcc==3)&&!memcmp(sfx,"egg",3)) return eggdev_rom_add_rom(rom,path);
  }
  struct eggdev_path parsed={0};
  if (eggdev_rom_parse_path(&parsed,rom,path)>=0) {
    if (parsed.tid&&parsed.rid) return eggdev_rom_add_file(rom,path);
  }
  return eggdev_rom_add_executable(rom,path);
}

/* Type names.
 * ROM object is optional to both of these.
 */
 
static char eggdev_tid_storage[256];
static int eggdev_tid_storagep=0;
 
const char *eggdev_tid_repr(const struct eggdev_rom *rom,int tid) {
  if ((tid<0)||(tid>0xff)) return "?";
  switch (tid) {
    #define _(tag) case EGG_TID_##tag: return #tag;
    EGG_TID_FOR_EACH
    #undef _
  }
  if (rom&&(tid<rom->tnamec)&&rom->tnamev[tid]) return rom->tnamev[tid];
  if (eggdev_tid_storagep>=sizeof(eggdev_tid_storage)-4) eggdev_tid_storagep=0;
  char *dst=eggdev_tid_storage+eggdev_tid_storagep;
  eggdev_tid_storagep+=sr_decsint_repr(dst,sizeof(eggdev_tid_storage)-eggdev_tid_storagep,tid);
  return dst;
}

int eggdev_tid_eval(const struct eggdev_rom *rom,const char *src,int srcc) {
  if (!src) return 0;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  #define _(tag) if ((srcc==sizeof(#tag)-1)&&!memcmp(src,#tag,srcc)) return EGG_TID_##tag;
  EGG_TID_FOR_EACH
  #undef _
  if (rom) {
    int i=rom->tnamec;
    while (i-->0) {
      const char *q=rom->tnamev[i];
      if (!q) continue;
      if (memcmp(q,src,srcc)) continue;
      if (q[srcc]) continue;
      return i;
    }
  }
  int tid;
  if ((sr_int_eval(&tid,src,srcc)>=2)&&(tid>0)&&(tid<=0xff)) return tid;
  return 0;
}

/* Define custom type.
 */
 
static int eggdev_rom_name_type(struct eggdev_rom *rom,int tid,const char *src,int srcc) {
  if ((tid<0)||(tid>0xff)) return -1;
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  if (tid>=rom->tnamea) {
    int na=((tid+16)&~15);
    void *nv=realloc(rom->tnamev,sizeof(void*)*na);
    if (!nv) return -1;
    rom->tnamev=nv;
    rom->tnamea=na;
  }
  if (!srcc) {
    if (tid>=rom->tnamec) return 0;
    if (!rom->tnamev[tid]) return 0;
    free(rom->tnamev[tid]);
    rom->tnamev[tid]=0;
    return 0;
  }
  if ((tid<rom->tnamec)&&rom->tnamev[tid]&&!memcmp(src,rom->tnamev[tid],srcc)&&!rom->tnamev[tid][srcc]) return 0;
  while (rom->tnamec<=tid) rom->tnamev[rom->tnamec++]=0;
  char *nv=malloc(srcc+1);
  if (!nv) return -1;
  memcpy(nv,src,srcc);
  nv[srcc]=0;
  if (rom->tnamev[tid]) free(rom->tnamev[tid]);
  rom->tnamev[tid]=nv;
  return 0;
}

/* Add manifest file.
 */
 
static int eggdev_rom_add_manifest_line(struct eggdev_rom *rom,const char *src,int srcc,const char *path,int lineno) {
  int srcp=0;
  while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
  if (srcp>=srcc) return 0;
  const char *kw=src+srcp;
  int kwc=0;
  while ((srcp<srcc)&&((unsigned char)src[srcp++]>0x20)) kwc++;
  while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
  
  if ((kwc==4)&&!memcmp(kw,"type",4)) {
    const char *tname=src+srcp;
    int tnamec=0;
    while ((srcp<srcc)&&((unsigned char)src[srcp++]>0x20)) tnamec++;
    while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
    const char *tidsrc=src+srcp;
    int tidsrcc=srcc-srcp;
    if (!tnamec||!tidsrcc) {
      fprintf(stderr,"%s:%d: Malformed manifest type line, expected 'type NAME ID'\n",path,lineno);
      return -2;
    }
    int tid;
    if ((sr_int_eval(&tid,tidsrc,tidsrcc)<2)||(tid<16)||(tid>127)) {
      fprintf(stderr,"%s:%d: Illegal type id '%.*s', expected integer in 16..127\n",path,lineno,tidsrcc,tidsrc);
      return -2;
    }
    if (eggdev_rom_name_type(rom,tid,tname,tnamec)<0) {
      if ((tid<rom->tnamec)&&rom->tnamev[tid]) {
        fprintf(stderr,"%s:%d: Failed to assign name '%.*s' to tid %d, already defined as '%s'\n",path,lineno,tnamec,tname,tid,rom->tnamev[tid]);
      } else {
        fprintf(stderr,"%s:%d: Failed to assign name '%.*s' to tid %d, reason unknown\n",path,lineno,tnamec,tname,tid);
      }
      return -2;
    }
    return 0;
  }
  
  fprintf(stderr,"%s:%d:WARNING: Ignoring unexpected command '%.*s' in ROM manifest\n",path,lineno,kwc,kw);
  return 0;
}
 
static int eggdev_rom_add_manifest_text(struct eggdev_rom *rom,const char *src,int srcc,const char *path) {
  struct sr_decoder decoder={.v=src,.c=srcc};
  const char *line;
  int linec,lineno=1,err;
  for (;(linec=sr_decode_line(&line,&decoder))>0;lineno++) {
    while (linec&&((unsigned char)line[linec-1]<=0x20)) linec--;
    while (linec&&((unsigned char)line[0]<=0x20)) { linec--; line++; }
    if (!linec||(line[0]=='#')) continue;
    if ((err=eggdev_rom_add_manifest_line(rom,line,linec,path,lineno))<0) {
      if (err!=-2) fprintf(stderr,"%s:%d: Unspecified error processing ROM manifest\n",path,lineno);
      return -2;
    }
  }
  return 0;
}
 
int eggdev_rom_add_manifest_file(struct eggdev_rom *rom,const char *path) {
  char *src=0;
  int srcc=0;
  if ((srcc=file_read(&src,path))<0) {
    // We attempt to read it without knowing whether it exists.
    // If the read fails, assume there is no manifest and quietly move along.
    return 0;
  }
  int err=eggdev_rom_add_manifest_text(rom,src,srcc,path);
  free(src);
  if (err>=0) return 0;
  if (err!=-2) fprintf(stderr,"%s: Unspecified error processing ROM manifest\n",path);
  return -2;
}

/* Add directory: Callback for second level, expect single-resource files.
 */
 
static int eggdev_rom_add_directory_cb_bottom(const char *path,const char *base,char ftype,void *userdata) {
  struct eggdev_rom *rom=userdata;
  if (!ftype) ftype=file_get_type(path);
  if (ftype!='f') {
    fprintf(stderr,"%s:WARNING: Ignoring file in resources due to unexpected type '%c'\n",path,ftype);
    return 0;
  }
  return eggdev_rom_add_file(rom,path);
}

/* Add directory: Callback for top level, expect type directories.
 */
 
static int eggdev_rom_add_directory_cb_top(const char *path,const char *base,char ftype,void *userdata) {
  struct eggdev_rom *rom=userdata;
  if (!ftype) ftype=file_get_type(path);
  if (ftype=='d') return dir_read(path,eggdev_rom_add_directory_cb_bottom,rom);
  if (ftype=='f') {
    if (!strcmp(base,"manifest")) return 0;
    if (!strcmp(base,"instruments")) return 0;
    // Should be "metadata" or "code.wasm", whatever, handle it generically:
    return eggdev_rom_add_file(rom,path);
  }
  return 0;
}

/* Add directory.
 */

int eggdev_rom_add_directory(struct eggdev_rom *rom,const char *path) {
  int err;
  char mfpath[1024];
  int mfpathc=snprintf(mfpath,sizeof(mfpath),"%s/manifest",path);
  if ((mfpathc>0)&&(mfpathc<sizeof(mfpath))) {
    if ((err=eggdev_rom_add_manifest_file(rom,mfpath))<0) {
      if (err!=-2) fprintf(stderr,"%s: Unspecified error applying ROM manifest\n",mfpath);
      return -2;
    }
  }
  return dir_read(path,eggdev_rom_add_directory_cb_top,rom);
}

/* Decode ROM file in memory.
 */
 
int eggdev_rom_add_rom_serial(struct eggdev_rom *rom,const void *src,int srcc,const char *path) {
  struct rom_reader reader;
  if (rom_reader_init(&reader,src,srcc)<0) {
    fprintf(stderr,"%s: Not an Egg ROM, signature mismatch\n",path);
    return -2;
  }
  struct rom_res *kres;
  while (kres=rom_reader_next(&reader)) {
    struct eggdev_res *res;
    int p=eggdev_rom_search(rom,kres->tid,kres->rid);
    if (p<0) {
      p=-p-1;
      if (!(res=eggdev_rom_insert(rom,p,kres->tid,kres->rid))) return -1;
    } else {
      res=rom->resv+p;
      if (res->seq==rom->seq) {
        fprintf(stderr,"%s: Duplicate resource %s:%d\n",path,eggdev_tid_repr(rom,res->tid),res->rid);
        return -2;
      }
      eggdev_res_set_name(res,0,0);
      eggdev_res_set_comment(res,0,0);
      eggdev_res_set_format(res,0,0);
      eggdev_res_set_path(res,0,0);
    }
    if (eggdev_res_set_serial(res,kres->v,kres->c)<0) return -1;
  }
  if (reader.status<0) {
    fprintf(stderr,"%s: Malformed ROM\n",path);
    return -2;
  }
  return 0;
}

/* Decode and add ROM file.
 */
  
int eggdev_rom_add_rom(struct eggdev_rom *rom,const char *path) {
  void *src=0;
  int srcc=file_read(&src,path);
  if (srcc<0) {
    fprintf(stderr,"%s: Failed to read file\n",path);
    return -2;
  }
  rom->totalsize+=srcc;
  int err=eggdev_rom_add_rom_serial(rom,src,srcc,path);
  free(src);
  if (err>=0) return 0;
  if (err!=-2) fprintf(stderr,"%s: Unspecified error decoding %d-byte ROM file\n",path,srcc);
  return -2;
}

/* Locate ROM in executable or whatever.
 */
 
int eggdev_locate_rom(void *dstpp,const void *src,int srcc) {
  const uint8_t *SRC=src;
  int srcp=0,stopp=srcc-4;
  while (srcp<=stopp) {
    struct rom_reader reader;
    if (rom_reader_init(&reader,SRC+srcp,srcc-srcp)<0) {
      srcp++;
      continue;
    }
    struct rom_res *kres;
    int resc=0;
    while (kres=rom_reader_next(&reader)) {
      resc++;
    }
    if (resc&&(reader.status>0)) {
      *(const void**)dstpp=SRC+srcp;
      return reader.srcp;
    }
    srcp++;
  }
  return -1;
}

/* Locate ROM in executable and add it.
 * We use brute force for this: Scan the whole file looking for the ROM's signature.
 */
 
int eggdev_rom_add_executable(struct eggdev_rom *rom,const char *path) {
  void *src=0;
  int srcc=file_read(&src,path);
  if (srcc<0) {
    fprintf(stderr,"%s: Failed to read file\n",path);
    return -2;
  }
  rom->totalsize+=srcc;
  const void *sub=0;
  int subc=eggdev_locate_rom(&sub,src,srcc);
  if (subc<0) {
    fprintf(stderr,"%s: Failed to locate Egg ROM in executable.\n",path);
    free(src);
    return -2;
  }
  int err=eggdev_rom_add_rom_serial(rom,sub,subc,path);
  free(src);
  if (err>=0) return 0;
  if (err!=-2) fprintf(stderr,"%s: Unspecified error decoding %d-byte embedded ROM\n",path,subc);
  return -2;
}

/* Decode ROM from HTML text and add it.
 */
 
int eggdev_rom_add_html_text(struct eggdev_rom *rom,const char *src,int srcc,const char *path) {
  struct eggdev_html_reader reader;
  eggdev_html_reader_init(&reader,src,srcc,path);
  const char *expr,*b64=0;
  int exprc,b64c=0;
  while ((exprc=eggdev_html_reader_next(&expr,&reader))>0) {
    switch (eggdev_html_expression_type(expr,exprc)) {
      case EGGDEV_HTML_EXPR_OPEN: {
          const char *name;
          int namec=eggdev_html_get_tag_name(&name,expr,exprc);
          if ((namec==7)&&!memcmp(name,"egg-rom",7)) {
            if (b64) {
              fprintf(stderr,"%s:%d: Multiple <egg-rom> tags.\n",path,eggdev_lineno(src,reader.srcp));
              return -2;
            }
            if ((b64c=eggdev_html_close(&b64,&reader))<0) {
              if (b64c!=-2) fprintf(stderr,"%s:%d: Unspecified error reading <egg-rom> tag\n",path,eggdev_lineno(src,reader.srcp));
              return -2;
            }
          }
        } break;
    }
  }
  if (!b64) {
    fprintf(stderr,"%s: No <egg-rom> tag\n",path);
    return -2;
  }
  if (b64c>0x10000000) {
    fprintf(stderr,"%s: Unreasonably long ROM text (%d bytes)\n",path,b64c);
    return -2;
  }
  int seriala=(b64c*3+3)/4;
  void *serial=malloc(seriala);
  if (!serial) return -1;
  int serialc=sr_base64_decode(serial,seriala,b64,b64c);
  if ((serialc<0)||(serialc>seriala)) {
    fprintf(stderr,"%s: Failed to decode <egg-rom> tag base64, %d bytes\n",path,b64c);
    free(serial);
    return -2;
  }
  int err=eggdev_rom_add_rom_serial(rom,serial,serialc,path);
  free(serial);
  return err;
}

/* Decode ROM from HTML file and add it.
 */
 
int eggdev_rom_add_html(struct eggdev_rom *rom,const char *path) {
  char *html=0;
  int htmlc=file_read(&html,path);
  if (htmlc<0) {
    fprintf(stderr,"%s: Failed to read file\n",path);
    return -2;
  }
  rom->totalsize+=htmlc;
  int err=eggdev_rom_add_html_text(rom,html,htmlc,path);
  free(html);
  if (err>=0) return 0;
  if (err!=-2) fprintf(stderr,"%s: Unspecified error extracting resources from HTML\n",path);
  return -2;
}

/* Add a single-resource file.
 */
 
int eggdev_rom_add_file(struct eggdev_rom *rom,const char *path) {
  struct eggdev_path parsed={0};
  int err=eggdev_rom_parse_path(&parsed,rom,path);
  if (err<0) {
    if (err!=-2) fprintf(stderr,"%s: Invalid resource path.\n",path);
    return -2;
  }
  struct eggdev_res *res=0;
  int p=eggdev_rom_search(rom,parsed.tid,parsed.rid);
  if (p<0) {
    p=-p-1;
    if (!(res=eggdev_rom_insert(rom,p,parsed.tid,parsed.rid))) return -1;
  } else {
    res=rom->resv+p;
    if (res->seq==rom->seq) {
      fprintf(stderr,"%s: Duplicate resource ID %d:%d (%s)\n",path,parsed.tid,parsed.rid,res->path);
      return -2;
    }
  }
  res->lang=parsed.lang;
  if (
    (eggdev_res_set_path(res,path,-1)<0)||
    (eggdev_res_set_name(res,parsed.name,parsed.namec)<0)||
    (eggdev_res_set_comment(res,parsed.comment,parsed.commentc)<0)||
    (eggdev_res_set_format(res,parsed.format,parsed.formatc)<0)
  ) return -1;
  void *serial=0;
  int serialc=file_read(&serial,path);
  if (serialc<0) {
    fprintf(stderr,"%s: Failed to read file\n",path);
    return -2;
  }
  rom->totalsize+=serialc;
  eggdev_res_handoff_serial(res,serial,serialc);
  return 0;
}

/* Parse path.
 */

int eggdev_rom_parse_path(
  struct eggdev_path *parsed,
  const struct eggdev_rom *rom,
  const char *path
) {
  if (!path) return -1;
  memset(parsed,0,sizeof(struct eggdev_path));

  // Extract the last two path components.
  const char *base=path,*dir=0;
  int basec=0,dirc=0,pathc=0;
  for (;path[pathc];pathc++) {
    if (path[pathc]=='/') {
      dir=base;
      dirc=basec;
      base=path+pathc+1;
      basec=0;
    } else basec++;
  }
  
  // Pick off a few special files.
  if ((basec==8)&&!memcmp(base,"metadata",8)) {
    parsed->tid=EGG_TID_metadata;
    parsed->rid=1;
    parsed->tname="metadata";
    parsed->tnamec=8;
    return 0;
  }
  if ((basec==9)&&!memcmp(base,"code.wasm",9)) {
    parsed->tid=EGG_TID_code;
    parsed->rid=1;
    parsed->tname="code";
    parsed->tnamec=4;
    return 0;
  }
  
  // Directory must name a type.
  if ((dir[0]>='0')&&(dir[0]<='9')) {
    int sepp=-1,i=0;
    for (;i<dirc;i++) {
      if (dir[i]=='-') {
        sepp=i;
        break;
      }
    }
    if (sepp<0) return -1;
    int v;
    if ((sr_int_eval(&v,dir,sepp)<2)||(v<1)||(v>0xff)) return -1;
    parsed->tid=v;
    parsed->tname=dir+sepp+1;
    parsed->tnamec=dirc-sepp-1;
  } else {
    if ((parsed->tid=eggdev_tid_eval(rom,dir,dirc))<1) return -1;
    parsed->tname=dir;
    parsed->tnamec=dirc;
  }
  
  // Basename may begin with language code. strings are expected to use this, and no other type, but any can.
  int basep=0,lang=0;
  if ((basec>=3)&&(base[0]>='a')&&(base[0]<='z')&&(base[1]>='a')&&(base[1]<='z')&&(base[2]=='-')) {
    lang=EGG_LANG_FROM_STRING(base);
    parsed->lang=lang;
    basep+=3;
  }
  
  // Next thing in base must be rid.
  if ((basep>=basec)||(base[basep]<'0')||(base[basep]>'9')) return -1;
  while ((basep<basec)&&(base[basep]>='0')&&(base[basep]<='9')) {
    parsed->rid*=10;
    parsed->rid+=base[basep++]-'0';
    if (parsed->rid>0xffff) return -1;
  }
  if (!parsed->rid) return -1;
  if (lang) {
    if (parsed->rid&~0x3f) {
      fprintf(stderr,"%s: Resource with language qualifier must have ID in 1..63\n",path);
      return -2;
    }
    parsed->rid|=lang<<6;
  }
  
  // If there's a dash, it's followed by name.
  if ((basep<basec)&&(base[basep]=='-')) {
    basep++;
    parsed->name=base+basep;
    while ((basep<basec)&&(base[basep]!='.')) {
      basep++;
      parsed->namec++;
    }
  }
  
  // Then dots introduce comment and format, extending to the end.
  if ((basep<basec)&&(base[basep]=='.')) {
    parsed->format=base+basep+1;
    parsed->formatc=basec-basep-1;
    basep=basec;
    int i=parsed->formatc;
    while (i-->0) {
      if (parsed->format[i]=='.') {
        parsed->comment=parsed->format;
        parsed->commentc=i;
        parsed->format+=i+1;
        parsed->formatc=parsed->formatc-i-1;
        break;
      }
    }
  }
  
  if (basep<basec) return -1;
  return 0;
}

/* Synthesize basename.
 */
 
int eggdev_synthesize_basename(char *dst,int dsta,const struct eggdev_res *res) {
  int dstc=0;
  
  /* If the type is 'strings', and ID is shaped right, emit language first.
   */
  int rid=res->rid;
  if (res->tid==EGG_TID_strings) {
    int a=(res->rid>>11)+'a';
    int b=((res->rid>>6)&0x1f)+'a';
    int c=(res->rid&0x3f);
    if ((a>='a')&&(a<='z')&&(b>='a')&&(b<='z')&&(c>0)) {
      if (dstc<=dsta-3) {
        dst[dstc++]=a;
        dst[dstc++]=b;
        dst[dstc++]='-';
      } else dstc+=3;
      rid=c;
    }
  }
  
  // rid
  dstc+=sr_decsint_repr(dst+dstc,dsta-dstc,rid);
  
  // name if present.
  if (res->namec) {
    if (dstc<dsta) dst[dstc]='-';
    dstc++;
    if (dstc<=dsta-res->namec) memcpy(dst+dstc,res->name,res->namec);
    dstc+=res->namec;
  }
  
  // comment if both comment and format are present (without a format, we must drop the comment).
  if (res->commentc&&res->formatc) {
    if (dstc<dsta) dst[dstc]='.';
    dstc++;
    if (dstc<=dsta-res->commentc) memcpy(dst+dstc,res->comment,res->commentc);
    dstc+=res->commentc;
  }
  
  // format
  if (res->formatc) {
    if (dstc<dsta) dst[dstc]='.';
    dstc++;
    if (dstc<=dsta-res->formatc) memcpy(dst+dstc,res->format,res->formatc);
    dstc+=res->formatc;
  }
  
  if (dstc<dsta) dst[dstc]=0;
  return dstc;
}

/* Search.
 */

int eggdev_rom_search(const struct eggdev_rom *rom,int tid,int rid) {
  int lo=0,hi=rom->resc;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
    const struct eggdev_res *q=rom->resv+ck;
         if (tid<q->tid) hi=ck;
    else if (tid>q->tid) lo=ck+1;
    else if (rid<q->rid) hi=ck;
    else if (rid>q->rid) lo=ck+1;
    else return ck;
  }
  return -lo-1;
}

/* Insert.
 */
 
struct eggdev_res *eggdev_rom_insert(struct eggdev_rom *rom,int p,int tid,int rid) {
  if ((p<0)||(p>rom->resc)) return 0;
  if ((tid<1)||(tid>0xff)) return 0;
  if (rid<1) return 0; // Allow too-high rids. sounds uses them, temporarily.
  if (p) {
    const struct eggdev_res *q=rom->resv+p-1;
    if (tid<q->tid) return 0;
    if ((tid==q->tid)&&(rid<=q->rid)) return 0;
  }
  if (p<rom->resc) {
    const struct eggdev_res *q=rom->resv+p;
    if (tid>q->tid) return 0;
    if ((tid==q->tid)&&(rid>=q->rid)) return 0;
  }
  if (rom->resc>=rom->resa) {
    int na=rom->resa+256;
    if (na>INT_MAX/sizeof(struct eggdev_res)) return 0;
    void *nv=realloc(rom->resv,sizeof(struct eggdev_res)*na);
    if (!nv) return 0;
    rom->resv=nv;
    rom->resa=na;
  }
  struct eggdev_res *res=rom->resv+p;
  memmove(res+1,res,sizeof(struct eggdev_res)*(rom->resc-p));
  rom->resc++;
  memset(res,0,sizeof(struct eggdev_res));
  res->tid=tid;
  res->rid=rid;
  res->seq=rom->seq;
  return res;
}

/* Set strings in resource.
 */
 
#define SETONE(tag) \
  int eggdev_res_set_##tag(struct eggdev_res *res,const char *src,int srcc) { \
    if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; } \
    char *nv=0; \
    if (srcc) { \
      if (!(nv=malloc(srcc+1))) return -1; \
      memcpy(nv,src,srcc); \
      nv[srcc]=0; \
    } \
    if (res->tag) free(res->tag); \
    res->tag=nv; \
    res->tag##c=srcc; \
    return 0; \
  }
SETONE(name)
SETONE(comment)
SETONE(format)
SETONE(path)
#undef SETONE

/* Set serial in resource.
 */
 
int eggdev_res_set_serial(struct eggdev_res *res,const void *src,int srcc) {
  if ((srcc<0)||(srcc&&!src)) return -1;
  void *nv=0;
  if (srcc) {
    if (!(nv=malloc(srcc))) return -1;
    memcpy(nv,src,srcc);
  }
  if (res->serial) free(res->serial);
  res->serial=nv;
  res->serialc=srcc;
  return 0;
}

void eggdev_res_handoff_serial(struct eggdev_res *res,void *src,int srcc) {
  if ((srcc<0)||(srcc&&!src)) srcc=0;
  if (res->serial) free(res->serial);
  res->serial=src;
  res->serialc=srcc;
}

/* Test resource comment.
 */
 
int eggdev_res_has_comment(const struct eggdev_res *res,const char *token,int tokenc) {
  if (!token) return 0;
  if (tokenc<0) { tokenc=0; while (token[tokenc]) tokenc++; }
  if (!tokenc) return 0;
  const char *src=res->comment;
  int srcc=res->commentc;
  int srcp=0;
  while (srcp<srcc) {
    if ((unsigned char)src[srcp]<=0x20) { srcp++; continue; }
    if (src[srcp]=='.') { srcp++; continue; }
    const char *q=src+srcp;
    int qc=0;
    while ((srcp<srcc)&&(src[srcp]!='.')&&((unsigned char)src[srcp]>0x20)) { srcp++; qc++; }
    if ((qc==tokenc)&&!memcmp(q,token,tokenc)) return 1;
  }
  return 0;
}

/* Instrument definitions.
 */
 
static int eggdev_rom_search_instruments(const struct eggdev_rom *rom,int fqpid) {
  int lo=0,hi=rom->instrumentc;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
    int q=rom->instrumentv[ck].fqpid;
         if (fqpid<q) hi=ck;
    else if (fqpid>q) lo=ck+1;
    else return ck;
  }
  return -lo-1;
}

static int eggdev_rom_handoff_instrument(struct eggdev_rom *rom,int fqpid,void *v,int c) {
  int p=eggdev_rom_search_instruments(rom,fqpid);
  if (p>=0) return -1;
  p=-p-1;
  if (rom->instrumentc>=rom->instrumenta) {
    int na=rom->instrumenta+32;
    if (na>INT_MAX/sizeof(struct eggdev_instrument)) return -1;
    void *nv=realloc(rom->instrumentv,sizeof(struct eggdev_instrument)*na);
    if (!nv) return -1;
    rom->instrumentv=nv;
    rom->instrumenta=na;
  }
  struct eggdev_instrument *ins=rom->instrumentv+p;
  memmove(ins+1,ins,sizeof(struct eggdev_instrument)*(rom->instrumentc-p));
  rom->instrumentc++;
  ins->fqpid=fqpid;
  ins->v=v; // HANDOFF
  ins->c=c;
  return 0;
}

static int eggdev_rom_parse_instruments(struct eggdev_rom *rom,const char *src,int srcc,const char *path) {
  struct synth_text_reader reader={.src=src,.srcc=srcc};
  int index,lineno,subsrcc;
  const char *subsrc;
  while ((subsrcc=synth_text_reader_next(&index,&lineno,&subsrc,&reader))>0) {
    struct sr_encoder dst={0};
    int err=synth_egs_from_text(&dst,subsrc,subsrcc,1,path,lineno);
    if (err<0) {
      sr_encoder_cleanup(&dst);
      return err;
    }
    if (dst.c) {
      if (eggdev_rom_handoff_instrument(rom,index,dst.v,dst.c)<0) {
        if (path) fprintf(stderr,"%s:%d: Failed to add instrument %d. Is it a duplicate?\n",path,lineno,index);
        sr_encoder_cleanup(&dst);
        return -1;
      }
    } else {
      sr_encoder_cleanup(&dst);
    }
  }
  return 0;
}
 
int eggdev_rom_require_instruments(struct eggdev_rom *rom) {
  if (rom->instrumenta) return 0;
  if (!(rom->instrumentv=malloc(sizeof(struct eggdev_instrument)*128))) return -1;
  rom->instrumenta=128;
  rom->instrumentc=0;
  
  // We're outside eggdev_rom_add_directory() here, so we don't have a concept of DATAROOT.
  // That's kind of stupid.
  // Borrow the path from metadata:1... hopefully it exists.
  int mp=eggdev_rom_search(rom,EGG_TID_metadata,1);
  if (mp<0) return 0;
  const char *mpath=rom->resv[mp].path;
  int mpathc=rom->resv[mp].pathc;
  while (mpathc&&(mpath[mpathc-1]!='/')) mpathc--;
  char path[1024];
  int pathc=snprintf(path,sizeof(path),"%.*sinstruments",mpathc,mpath);
  if ((pathc<1)||(pathc>=sizeof(path))) return 0;
  
  char *src=0;
  int srcc=file_read(&src,path);
  if (srcc<0) return 0;
  eggdev_rom_parse_instruments(rom,src,srcc,path);
  free(src);
  return 0;
}

int eggdev_rom_get_instrument(void *dstpp,struct eggdev_rom *rom,int fqpid) {
  if (eggdev_rom_require_instruments(rom)<0) return 0;
  int p=eggdev_rom_search_instruments(rom,fqpid);
  if (p<0) return 0;
  const struct eggdev_instrument *ins=rom->instrumentv+p;
  if (dstpp) *(const void**)dstpp=ins->v;
  return ins->c;
}

/* Validate.
 */

int eggdev_rom_validate(struct eggdev_rom *rom) {

  /* Drop zero-length resources.
   */
  int i=rom->resc;
  struct eggdev_res *res=rom->resv+i-1;
  for (;i-->0;res--) {
    if (res->serialc) continue;
    eggdev_res_cleanup(res);
    rom->resc--;
    memmove(res,res+1,sizeof(struct eggdev_res)*(rom->resc-i));
  }
  
  /* Validate order and per-resource fields.
   */
  int tid=1,rid=1;
  for (res=rom->resv,i=rom->resc;i-->0;res++) {
    if ((res->tid<tid)||((res->tid==tid)&&(res->rid<rid))) {
      fprintf(stderr,"ERROR: Duplicate or missorted resource IDs: (%d,%d)>=(%d,%d)\n",tid,rid,res->tid,res->rid);
      return -2;
    }
    tid=res->tid;
    rid=res->rid+1;
    if ((res->tid<1)||(res->tid>0xff)||(res->rid<1)||(res->rid>0xffff)) {
      fprintf(stderr,"ERROR: Invalid resource ID (%d,%d)\n",res->tid,res->rid);
      return -2;
    }
    if (res->serialc>4210688) {
      fprintf(stderr,"ERROR: Invalid resource size %d for %s:%d (%s)\n",res->serialc,eggdev_tid_repr(rom,res->tid),res->rid,res->path);
      return -2;
    }
    switch (res->tid) {
      case EGG_TID_metadata:
      case EGG_TID_code: {
          if (res->rid!=1) {
            fprintf(stderr,"ERROR: ID for '%s' resource can only be 1, found %d.\n",eggdev_tid_repr(rom,res->tid),res->rid);
            return -2;
          }
        } break;
    }
  }
  
  /* ROM must begin with metadata:1 (1:1).
   * If it's absent, create it.
   */
  if ((rom->resc<1)||(rom->resv->tid!=EGG_TID_metadata)||(rom->resv->rid!=1)) {
    if (!(res=eggdev_rom_insert(rom,0,EGG_TID_metadata,1))) return -1;
    if (eggdev_res_set_serial(res,"\0EM\xff\0",5)<0) return -1;
  }
  
  return 0;
}

/* Encode.
 */

int eggdev_rom_encode(struct sr_encoder *dst,const struct eggdev_rom *rom) {
  if (sr_encode_raw(dst,"\0EGG",4)<0) return -1;
  const struct eggdev_res *res=rom->resv;
  int i=rom->resc,tid=1,rid=1;
  for (;i-->0;res++) {
  
    // Skip empties.
    if (!res->serialc) continue;
  
    // Advance tid if required.
    if (res->tid>0xff) return -1;
    int d=res->tid-tid;
    if (d>0) {
      while (d>=0x3f) {
        if (sr_encode_u8(dst,0x3f)<0) return -1;
        d-=0x3f;
      }
      if (d&&(sr_encode_u8(dst,d)<0)) return -1;
      rid=1;
    } else if (d<0) return -1;
    tid=res->tid;
    
    // Advance rid if required.
    if (res->rid>0xffff) return -1;
    d=res->rid-rid;
    if (d>0) {
      while (d>=0x3fff) {
        if (sr_encode_intbe(dst,0x7fff,2)<0) return -1;
        d-=0x3fff;
      }
      if (d&&(sr_encode_intbe(dst,0x4000|d,2)<0)) return -1;
    } else if (d<0) return -1;
    rid=res->rid+1;
  
    // Emit resource.
    if (res->serialc>4210688) return -1;
    if (res->serialc>=16385) {
      if (sr_encode_intbe(dst,0xc00000|(res->serialc-16385),3)<0) return -1;
    } else {
      if (sr_encode_intbe(dst,0x8000|(res->serialc-1),2)<0) return -1;
    }
    if (sr_encode_raw(dst,res->serial,res->serialc)<0) return -1;
    
  }
  if (sr_encode_u8(dst,0x00)<0) return -1;
  return 0;
}
