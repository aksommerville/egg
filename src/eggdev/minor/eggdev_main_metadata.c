#include "eggdev/eggdev_internal.h"

/* Context.
 */
 
struct eggdev_metadata_context {
  const char *path;
  struct sr_encoder dst;
  const uint8_t *src;
  int srcc;
  int lang; // -1=all, 0=none, >0=single (10-bit)
  int iconImage;
  int posterImage;
  struct eggdev_metadata_field {
    // Empty keys will be skipped at the final encode.
    char *k,*v;
    int kc,vc;
    int raw;
  } *fieldv;
  int fieldc,fielda;
};

static void eggdev_metadata_field_cleanup(struct eggdev_metadata_field *field) {
  if (field->k) free(field->k);
  if (field->v) free(field->v);
}

static void eggdev_metadata_context_cleanup(struct eggdev_metadata_context *ctx) {
  sr_encoder_cleanup(&ctx->dst);
  if (ctx->fieldv) {
    while (ctx->fieldc-->0) eggdev_metadata_field_cleanup(ctx->fieldv+ctx->fieldc);
    free(ctx->fieldv);
  }
}

/* Add a field.
 */
 
static int eggdev_metadata_add(struct eggdev_metadata_context *ctx,const char *k,int kc,const char *v,int vc) {
  if (ctx->fieldc>=ctx->fielda) {
    int na=ctx->fielda+16;
    if (na>INT_MAX/sizeof(struct eggdev_metadata_field)) return -1;
    void *nv=realloc(ctx->fieldv,sizeof(struct eggdev_metadata_field)*na);
    if (!nv) return -1;
    ctx->fieldv=nv;
    ctx->fielda=na;
  }
  char *nk=malloc(kc+1);
  if (!nk) return -1;
  char *nv=malloc(vc+1);
  if (!nv) { free(nk); return -1; }
  memcpy(nk,k,kc);
  nk[kc]=0;
  memcpy(nv,v,vc);
  nv[vc]=0;
  struct eggdev_metadata_field *field=ctx->fieldv+ctx->fieldc++;
  field->k=nk;
  field->kc=kc;
  field->v=nv;
  field->vc=vc;
  field->raw=0;
  return 0;
}

/* Search fields.
 */
 
static int eggdev_metadata_field_search(const struct eggdev_metadata_context *ctx,const char *k,int kc) {
  int i=0;
  const struct eggdev_metadata_field *field=ctx->fieldv;
  for (;i<ctx->fieldc;i++,field++) {
    if (field->kc!=kc) continue;
    if (memcmp(field->k,k,kc)) continue;
    return i;
  }
  return -1;
}

/* Resolve languaged string.
 * Allocate a new output buffer and emit encoded JSON.
 */
 
static int eggdev_metadata_resolve_single_lang(void *dstpp,const struct eggdev_metadata_context *ctx,int lang,int rid,int index,const char *natural,int naturalc) {
  int qrid=(lang<<6)|rid;
  int resp=eggdev_rom_search(eggdev.rom,EGG_TID_strings,qrid);
  if (resp>=0) {
    const char *src=0;
    int srcc=rom_string_by_index(&src,eggdev.rom->resv[resp].serial,eggdev.rom->resv[resp].serialc,index);
    if (srcc>0) {
      natural=src;
      naturalc=srcc;
    }
  }
  int dsta=naturalc+16;
  char *dst=malloc(dsta);
  if (!dst) return -1;
  int dstc=-1;
  for (;;) {
    dstc=sr_string_repr(dst,dsta,natural,naturalc);
    if (dstc<0) { free(dst); return -1; }
    if (dstc<=dsta) break;
    dsta=dstc+1;
    void *nv=realloc(dst,dsta);
    if (!nv) { free(dst); return -1; }
    dst=nv;
  }
  *(void**)dstpp=dst;
  return dstc;
}
 
