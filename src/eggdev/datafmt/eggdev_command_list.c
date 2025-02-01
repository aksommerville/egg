#include "eggdev/eggdev_internal.h"

/* Compile one argument.
 */
 
static int eggdev_command_arg_compile(
  struct sr_encoder *dst,
  const char *src,int srcc,
  const char *refname,int lineno,
  const struct eggdev_ns_entry *entry,
  int argp
) {
  if (srcc<1) return 0;
  
  /* JSON strings are legal, but because they tokenize differently, they won't land here.
   */

  /* "0x" begins a hex dump of arbitrary length.
   * (not limited to integer sizes).
   */
  if ((srcc>=2)&&(!memcmp(src,"0x",2)||!memcmp(src,"0X",2))) {
    if (srcc&1) {
      fprintf(stderr,"%s:%d: Hexadecimal integer must have even length, found %d.\n",refname,lineno,srcc);
      return -2;
    }
    int srcp=2;
    while (srcp<srcc) {
      int hi=sr_digit_eval(src[srcp++]);
      int lo=sr_digit_eval(src[srcp++]);
      if ((hi<0)||(hi>0xf)||(lo<0)||(lo>0xf)) {
        fprintf(stderr,"%s:%d: Expected hexadecimal byte, found '%.2s'\n",refname,lineno,src+srcp-2);
        return -2;
      }
      if (sr_encode_u8(dst,(hi<<4)|lo)<0) return -1;
    }
    return 0;
  }
  
  /* "@..." begins a comma-delimited list of single-byte integers.
   */
  if (src[0]=='@') {
    int srcp=1;
    while (srcp<srcc) {
      const char *token=src+srcp;
      int tokenc=0,v;
      while ((srcp<srcc)&&(src[srcp++]!=',')) tokenc++;
      if ((sr_int_eval(&v,token,tokenc)<2)||(v<0)||(v>0xff)) {
        fprintf(stderr,"%s:%d: Expected integer in 0..255, found '%.*s'\n",refname,lineno,tokenc,token);
        return -2;
      }
      if (sr_encode_u8(dst,v)<0) return -1;
    }
    return 0;
  }
  
  /* "(bSIZE:NAMESPACE)(TOKEN,TOKEN...)" for bitmaps.
   */
  if ((srcc>=2)&&(src[0]=='(')&&(src[1]=='b')) {
    int srcp=2,size=0;
    while ((srcp<srcc)&&(src[srcp]!=':')) {
      char ch=src[srcp++];
      if ((ch<'0')||(ch>'9')) {
        fprintf(stderr,"%s:%d: Expected ':NAMESPACE' after bitmap size.\n",refname,lineno);
        return -2;
      }
      size*=10;
      size+=ch-'0';
      if (size>999) {
        fprintf(stderr,"%s:%d: Unreasonable bitmap size.\n",refname,lineno);
        return -2;
      }
    }
    if ((srcp>=srcc)||(src[srcp++]!=':')) {
      fprintf(stderr,"%s:%d: Expected ':NAMESPACE' after bitmap size.\n",refname,lineno);
      return -2;
    }
    switch (size) {
      case 8: case 16: case 24: case 32: break;
      default: {
          fprintf(stderr,"%s:%d: Bitmap size must be 8, 16, 24, or 32. Found %d.\n",refname,lineno,size);
          return -2;
        }
    }
    const char *nsname=src+srcp;
    int nsnamec=0;
    while ((srcp<srcc)&&(src[srcp++]!=')')) nsnamec++;
    if ((srcp>=srcc)||(src[srcp++]!='(')) {
      fprintf(stderr,"%s:%d: Expected '(TOKENS,...)' after bitmap introducer.\n",refname,lineno);
      return -2;
    }
    uint32_t v=0;
    for (;;) {
      if (srcp>=srcc) {
        fprintf(stderr,"%s:%d: Unclosed bitmap\n",refname,lineno);
        return -2;
      }
      if (src[srcp]==')') {
        srcp++;
        break;
      }
      if (src[srcp]==',') {
        srcp++;
        continue;
      }
      const char *bitname=src+srcp;
      int bitnamec=0;
      while ((srcp<srcc)&&(src[srcp]!=')')&&(src[srcp]!=',')) { srcp++; bitnamec++; }
      int bitix=0;
      if (eggdev_lookup_value_from_name(&bitix,EGGDEV_NS_MODE_NS,nsname,nsnamec,bitname,bitnamec)<0) {
        fprintf(stderr,"%s:%d: Failed to evaluate '%.*s' in namespace '%.*s'\n",refname,lineno,bitnamec,bitname,nsnamec,nsname);
        return -2;
      }
      if ((bitix<0)||(bitix>=size)) {
        fprintf(stderr,"%s:%d: Illegal bit index %d ('%.*s') for %d-bit integer.\n",refname,lineno,bitix,bitnamec,bitname,size);
        return -2;
      }
      v|=1<<bitix;
    }
    if (sr_encode_intbe(dst,v,size>>3)<0) return -1;
    return 0;
  }
  
  /* "(uSIZE)INT" or "(uSIZE:NAMESPACE)IDENTIFIER" for general integers.
   */
  if (src[0]=='(') {
    int srcp=1;
    if ((srcp>=srcc)||(src[srcp++]!='u')) {
      // Always 'u', and it's technically meaningless. Can take signed ints too.
      fprintf(stderr,"%s:%d: Expected 'u' and size after '('\n",refname,lineno);
      return -2;
    }
    int size=0;
    while ((srcp<srcc)&&(src[srcp]>='0')&&(src[srcp]<='9')) {
      size*=10;
      size+=src[srcp++]-'0';
      if (size>999) break;
    }
    if ((size<8)||(size>32)||(size&7)) {
      fprintf(stderr,"%s:%d: Integer size must be 8, 16, 24, or 32. Found %d.\n",refname,lineno,size);
      return -2;
    }
    const char *ns=0;
    int nsc=0;
    if ((srcp<srcc)&&(src[srcp]==':')) {
      srcp++;
      ns=src+srcp;
      while ((srcp<srcc)&&(src[srcp]!=')')) { srcp++; nsc++; }
    }
    if ((srcp>=srcc)||(src[srcp++]!=')')) {
      fprintf(stderr,"%s:%d: Expected ')'\n",refname,lineno);
      return -2;
    }
    int v,err;
    if (ns) {
      err=eggdev_lookup_value_from_name(&v,EGGDEV_NS_MODE_NS,ns,nsc,src+srcp,srcc-srcp);
    } else {
      err=sr_int_eval(&v,src+srcp,srcc-srcp);
    }
    if (err<0) {
      if (ns) fprintf(stderr,"%s:%d: Failed to evaluate '%.*s' in namespace '%.*s'\n",refname,lineno,srcc-srcp,src+srcp,nsc,ns);
      else fprintf(stderr,"%s:%d: Failed to evaluate '%.*s' as integer\n",refname,lineno,srcc-srcp,src+srcp);
      return -2;
    }
    switch (size) {
      case 8: if ((v<-128)||(v>0xff)) { fprintf(stderr,"%s:%d: Integer overflow, %d as u8\n",refname,lineno,v); return -1; } break;
      case 16: if ((v<-32768)||(v>0xffff)) { fprintf(stderr,"%s:%d: Integer overflow, %d as u16\n",refname,lineno,v); return -1; } break;
      case 24: if ((v<-16777216)||(v>0xffffff)) { fprintf(stderr,"%s:%d: Integer overflow, %d as u24\n",refname,lineno,v); return -1; } break;
    }
    if (sr_encode_intbe(dst,v,size>>3)<0) return -1;
    return 0;
  }
  
  /* "TYPE:NAME" for a 2-byte resource ID.
   */
  if (eggdev.rom) {
    int i=srcc;
    while (i-->0) {
      if (src[i]==':') {
        struct eggdev_res *res=eggdev_rom_res_by_string(eggdev.rom,src,srcc);
        if (!res) {
          fprintf(stderr,"%s:%d: Resource '%.*s' not found\n",refname,lineno,srcc,src);
          return -2;
        }
        // If there's a language, remove it from the emitted ID.
        if (sr_encode_intbe(dst,res->rid-(res->lang<<6),2)<0) return -1;
        return 0;
      }
    }
  }
  
  /* Everything else is a naked integer in 8 bits.
   */
  int v;
  if ((sr_int_eval(&v,src,srcc)<2)||(v<-128)||(v>0xff)) {
    fprintf(stderr,"%s:%d: Unexpected token '%.*s'\n",refname,lineno,srcc,src);
    return -2;
  }
  return sr_encode_u8(dst,v);
}

