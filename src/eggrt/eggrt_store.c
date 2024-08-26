#include "eggrt_internal.h"

/* Quit.
 */
 
static void eggrt_store_field_cleanup(struct eggrt_store_field *field) {
  if (field->k) free(field->k);
  if (field->v) free(field->v);
}
 
void eggrt_store_quit() {
  if (eggrt.storev) {
    while (eggrt.storec-->0) eggrt_store_field_cleanup(eggrt.storev+eggrt.storec);
    free(eggrt.storev);
  }
  eggrt.storev=0;
  eggrt.storec=0;
  eggrt.storea=0;
}

/* Encode and decode.
 */
 
static int eggrt_store_decode(const void *src,int srcc,const char *path) {
  if (eggrt.storec) return -1;
  struct sr_decoder decoder={.v=src,.c=srcc};
  if (sr_decode_json_object_start(&decoder)<0) return -1;
  const char *k;
  int kc;
  int va=1024;
  char *v=malloc(va);
  if (!v) return -1;
  while ((kc=sr_decode_json_next(&k,&decoder))>0) {
    struct eggrt_store_field *field=eggrt_store_get_field(k,kc,1);
    if (!field) { free(v); return -1; }
    
    int vc;
    for (;;) {
      if ((vc=sr_decode_json_string(v,va,&decoder))<0) { free(v); return -1; }
      if (vc<=va) break;
      va=(vc+1024)&~1023;
      void *nv=realloc(v,va);
      if (!nv) { free(v); return -1; }
      v=nv;
    }
    
    if (eggrt_store_set_field(field,v,vc)<0) { free(v); return -1; }
  }
  int err=sr_decode_json_end(&decoder,0);
  return err;
}

static int eggrt_store_encode(struct sr_encoder *dst) {
  if (sr_encode_json_object_start(dst,0,0)<0) return -1;
  const struct eggrt_store_field *field=eggrt.storev;
  int i=eggrt.storec;
  for (;i-->0;field++) {
    if (sr_encode_json_string(dst,field->k,field->kc,field->v,field->vc)<0) return -1;
  }
  if (sr_encode_json_end(dst,0)<0) return -1;
  if (sr_encode_u8(dst,0x0a)<0) return -1;
  return 0;
}

/* Init.
 */
 
int eggrt_store_init() {
  if (!eggrt.storepath) {
    fprintf(stderr,"%s: Will not save game as --store unset.\n",eggrt.exename);
    return 0;
  }
  void *src=0;
  int srcc=file_read(&src,eggrt.storepath);
  if (srcc<0) {
    fprintf(stderr,"%s: No saved game.\n",eggrt.storepath);
    return 0;
  }
  int err=eggrt_store_decode(src,srcc,eggrt.storepath);
  free(src);
  if (err<0) {
    if (err!=-2) fprintf(stderr,"%s: Malformed saved game.\n",eggrt.storepath);
    return -2;
  }
  fprintf(stderr,"%s: Loaded saved game.\n",eggrt.storepath);
  return 0;
}

/* Save.
 */
 
int eggrt_store_save() {
  if (!eggrt.storepath) return -1;
  eggrt.store_dirty=0; // Clear dirty flag even if it fails. We won't try again until the next change.
  struct sr_encoder encoder={0};
  if (eggrt_store_encode(&encoder)<0) {
    sr_encoder_cleanup(&encoder);
    return -1;
  }
  int err=file_write(eggrt.storepath,encoder.v,encoder.c);
  sr_encoder_cleanup(&encoder);
  if (err<0) {
    fprintf(stderr,"%s: Failed to write saved game, %d bytes.\n",eggrt.storepath,encoder.c);
    return -2;
  }
  return 0;
}

/* Validate text.
 */
 
static int eggrt_store_key_valid(const char *src,int srcc) {
  if ((srcc<1)||(srcc>255)) return 0;
  for (;srcc-->0;src++) {
    if (*src<0x20) return 0;
    if (*src>0x7e) return 0;
    if (*src=='\\') return 0;
    if (*src=='"') return 0;
  }
  return 1;
}

static int eggrt_store_value_valid(const char *src,int srcc) {
  int srcp=0,ch,seqlen;
  while (srcp<srcc) {
    if (!(src[srcp]&0x80)) { srcp++; continue; }
    if ((seqlen=sr_utf8_decode(&ch,src+srcp,srcc-srcp))<1) return 0;
    srcp+=seqlen;
  }
  return 1;
}

/* Get field.
 */
 
struct eggrt_store_field *eggrt_store_get_field(const char *k,int kc,int create) {
  if (!k) return 0;
  if (kc<0) { kc=0; while ((kc<256)&&k[kc]) kc++; }
  if (!eggrt_store_key_valid(k,kc)) return 0;
  int lo=0,hi=eggrt.storec;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
    struct eggrt_store_field *field=eggrt.storev+ck;
         if (kc<field->kc) hi=ck;
    else if (kc>field->kc) lo=ck+1;
    else {
      int cmp=memcmp(k,field->k,kc);
           if (cmp<0) hi=ck;
      else if (cmp>0) lo=ck+1;
      else return field;
    }
  }
  if (create) {
    if (eggrt.storec>=eggrt.storea) {
      int na=eggrt.storea+16;
      if (na>INT_MAX/sizeof(struct eggrt_store_field)) return 0;
      void *nv=realloc(eggrt.storev,sizeof(struct eggrt_store_field)*na);
      if (!nv) return 0;
      eggrt.storev=nv;
      eggrt.storea=na;
    }
    char *nk=malloc(kc+1);
    if (!nk) return 0;
    memcpy(nk,k,kc);
    nk[kc]=0;
    struct eggrt_store_field *field=eggrt.storev+lo;
    memmove(field+1,field,sizeof(struct eggrt_store_field)*(eggrt.storec-lo));
    eggrt.storec++;
    field->k=nk;
    field->kc=kc;
    field->v=0;
    field->vc=0;
    return field;
  }
  return 0;
}

/* Set field.
 */
 
int eggrt_store_set_field(struct eggrt_store_field *field,const char *v,int vc) {
  if (!eggrt.storepath) return -1; // No saving, and we won't pretend to.
  if (!field) return -1;
  if ((field<eggrt.storev)||(field-eggrt.storev>=eggrt.storec)) return -1;
  if (!v) vc=0; else if (vc<0) { vc=0; while (v[vc]) vc++; }
  if (!eggrt_store_value_valid(v,vc)) return -1;
  
  if (vc) {
    if (vc>field->vc) {
      char *nv=malloc(vc+1);
      if (!nv) return -1;
      if (field->v) free(field->v);
      field->v=nv;
    }
    memcpy(field->v,v,vc);
    field->v[vc]=0;
    field->vc=vc;
    
  } else {
    int p=field-eggrt.storev;
    eggrt_store_field_cleanup(field);
    eggrt.storec--;
    memmove(field,field+1,sizeof(struct eggrt_store_field)*(eggrt.storec-p));
  }
  eggrt.store_dirty=1;
  return 0;
}