static int eggdev_metadata_resolve_all_langs(void *dstpp,const struct eggdev_metadata_context *ctx,int rid,int index,const char *natural,int naturalc) {
  struct sr_encoder encoder={0};
  if (sr_encode_json_object_start(&encoder,0,0)<0) return -1;
  if (naturalc) {
    if (sr_encode_json_string(&encoder,"default",7,natural,naturalc)<0) { sr_encoder_cleanup(&encoder); return -1; }
  }
  const struct eggdev_res *res=eggdev.rom->resv;
  int resi=eggdev.rom->resc;
  for (;resi-->0;res++) {
    if (res->tid<EGG_TID_strings) continue;
    if (res->tid>EGG_TID_strings) break;
    if ((res->rid&0x3f)!=rid) continue;
    int lang=res->rid>>6;
    if (lang&~0x3ff) continue;
    if (((lang>>5)>=26)||((lang&0x1f)>=26)) continue;
    const char *v=0;
    int vc=rom_string_by_index(&v,res->serial,res->serialc,index);
    if (vc<1) continue;
    char langstr[2]={'a'+(lang>>5),'a'+(lang&0x1f)};
    if (sr_encode_json_string(&encoder,langstr,2,v,vc)<0) { sr_encoder_cleanup(&encoder); return -1; }
  }
  if (sr_encode_json_end(&encoder,0)<0) { sr_encoder_cleanup(&encoder); return -1; }
  *(void**)dstpp=encoder.v; // yoink
  return encoder.c;
}

/* With all fields populated, replace values as needed.
 * If there were no extra arguments from the user, we're noop.
 */
 
static int eggdev_metadata_digest(struct eggdev_metadata_context *ctx) {

  // If iconImage or posterImage in play, they're simple 1:1 replacements.
  if (ctx->iconImage||ctx->posterImage) {
    struct eggdev_metadata_field *field=ctx->fieldv;
    int i=ctx->fieldc;
    for (;i-->0;field++) {
      if (
        (ctx->iconImage&&(field->kc==9)&&!memcmp(field->k,"iconImage",9))||
        (ctx->posterImage&&(field->kc==11)&&!memcmp(field->k,"posterImage",11))
      ) {
        int rid=0;
        if ((sr_int_eval(&rid,field->v,field->vc)<2)||(rid<1)||(rid>0xffff)) {
          fprintf(stderr,"%s:WARNING: %.*s '%.*s', invalid resource id.\n",ctx->path,field->kc,field->k,field->vc,field->v);
          field->kc=0;
        } else {
          int resp=eggdev_rom_search(eggdev.rom,EGG_TID_image,rid);
          if (resp<0) {
            fprintf(stderr,"%s:WARNING: %.*s %d, image not found.\n",ctx->path,field->kc,field->k,rid);
            field->kc=0;
          } else {
            const void *src=eggdev.rom->resv[resp].serial;
            int srcc=eggdev.rom->resv[resp].serialc;
            int dsta=2+(srcc<<1); // Really 2+ceil(srcc*4/3); +2*2 covers it.
            char *dst=malloc(dsta);
            if (!dst) return -1;
            int dstc=sr_base64_encode(dst+1,dsta-1,src,srcc);
            if ((dstc<0)||(dstc>dsta-2)) {
              free(dst);
              return -1;
            }
            dst[0]='"';
            dst[1+dstc]='"';
            dstc+=2;
            free(field->v);
            field->v=dst;
            field->vc=dstc;
            field->raw=1;
          }
        }
      }
    }
  }
  
  // If we have a language, it's messy.
  if (ctx->lang) {
    struct eggdev_metadata_field *field=ctx->fieldv;
    int i=ctx->fieldc;
    for (;i-->0;field++) {
      if (field->kc<1) continue;
      if (field->k[field->kc-1]!='$') continue;
      int index=-1;
      if ((sr_int_eval(&index,field->v,field->vc)<2)||(index<0)) {
        fprintf(stderr,"%s: Invalid index '%.*s' for field '%.*s'\n",ctx->path,field->vc,field->v,field->kc,field->k);
        continue;
      }
      int naturalp=eggdev_metadata_field_search(ctx,field->k,field->kc-1);
      const char *naturalv=0;
      int naturalvc=0;
      if (naturalp>=0) {
        naturalv=ctx->fieldv[naturalp].v;
        naturalvc=ctx->fieldv[naturalp].vc;
      }
      char *nv=0;
      int nvc=-1;
      if (ctx->lang>=0) {
        nvc=eggdev_metadata_resolve_single_lang(&nv,ctx,ctx->lang,1,index,naturalv,naturalvc);
      } else {
        nvc=eggdev_metadata_resolve_all_langs(&nv,ctx,1,index,naturalv,naturalvc);
      }
      if (nvc<0) return -1;
      if (field->v) free(field->v);
      field->v=nv;
      field->vc=nvc;
      field->raw=1;
      field->kc--; // Lop off the '$'
      if (naturalp>=0) {
        ctx->fieldv[naturalp].kc=0;
      }
    }
  }

  return 0;
}

/* metadata, in context.
 * Caller populates (path,rom,lang,iconImage,posterImage).
 */
 