/* Binary from text.
 */
 
int eggdev_command_list_compile(
  struct sr_encoder *dst,
  const char *src,int srcc,
  const char *refname,int lineno0,
  const struct eggdev_ns *ns
) {
  struct sr_decoder decoder={.v=src,.c=srcc};
  const char *line;
  int linec,lineno=lineno0;
  for (;(linec=sr_decode_line(&line,&decoder))>0;lineno++) {
    while (linec&&((unsigned char)line[linec-1]<=0x20)) linec--;
    while (linec&&((unsigned char)line[0]<=0x20)) { linec--; line++; }
    if (!linec||(line[0]=='#')) continue;
    int linep=0;
    
    /* First token is name or opcode.
     */
    const char *name=line+linep;
    int namec=0,opcode;
    const struct eggdev_ns_entry *entry=0;
    while ((linep<linec)&&((unsigned char)line[linep++]>0x20)) namec++;
    if ((sr_int_eval(&opcode,name,namec)>=2)&&(opcode>0)&&(opcode<=0xff)) {
      entry=eggdev_ns_entry_by_value(ns,opcode); // Null is ok.
    } else {
      entry=eggdev_ns_entry_by_name(ns,name,namec);
      if (!entry) {
        fprintf(stderr,"%s:%d: Unknown command '%.*s'\n",refname,lineno,namec,name);
        return -2;
      }
      opcode=entry->id;
    }
    if (sr_encode_u8(dst,opcode)<0) return -1;
    
    /* Remaining tokens decode generically and independantly.
     * Mostly they split on whitespace, but there's also JSON string tokens.
     */
    int payloadp=dst->c,argp=0,fill=0;
    while (linep<linec) {
      if ((unsigned char)line[linep]<=0x20) { linep++; continue; }
      const char *token=line+linep;
      int tokenc=0;
      if (token[0]=='*') {
        fill=1;
        tokenc=1;
        linep++;
      } else if (fill) {
        fprintf(stderr,"%s:%d: '*' may only be the final argument\n",refname,lineno);
        return -2;
      } else if (token[0]=='"') {
        int simple=0;
        if ((tokenc=sr_string_measure(line+linep,linec-linep,&simple))<2) {
          fprintf(stderr,"%s:%d: Malformed string token.\n",refname,lineno);
          return -2;
        }
        if (simple) {
          if (sr_encode_raw(dst,token+1,tokenc-2)<0) return -1;
        } else {
          for (;;) {
            int err=sr_string_eval((char*)(dst->v)+dst->c,dst->a-dst->c,token,tokenc);
            if (err<0) {
              fprintf(stderr,"%s:%d: Malformed string token.\n",refname,lineno);
              return -2;
            }
            if (dst->c<=dst->a-err) {
              dst->c+=err;
              break;
            }
            if (sr_encoder_require(dst,err)<0) return -1;
          }
        }
        linep+=tokenc;
      } else {
        while ((linep<linec)&&((unsigned char)line[linep++]>0x20)) tokenc++;
        int err=eggdev_command_arg_compile(dst,token,tokenc,refname,lineno,entry,argp);
        if (err<0) {
          if (err!=-2) fprintf(stderr,"%s:%d: Unspecified error compiling argument '%.*s'\n",refname,lineno,tokenc,token);
          return -2;
        }
      }
      argp++;
    }
    
    /* Validate length, fill in if needed, and insert length for variable-length commands.
     */
    int paylen=dst->c-payloadp,reqlen;
    switch (opcode&0xe0) {
      case 0x00: reqlen=0; break;
      case 0x20: reqlen=2; break;
      case 0x40: reqlen=4; break;
      case 0x60: reqlen=8; break;
      case 0x80: reqlen=12; break;
      case 0xa0: reqlen=16; break;
      case 0xc0: {
          if (paylen>0xff) {
            fprintf(stderr,"%s:%d: Variable-length payload must not exceed 255 bytes, found %d.\n",refname,lineno,paylen);
            return -2;
          }
          reqlen=paylen;
          uint8_t tmp=paylen;
          if (sr_encoder_insert(dst,payloadp,&tmp,1)<0) return -1;
        } break;
      case 0xf0: reqlen=paylen; break; // Reserved commands are presumed valid, trust that the dev knows what he's doing.
    }
    if (paylen>reqlen) {
      fprintf(stderr,"%s:%d: Payload for opcode 0x%02x must be exactly %d bytes, found %d.\n",refname,lineno,opcode,reqlen,paylen);
      return -2;
    } else if (paylen<reqlen) {
      if (fill) {
        if (sr_encode_zero(dst,reqlen-paylen)<0) return -1;
      } else {
        fprintf(stderr,"%s:%d: Payload for opcode 0x%02x must be exactly %d bytes, found %d. Hint: add '*' to zero-pad automatically.\n",refname,lineno,opcode,reqlen,paylen);
        return -2;
      }
    }
  }
  return 0;
}

