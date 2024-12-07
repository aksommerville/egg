#include "eggdev_internal.h"

/* Find schema by name or opcode.
 */
 
const struct eggdev_command_list_schema *eggdev_command_list_schema_by_opcode(
  const struct eggdev_command_list_schema *schema,int schemac,
  uint8_t opcode
) {
  if (!opcode) return 0;
  for (;schemac-->0;schema++) {
    if (schema->opcode==opcode) return schema;
  }
  return 0;
}
 
const struct eggdev_command_list_schema *eggdev_command_list_schema_by_name(
  const struct eggdev_command_list_schema *schema,int schemac,
  const char *name,int namec
) {
  if (!name) return 0;
  if (namec<0) { namec=0; while (name[namec]) namec++; }
  for (;schemac-->0;schema++) {
    if (memcmp(schema->name,name,namec)) continue;
    if (schema->name[namec]) continue;
    return schema;
  }
  return 0;
}

/* Compile one argument.
 */
 
static int eggdev_command_arg_compile(
  struct sr_encoder *dst,
  const char *src,int srcc,
  const char *refname,int lineno,
  const struct eggdev_command_list_schema *def,
  int argp,
  struct eggdev_rom *rom
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
      err=eggdev_namespace_lookup(&v,ns,nsc,src+srcp,srcc-srcp,rom);
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
  if (rom) {
    int i=srcc;
    while (i-->0) {
      if (src[i]==':') {
        struct eggdev_res *res=eggdev_rom_res_by_string(rom,src,srcc);
        if (!res) {
          fprintf(stderr,"%s:%d: Resource '%.*s' not found\n",refname,lineno,srcc,src);
          return -2;
        }
        if (sr_encode_intbe(dst,res->rid,2)<0) return -1;
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
  const struct eggdev_command_list_schema *schema,int schemac,
  struct eggdev_rom *rom
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
    const struct eggdev_command_list_schema *def=0;
    while ((linep<linec)&&((unsigned char)line[linep++]>0x20)) namec++;
    if ((sr_int_eval(&opcode,name,namec)>=2)&&(opcode>0)&&(opcode<=0xff)) {
      def=eggdev_command_list_schema_by_opcode(schema,schemac,opcode); // Null is ok.
    } else {
      def=eggdev_command_list_schema_by_name(schema,schemac,name,namec);
      if (!def) {
        fprintf(stderr,"%s:%d: Unknown command '%.*s'\n",refname,lineno,namec,name);
        return -2;
      }
      opcode=def->opcode;
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
        int err=eggdev_command_arg_compile(dst,token,tokenc,refname,lineno,def,argp,rom);
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
  const struct eggdev_command_list_schema *schema,int schemac,
  struct eggdev_rom *rom
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
    
    const struct eggdev_command_list_schema *def=eggdev_command_list_schema_by_opcode(schema,schemac,opcode);
    if (def) {
      if (sr_encode_raw(dst,def->name,-1)<0) return -1;
    } else {
      if (sr_encode_fmt(dst,"0x%02x",opcode)<0) return -1;
    }
    if (paylen) {
      //TODO We might format (def->args) formally such that we could produce sensible context-aware text here. Skipping that for now.
      if (sr_encode_raw(dst," 0x",3)<0) return -1;
      int i=0; for (;i<paylen;i++) {
        if (sr_encode_fmt(dst,"%02x",payload[i])<0) return -1;
      }
    }
    if (sr_encode_u8(dst,0x0a)<0) return -1;
  }
  return 0;
}

/* Schema and namespace cache primitives.
 */
 
static int eggdev_cmd_entry_search(int tid) {
  int lo=0,hi=eggdev.cmd_entryc;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
    const struct eggdev_cmd_entry *q=eggdev.cmd_entryv+ck;
         if (tid<q->tid) hi=ck;
    else if (tid>q->tid) lo=ck+1;
    else return ck;
  }
  return -lo-1;
}

static struct eggdev_cmd_entry *eggdev_cmd_entry_insert(int p,int tid) {
  if (eggdev.cmd_entryc>=eggdev.cmd_entrya) {
    int na=eggdev.cmd_entrya+8;
    if (na>INT_MAX/sizeof(struct eggdev_cmd_entry)) return 0;
    void *nv=realloc(eggdev.cmd_entryv,sizeof(struct eggdev_cmd_entry)*na);
    if (!nv) return 0;
    eggdev.cmd_entryv=nv;
    eggdev.cmd_entrya=na;
  }
  struct eggdev_cmd_entry *entry=eggdev.cmd_entryv+p;
  memmove(entry+1,entry,sizeof(struct eggdev_cmd_entry)*(eggdev.cmd_entryc-p));
  eggdev.cmd_entryc++;
  memset(entry,0,sizeof(struct eggdev_cmd_entry));
  entry->tid=tid;
  return entry;
}

/* Define new symbols.
 */
 
static int eggdev_define_schema_symbol(const char *tname,int tnamec,const char *cname,int cnamec,const char *args,int argsc,int v,const char *path,int lineno,struct eggdev_rom *rom) {
  int tid=eggdev_tid_eval(rom,tname,tnamec);
  if (tid<=0) {
    fprintf(stderr,"%s:%d: Unknown resource type '%.*s'\n",path,lineno,tnamec,tname);
    return -2;
  }
  struct eggdev_cmd_entry *entry;
  int p=eggdev_cmd_entry_search(tid);
  if (p>=0) {
    entry=eggdev.cmd_entryv+p;
  } else {
    p=-p-1;
    if (!(entry=eggdev_cmd_entry_insert(p,tid))) return -1;
  }
  if (entry->c>=entry->a) {
    int na=entry->a+16;
    if (na>INT_MAX/sizeof(struct eggdev_command_list_schema)) return -1;
    void *nv=realloc(entry->v,sizeof(struct eggdev_command_list_schema)*na);
    if (!nv) return -1;
    entry->v=nv;
    entry->a=na;
  }
  struct eggdev_command_list_schema *schema=entry->v+entry->c++;
  memset(schema,0,sizeof(struct eggdev_command_list_schema));
  char *nname=malloc(cnamec+1);
  if (!nname) return -1;
  memcpy(nname,cname,cnamec);
  nname[cnamec]=0;
  schema->name=nname; // leak
  schema->opcode=v;
  if (argsc) {
    char *nargs=malloc(argsc+1);
    if (!nargs) return -1;
    memcpy(nargs,args,argsc);
    nargs[argsc]=0;
    schema->args=nargs; // leak
  }
  return 0;
}

static int eggdev_namespace_add(struct eggdev_namespace *namespace,const char *name,int namec,const char *args,int argsc,int v) {
  if (namespace->entryc>=namespace->entrya) {
    int na=namespace->entrya+16;
    if (na>INT_MAX/sizeof(struct eggdev_ns_entry)) return -1;
    void *nv=realloc(namespace->entryv,sizeof(struct eggdev_ns_entry)*na);
    if (!nv) return -1;
    namespace->entryv=nv;
    namespace->entrya=na;
  }
  struct eggdev_ns_entry *entry=namespace->entryv+namespace->entryc++;
  memset(entry,0,sizeof(struct eggdev_ns_entry));
  if (!(entry->name=malloc(namec+1))) return -1;
  memcpy(entry->name,name,namec);
  entry->name[namec]=0;
  entry->namec=namec;
  entry->id=v;
  return 0;
}

static int eggdev_define_ns_symbol(const char *ns,int nsc,const char *name,int namec,const char *args,int argsc,int v,const char *path,int lineno,struct eggdev_rom *rom) {
  int nsi=eggdev.namespacec;
  while (nsi-->0) {
    struct eggdev_namespace *namespace=eggdev.namespacev+nsi;
    if (namespace->nsc!=nsc) continue;
    if (memcmp(namespace->ns,ns,nsc)) continue;
    return eggdev_namespace_add(namespace,name,namec,args,argsc,v);
  }
  if (eggdev.namespacec>=eggdev.namespacea) {
    int na=eggdev.namespacea+8;
    if (na>INT_MAX/sizeof(struct eggdev_namespace)) return -1;
    void *nv=realloc(eggdev.namespacev,sizeof(struct eggdev_namespace)*na);
    if (!nv) return -1;
    eggdev.namespacev=nv;
    eggdev.namespacea=na;
  }
  struct eggdev_namespace *namespace=eggdev.namespacev+eggdev.namespacec++;
  memset(namespace,0,sizeof(struct eggdev_namespace));
  if (!(namespace->ns=malloc(nsc+1))) return -1;
  memcpy(namespace->ns,ns,nsc);
  namespace->ns[nsc]=0;
  namespace->nsc=nsc;
  return eggdev_namespace_add(namespace,name,namec,args,argsc,v);
}

/* Read a schema file and add any symbols we find.
 */
 
static int eggdev_schema_read(const char *path,struct eggdev_rom *rom) {
  char *src=0;
  int srcc=file_read(&src,path);
  if (srcc<0) {
    fprintf(stderr,"%s:WARNING: Failed to read schema file.\n",path);
    return -2;
  }
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
    
    int v,err;
    if (sr_int_eval(&v,value,valuec)<2) continue;
    
    if ((symbolc>=4)&&!memcmp(symbol,"CMD_",4)) {
      int symp=4;
      for (;symp<symbolc;symp++) {
        if (symbol[symp]=='_') {
          const char *tname=symbol+4;
          int tnamec=symp-4;
          const char *cname=symbol+symp+1;
          int cnamec=symbolc-symp-1;
          if ((err=eggdev_define_schema_symbol(tname,tnamec,cname,cnamec,args,argsc,v,path,lineno,rom))<0) return err;
          break;
        }
      }
      
    } else if ((symbolc>=3)&&!memcmp(symbol,"NS_",3)) {
      int symp=3;
      for (;symp<symbolc;symp++) {
        if (symbol[symp]=='_') {
          const char *ns=symbol+3;
          int nsc=symp-3;
          const char *name=symbol+symp+1;
          int namec=symbolc-symp-1;
          if ((err=eggdev_define_ns_symbol(ns,nsc,name,namec,args,argsc,v,path,lineno,rom))<0) return err;
          break;
        }
      }
    }
  }
  free(src);
  return 0;
}

/* Get schema.
 */
 
int eggdev_command_list_schema_lookup(const struct eggdev_command_list_schema **dstpp,int tid,struct eggdev_rom *rom) {
  struct eggdev_cmd_entry *entry=0;
  int p=eggdev_cmd_entry_search(tid);
  if (p<0) {
    p=-p-1;
    entry=eggdev_cmd_entry_insert(p,tid);
    if (!entry) return 0;
    while (eggdev.schemasrcc&&!entry->c) {
      const char *path=eggdev.schemasrcv[--(eggdev.schemasrcc)];
      eggdev_schema_read(path,rom);
    }
  } else {
    entry=eggdev.cmd_entryv+p;
  }
  *dstpp=entry->v;
  return entry->c;
}

/* Lookup symbol in namespace.
 */
 
int eggdev_namespace_lookup(int *v,const char *ns,int nsc,const char *token,int tokenc,struct eggdev_rom *rom) {
  if (!ns||!token) return -1;
  if (nsc<0) { nsc=0; while (ns[nsc]) nsc++; }
  if (tokenc<0) { tokenc=0; while (token[tokenc]) tokenc++; }
 _again_:;
  struct eggdev_namespace *namespace=eggdev.namespacev;
  int nsi=eggdev.namespacec;
  for (;nsi-->0;namespace++) {
    if (namespace->nsc!=nsc) continue;
    if (memcmp(namespace->ns,ns,nsc)) continue;
    struct eggdev_ns_entry *entry=namespace->entryv;
    int ei=namespace->entryc;
    for (;ei-->0;entry++) {
      if (entry->namec!=tokenc) continue;
      if (memcmp(entry->name,token,tokenc)) continue;
      *v=entry->id;
      return 0;
    }
    return -1;
  }
  // We didn't find the namespace. If any schemae are still unresolved, resolve them and try again.
  if (eggdev.schemasrcc>0) {
    eggdev_command_list_require(rom);
    if (!eggdev.schemasrcc) goto _again_;
  }
  return -1;
}

/* Namespace reverse lookup.
 */
 
int eggdev_namespace_name_from_id(const char **dstpp,const char *ns,int nsc,int id,struct eggdev_rom *rom) {
  if (!ns) return -1;
  if (nsc<0) { nsc=0; while (ns[nsc]) nsc++; }
 _again_:;
  struct eggdev_namespace *namespace=eggdev.namespacev;
  int nsi=eggdev.namespacec;
  for (;nsi-->0;namespace++) {
    if (namespace->nsc!=nsc) continue;
    if (memcmp(namespace->ns,ns,nsc)) continue;
    struct eggdev_ns_entry *entry=namespace->entryv;
    int ei=namespace->entryc;
    for (;ei-->0;entry++) {
      if (entry->id==id) {
        *dstpp=entry->name;
        return entry->namec;
      }
    }
    return -1;
  }
  // We didn't find the namespace. If any schemae are still unresolved, resolve them and try again.
  if (eggdev.schemasrcc>0) {
    eggdev_command_list_require(rom);
    if (!eggdev.schemasrcc) goto _again_;
  }
  return -1;
}

/* Load all def files.
 */
 
void eggdev_command_list_require(struct eggdev_rom *rom) {
  while (eggdev.schemasrcc>0) {
    const char *path=eggdev.schemasrcv[--(eggdev.schemasrcc)];
    eggdev_schema_read(path,rom);
  }
}