static int eggdev_metadata_inner(struct eggdev_metadata_context *ctx) {
  
  /* metadata:1 should always be the first resource, but search generically just to be safe.
   */
  int resp=eggdev_rom_search(eggdev.rom,EGG_TID_metadata,1);
  if (resp<0) {
    fprintf(stderr,"%s: metadata:1 not found\n",ctx->path);
    return -2;
  }
  ctx->src=eggdev.rom->resv[resp].serial;
  ctx->srcc=eggdev.rom->resv[resp].serialc;
  if ((ctx->srcc<4)||memcmp(ctx->src,"\0EM\xff",4)) {
    fprintf(stderr,"%s: Invalid metadata, signature check failed.\n",ctx->path);
    return -2;
  }
  int srcp=4,err;
  
  /* Collect the verbatim values for each field.
   * In the no-args case, this is overkill, we could just write them directly.
   * But no sense making a special case for that.
   */
  while (srcp<ctx->srcc) {
    uint8_t kc=ctx->src[srcp++];
    if (!kc) break;
    if (srcp>=ctx->srcc) break;
    uint8_t vc=ctx->src[srcp++];
    if (srcp>ctx->srcc-vc-kc) break;
    const char *k=(char*)(ctx->src+srcp); srcp+=kc;
    const char *v=(char*)(ctx->src+srcp); srcp+=vc;
    if ((err=eggdev_metadata_add(ctx,k,kc,v,vc))<0) return err;
  }
  if ((err=eggdev_metadata_digest(ctx))<0) return err;
  
  /* We're going to emit the JSON framing piecemeal instead of using sr_encoder's facilities.
   * Mostly just because sr_encoder does not provide newlines and indent.
   */
  ctx->dst.c=0;
  if (sr_encode_raw(&ctx->dst,"{\n",2)<0) return -1;
  int commap=-1; // Track the position of the final comma. It's illegal and we'll have to overwrite it with a space.
  const struct eggdev_metadata_field *field=ctx->fieldv;
  int i=ctx->fieldc;
  for (;i-->0;field++) {
    if (!field->kc) continue;
    if (sr_encode_raw(&ctx->dst,"  ",2)<0) return -1;
    if (sr_encode_json_string(&ctx->dst,0,0,field->k,field->kc)<0) return -1;
    if (sr_encode_raw(&ctx->dst,": ",2)<0) return -1;
    if (field->raw) {
      if (sr_encode_raw(&ctx->dst,field->v,field->vc)<0) return -1;
    } else {
      if (sr_encode_json_string(&ctx->dst,0,0,field->v,field->vc)<0) return -1;
    }
    commap=ctx->dst.c;
    if (sr_encode_raw(&ctx->dst,",\n",2)<0) return -1;
  }
  
  if (commap>=0) ((char*)ctx->dst.v)[commap]=' ';
  if (sr_encode_raw(&ctx->dst,"}\n",2)<0) return -1;
  fprintf(stdout,"%.*s",ctx->dst.c,(char*)ctx->dst.v);
  return 0;
}

/* metadata, main entry point.
 */
 
int eggdev_main_metadata() {
  int err;
  if (eggdev.srcpathc!=1) {
    fprintf(stderr,"%s: Exactly one input file is required for 'metadata'\n",eggdev.exename);
    return -2;
  }
  struct eggdev_metadata_context ctx={
    .path=eggdev.srcpathv[0],
    .iconImage=eggdev.iconImage,
    .posterImage=eggdev.posterImage,
  };
  if (eggdev.lang) {
    if (!strcmp(eggdev.lang,"all")) {
      ctx.lang=-1;
    } else {
      const char *l=eggdev.lang;
      if ((l[0]>='a')&&(l[0]<='z')&&(l[1]>='a')&&(l[1]<='z')&&!l[2]) {
        ctx.lang=((l[0]-'a')<<5)|(l[1]-'a');
      } else {
        fprintf(stderr,"%s: Unexpected value '%s' for --lang. Must be 'all' or 2 lowercase letters.\n",eggdev.exename,eggdev.lang);
        return -2;
      }
    }
  }
  if ((err=eggdev_require_rom(ctx.path))<0) return err;
  if ((err=eggdev_metadata_inner(&ctx))<0) {
    if (err!=-2) fprintf(stderr,"%s: Unspecified error dumping metadata.\n",ctx.path);
    eggdev_metadata_context_cleanup(&ctx);
    return -2;
  }
  eggdev_metadata_context_cleanup(&ctx);
  return 0;
}