/* Text from binary.
 */
 
int eggdev_command_list_uncompile(
  struct sr_encoder *dst,
  const uint8_t *src,int srcc,
  const char *refname,
  const struct eggdev_ns *ns
) {
  int srcp=0;
  while (srcp<srcc) {
  
    uint8_t opcode=src[srcp++];
    if (!opcode) break; // Explicit EOF.
    
    const uint8_t *payload=src+srcp;
    int paylen=0;
    switch (opcode&0xe0) {
      case 0x00: break;
      case 0x20: paylen=2; break;
      case 0x40: paylen=4; break;
      case 0x60: paylen=8; break;
      case 0x80: paylen=12; break;
      case 0xa0: paylen=16; break;
      case 0xc0: {
          if (srcp>=srcc) return -1;
          paylen=src[srcp++];
          payload=src+srcp;
        } break;
      case 0xf0: return -1;
    }
    if (srcp>srcc-paylen) return -1;
    srcp+=paylen;
    
    const struct eggdev_ns_entry *entry=eggdev_ns_entry_by_value(ns,opcode);
    if (entry) {
      if (sr_encode_raw(dst,entry->name,entry->namec)<0) return -1;
    } else {
      if (sr_encode_fmt(dst,"0x%02x",opcode)<0) return -1;
    }
    if (paylen) {
      //TODO We might format (entry->args) formally such that we could produce sensible context-aware text here. Skipping that for now.
      if (sr_encode_raw(dst," 0x",3)<0) return -1;
      int i=0; for (;i<paylen;i++) {
        if (sr_encode_fmt(dst,"%02x",payload[i])<0) return -1;
      }
    }
    if (sr_encode_u8(dst,0x0a)<0) return -1;
  }
  return 0;
}

