#include "eggrt_internal.h"

#define EMBEDDED 1
#define EXTERNAL 2

#if ROMSRC==EMBEDDED
  const int eggrt_has_embedded_rom=1;
  extern const unsigned char egg_embedded_rom[];
  extern const int egg_embedded_rom_size;

#elif ROMSRC==EXTERNAL
  const int eggrt_has_embedded_rom=0;
  #include "opt/fs/fs.h"
  
#else
  #error "Please build with -DROMSRC=EMBEDDED or -DROMSRC=EXTERNAL"
#endif

/* Quit.
 */
 
void eggrt_romsrc_quit() {
  if (eggrt.resv) free(eggrt.resv);
  eggrt.resv=0;
  eggrt.resc=eggrt.resa=0;
  #if ROMSRC==EXTERNAL
    if (eggrt.romserial) free((void*)eggrt.romserial);
  #endif
  eggrt.romserial=0;
  eggrt.romserialc=0;
}

/* Init.
 */
 
int eggrt_romsrc_init() {

  // Acquire the encoded ROM.
  #if ROMSRC==EMBEDDED
    eggrt.romserial=egg_embedded_rom;
    eggrt.romserialc=egg_embedded_rom_size;
  #elif ROMSRC==EXTERNAL
    if (!eggrt.rompath) {
      fprintf(stderr,"%s: ROM required.\n",eggrt.exename);
      return -2;
    }
    if ((eggrt.romserialc=file_read(&eggrt.romserial,eggrt.rompath))<0) {
      eggrt.romserialc=0;
      fprintf(stderr,"%s: Failed to read file.\n",eggrt.rompath);
      return -2;
    }
  #endif
  
  // Split into a handy TOC.
  eggrt.resc=0;
  struct rom_reader reader;
  if (rom_reader_init(&reader,eggrt.romserial,eggrt.romserialc)<0) {
    fprintf(stderr,"%s: Invalid ROM, signature mismatch.\n",eggrt.rptname);
    return -2;
  }
  struct rom_res *res;
  while (res=rom_reader_next(&reader)) {
    if (eggrt.resc>=eggrt.resa) {
      int na=eggrt.resa+256;
      if (na>INT_MAX/sizeof(struct rom_res)) return -1;
      void *nv=realloc(eggrt.resv,sizeof(struct rom_res)*na);
      if (!nv) return -1;
      eggrt.resv=nv;
      eggrt.resa=na;
    }
    memcpy(eggrt.resv+eggrt.resc,res,sizeof(struct rom_res));
    eggrt.resc++;
  }
  
  // Confirm that metadata:1 is first. Elsewhere in eggrt, we will assume that that is so.
  if ((eggrt.resc<1)||(eggrt.resv->tid!=EGG_TID_metadata)||(eggrt.resv->rid!=1)) {
    fprintf(stderr,"%s: ROM does not begin with metadata:1 as required.\n",eggrt.rptname);
    return -2;
  }
  
  //fprintf(stderr,"%s: Acquired %d-byte ROM with %d resources.\n",eggrt.rptname,eggrt.romserialc,eggrt.resc);

  return 0;
}

/* Get resource.
 */
 
int eggrt_rom_get(void *dstpp,int tid,int rid) {
  int lo=0,hi=eggrt.resc;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
    const struct rom_res *res=eggrt.resv+ck;
         if (tid<res->tid) hi=ck;
    else if (tid>res->tid) lo=ck+1;
    else if (rid<res->rid) hi=ck;
    else if (rid>res->rid) lo=ck+1;
    else {
      *(const void**)dstpp=res->v;
      return res->c;
    }
  }
  return 0;
}