/* Add a namespace.
 */
 
static struct eggdev_ns *eggdev_ns_create(int mode,const char *nsname,int nsnamec) {
  if (eggdev.nsc>=eggdev.nsa) {
    int na=eggdev.nsa+16;
    if (na>INT_MAX/sizeof(struct eggdev_ns)) return 0;
    void *nv=realloc(eggdev.nsv,sizeof(struct eggdev_ns)*na);
    if (!nv) return 0;
    eggdev.nsv=nv;
    eggdev.nsa=na;
  }
  if (!nsname) nsnamec=0; else if (nsnamec<0) { nsnamec=0; while (nsname[nsnamec]) nsnamec++; }
  if (!nsnamec) return 0;
  char *nname=malloc(nsnamec+1);
  if (!nname) return 0;
  memcpy(nname,nsname,nsnamec);
  nname[nsnamec]=0;
  struct eggdev_ns *ns=eggdev.nsv+eggdev.nsc++;
  memset(ns,0,sizeof(struct eggdev_ns));
  ns->name=nname;
  ns->namec=nsnamec;
  ns->mode=mode;
  return ns;
}

/* Define one namespace symbol.
 */
 
static int eggdev_ns_define(int mode,const char *nsname,int nsnamec,const char *fname,int fnamec,int id,const char *args,int argsc) {
  struct eggdev_ns *ns=eggdev_ns_by_name(mode,nsname,nsnamec);
  if (!ns&&!(ns=eggdev_ns_create(mode,nsname,nsnamec))) return -1;
  if (ns->c>=ns->a) {
    int na=ns->a+32;
    if (na>INT_MAX/sizeof(struct eggdev_ns_entry)) return -1;
    void *nv=realloc(ns->v,sizeof(struct eggdev_ns_entry)*na);
    if (!nv) return -1;
    ns->v=nv;
    ns->a=na;
  }
  if (!fname) fnamec=0; else if (fnamec<0) { fnamec=0; while (fname[fnamec]) fnamec++; }
  if (!args) argsc=0; else if (argsc<0) { argsc=0; while (args[argsc]) argsc++; }
  char *nfname=0,*nargs=0;
  if (fnamec) {
    if (!(nfname=malloc(fnamec+1))) return -1;
    memcpy(nfname,fname,fnamec);
    nfname[fnamec]=0;
  }
  if (argsc) {
    if (!(nargs=malloc(argsc+1))) return -1;
    memcpy(nargs,args,argsc);
    nargs[argsc]=0;
  }
  struct eggdev_ns_entry *entry=ns->v+ns->c++;
  memset(entry,0,sizeof(struct eggdev_ns_entry));
  entry->name=nfname;
  entry->namec=fnamec;
  entry->args=nargs;
  entry->argsc=argsc;
  entry->id=id;
  return 0;
}

/* Acquire namespace from loose text.
 */
 
static int eggdev_ns_acquire_from_text(const char *src,int srcc,const char *path) {
  struct sr_decoder decoder={.v=src,.c=srcc};
  const char *line;
  int linec,lineno=1;
  for (;(linec=sr_decode_line(&line,&decoder))>0;lineno++) {
    while (linec&&((unsigned char)line[linec-1]<=0x20)) linec--;
    while (linec&&((unsigned char)line[0]<=0x20)) { linec--; line++; }
    
    /* Yoink the first 3 space-delimited tokens.
     * Must be "#define", "CMD_*" or "NS_*", and a plain integer.
     * After those three, must be EOL or a block comment extending to EOL.
     */
    int linep=0;
    const char *intro=line+linep;
    int introc=0;
    while ((linep<linec)&&((unsigned char)line[linep++]>0x20)) introc++;
    if ((introc!=7)||memcmp(intro,"#define",7)) continue;
    while ((linep<linec)&&((unsigned char)line[linep]<=0x20)) linep++;
    const char *symbol=line+linep;
    int symbolc=0;
    while ((linep<linec)&&((unsigned char)line[linep++]>0x20)) symbolc++;
    while ((linep<linec)&&((unsigned char)line[linep]<=0x20)) linep++;
    const char *value=line+linep;
    int valuec=0;
    while ((linep<linec)&&((unsigned char)line[linep++]>0x20)) valuec++;
    while ((linep<linec)&&((unsigned char)line[linep]<=0x20)) linep++;
    const char *args=0;
    int argsc=0;
    int remaining=linec-linep;
    if (!remaining) {
      // At EOL, cool.
    } else if ((remaining<4)||memcmp(line+linep,"/*",2)||memcmp(line+linec-2,"*/",2)) {
      continue; // Something other than a single-token value and block comment. Ignore line.
    } else {
      args=line+linep+2;
      argsc=linec-linep-4;
      while (argsc&&((unsigned char)args[argsc-1]<=0x20)) argsc--;
      while (argsc&&((unsigned char)args[0]<=0x20)) { args++; argsc--; }
    }
    
    // Value must be a plain integer.
    int v,err;
    if (sr_int_eval(&v,value,valuec)<2) continue;
    
    // Symbol is "{MODE}_{NAMESPACE}_{SYMBOL}"
    // If it's missing that second underscore, ignore the line.
    int symbolp,mode;
    if ((symbolc>=4)&&!memcmp(symbol,"CMD_",4)) { symbolp=4; mode=EGGDEV_NS_MODE_CMD; }
    else if ((symbolc>=3)&&!memcmp(symbol,"NS_",3)) { symbolp=3; mode=EGGDEV_NS_MODE_NS; }
    else continue;
    const char *nsname=symbol+symbolp;
    int nsnamec=0;
    while ((symbolp<symbolc)&&(symbol[symbolp++]!='_')) nsnamec++;
    const char *fname=symbol+symbolp;
    int fnamec=symbolc-symbolp;
    if (fnamec<1) continue;
    
    // Cool, define it.
    if ((err=eggdev_ns_define(mode,nsname,nsnamec,fname,fnamec,v,args,argsc))<0) return err;
  }
  return 0;
}

/* Acquire namespace from path.
 */
 
static int eggdev_ns_acquire(const char *path) {
  void *src=0;
  int srcc=file_read(&src,path);
  if (srcc<0) {
    fprintf(stderr,"%s: Failed to read schema file.\n",path);
    return -2;
  }
  int err=eggdev_ns_acquire_from_text(src,srcc,path);
  free(src);
  return err;
}

/* Namespace cache, public entry points.
 */
 
int eggdev_lookup_value_from_name(int *v,int mode,const char *_ns,int nsc,const char *name,int namec) {
  const struct eggdev_ns *ns=eggdev_ns_by_name(mode,_ns,nsc);
  return eggdev_ns_value_from_name(v,ns,name,namec);
}

int eggdev_lookup_name_from_value(const char **dstpp,int mode,const char *_ns,int nsc,int v) {
  const struct eggdev_ns *ns=eggdev_ns_by_name(mode,_ns,nsc);
  return eggdev_ns_name_from_value(dstpp,ns,v);
}

struct eggdev_ns *eggdev_ns_by_name(int mode,const char *name,int namec) {
  if (!name) return 0;
  if (namec<0) { namec=0; while (name[namec]) namec++; }
  if (!namec) return 0;
  eggdev_ns_require();
  struct eggdev_ns *ns=eggdev.nsv;
  int i=eggdev.nsc;
  for (;i-->0;ns++) {
    if (mode&&(mode!=ns->mode)) continue;
    if (ns->namec!=namec) continue;
    if (memcmp(ns->name,name,namec)) continue;
    return ns;
  }
  return 0;
}

struct eggdev_ns *eggdev_ns_by_tid(int tid) {
  const char *name=eggdev_tid_repr(tid);
  return eggdev_ns_by_name(EGGDEV_NS_MODE_CMD,name,-1);
}

struct eggdev_ns_entry *eggdev_ns_entry_by_name(const struct eggdev_ns *ns,const char *name,int namec) {
  if (!ns) return 0;
  if (!name) namec=0; else if (namec<0) { namec=0; while (name[namec]) namec++; }
  if (!namec) return 0;
  struct eggdev_ns_entry *entry=ns->v;
  int i=ns->c;
  for (;i-->0;entry++) {
    if (entry->namec!=namec) continue;
    if (memcmp(entry->name,name,namec)) continue;
    return entry;
  }
  return 0;
}

struct eggdev_ns_entry *eggdev_ns_entry_by_value(const struct eggdev_ns *ns,int v) {
  if (!ns) return 0;
  struct eggdev_ns_entry *entry=ns->v;
  int i=ns->c;
  for (;i-->0;entry++) {
    if (entry->id!=v) continue;
    return entry;
  }
  return 0;
}

int eggdev_ns_value_from_name(int *v,const struct eggdev_ns *ns,const char *name,int namec) {
  const struct eggdev_ns_entry *entry=eggdev_ns_entry_by_name(ns,name,namec);
  if (!entry) return -1;
  *v=entry->id;
  return 0;
}

int eggdev_ns_name_from_value(const char **dstpp,const struct eggdev_ns *ns,int v) {
  const struct eggdev_ns_entry *entry=eggdev_ns_entry_by_value(ns,v);
  if (!entry) return -1;
  *dstpp=entry->name;
  return entry->namec;
}

void eggdev_ns_require() {
  // Populating the cache will accidentally re-enter here.
  // Rather than making separate reentrant and non-reentrant APIs, just flag the function and abort if already running.
  if (eggdev.ns_acquisition_in_progress) return;
  eggdev.ns_acquisition_in_progress=1;
  if (eggdev.schema_volatile) {
    if (!eggdev.nsc) {
      int i=eggdev.schemasrcc;
      while (i-->0) {
        eggdev_ns_acquire(eggdev.schemasrcv[i]);
      }
    }
  } else {
    while (eggdev.schemasrcc>0) {
      const char *path=eggdev.schemasrcv[--(eggdev.schemasrcc)];
      eggdev_ns_acquire(path);
    }
  }
  eggdev.ns_acquisition_in_progress=0;
}

void eggdev_ns_flush() {
  while (eggdev.nsc>0) {
    eggdev.nsc--;
    struct eggdev_ns *ns=eggdev.nsv+eggdev.nsc;
    if (ns->name) free(ns->name);
    if (ns->v) {
      while (ns->c-->0) {
        struct eggdev_ns_entry *entry=ns->v+ns->c;
        if (entry->name) free(entry->name);
        if (entry->args) free(entry->args);
      }
      free(ns->v);
    }
  }
}
