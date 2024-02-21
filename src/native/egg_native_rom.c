#include "egg_native_internal.h"
#include "opt/fs/fs.h"
#include "opt/serial/serial.h"

#define NATIVE 1
#define BUNDLED 2
#define EXTERNAL 3

#if ( \
  (EGG_NATIVE_ROM_MODE!=NATIVE)&& \
  (EGG_NATIVE_ROM_MODE!=BUNDLED)&& \
  (EGG_NATIVE_ROM_MODE!=EXTERNAL) \
)
  #error "Please define EGG_NATIVE_ROM_MODE as one of: NATIVE BUNDLED EXTERNAL"
#endif

#if EGG_NATIVE_ROM_MODE==NATIVE || EGG_NATIVE_ROM_MODE==BUNDLED
  extern const char egg_bundled_rom[];
  extern const int egg_bundled_rom_size;
#endif

/* Exposure of compile-time properties.
 */
 
int egg_native_rom_expects_file() {
  #if EGG_NATIVE_ROM_MODE==NATIVE
    return 0;
  #elif EGG_NATIVE_ROM_MODE==BUNDLED
    return 0;
  #elif EGG_NATIVE_ROM_MODE==EXTERNAL
    return 1;
  #endif
}

int egg_native_rom_is_true_native() {
  #if EGG_NATIVE_ROM_MODE==NATIVE
    return 1;
  #elif EGG_NATIVE_ROM_MODE==BUNDLED
    return 0;
  #elif EGG_NATIVE_ROM_MODE==EXTERNAL
    return 0;
  #endif
}

/* Cleanup.
 */
 
void egg_native_rom_cleanup() {
  arr_cleanup(&egg.arr);
  if (egg.rom) free(egg.rom);
}

/* Load archive.
 * Caller must guarantee that (src) will remain constant for the program's life.
 * If it's not statically linked into this executable, it should be cached as (egg.rom).
 */
 
static int egg_native_rom_load_archive(const void *src,int srcc,const char *path) {
  if (arr_init(&egg.arr,src,srcc)<0) return -1;
  //TODO NATIVE and BUNDLED, we could store the precalculated hash, it's constant.
  int hashc=sr_sha1(egg.romhash,sizeof(egg.romhash),src,srcc);
  if (hashc!=sizeof(egg.romhash)) {
    fprintf(stderr,"%s: Expected %d-byte hash, got %d\n",__func__,(int)sizeof(egg.romhash),hashc);
    return -2;
  }
  return 0;
}

/* Assert that the loaded archive contains expected client entry points.
 * Record them for future use.
 */

#if EGG_NATIVE_ROM_MODE!=NATIVE
static int egg_native_rom_require_eggxecutables() {
  fprintf(stderr,"%s\n",__func__);//TODO
  return 0;
}
#endif

/* Initialize.
 */
 
int egg_native_rom_init() {

  #if EGG_NATIVE_ROM_MODE==NATIVE || EGG_NATIVE_ROM_MODE==BUNDLED
    int err=egg_native_rom_load_archive(egg_bundled_rom,egg_bundled_rom_size,"<builtin>");
    if (err<0) {
      if (err!=-2) fprintf(stderr,"%s: Unspecified error loading built-in ROM file.\n",egg.exename);
      return -2;
    }
    #if EGG_NATIVE_ROM_MODE==BUNDLED
      return egg_native_rom_require_eggxecutables();
    #else
      return 0;
    #endif
    
  #elif EGG_NATIVE_ROM_MODE==EXTERNAL
    if (!egg.rompath) {
      fprintf(stderr,"%s: ROM file required.\n",egg.exename);
      return -2;
    }
    void *serial=0;
    int serialc=file_read(&serial,egg.rompath);
    if (serialc<0) {
      fprintf(stderr,"%s: Failed to read file.\n",egg.rompath);
      return -2;
    }
    int err=egg_native_rom_load_archive(serial,serialc,egg.rompath);
    if (err<0) {
      free(serial);
      if (err!=-2) fprintf(stderr,"%s: Unspecified error loading ROM file.\n",egg.rompath);
      return -2;
    }
    if (egg.rom) free(egg.rom);
    egg.rom=serial;
    return egg_native_rom_require_eggxecutables();
  
  #endif
}

/* Run client bootstrap.
 */
 
int egg_native_rom_start() {

  #if EGG_NATIVE_ROM_MODE==NATIVE
    int err=egg_client_init();
    if (err<0) {
      fprintf(stderr,"%s: Error in game's bootstrap.\n",egg.exename);
      return -2;
    }
    if (!err) {
      egg.terminate=1;
    }
    return 0;
  
  #elif EGG_NATIVE_ROM_MODE==BUNDLED || EGG_NATIVE_ROM_MODE==EXTERNAL
    fprintf(stderr,"%s:%d:TODO: Locate WebAssembly or JavaScript bootstrap, and execute it.\n",__FILE__,__LINE__);
    return 0;

  #endif
}

/* Straight client hooks.
 */
 
int egg_native_rom_update(double elapsed) {

  #if EGG_NATIVE_ROM_MODE==NATIVE
    egg_client_update(elapsed);
    return 0;
    
  #elif EGG_NATIVE_ROM_MODE==BUNDLED || EGG_NATIVE_ROM_MODE==EXTERNAL
    //fprintf(stderr,"%s:%d:TODO: Locate ROM's update hook and call it.\n",__FILE__,__LINE__); TODO
    // well actually, one hopes it's already been located...
    return 0;
    
  #endif
}

void egg_native_rom_render() {

  #if EGG_NATIVE_ROM_MODE==NATIVE
    egg_client_render();
    
  #elif EGG_NATIVE_ROM_MODE==BUNDLED || EGG_NATIVE_ROM_MODE==EXTERNAL
    //TODO
    
  #endif
}

void egg_native_rom_event(const void *event) {

  #if EGG_NATIVE_ROM_MODE==NATIVE
    egg_client_event(event);
    
  #elif EGG_NATIVE_ROM_MODE==BUNDLED || EGG_NATIVE_ROM_MODE==EXTERNAL
    //TODO
    
  #endif
}

/* Call client's farewell hook, if it bootstrapped.
 */
 
void egg_native_rom_farewell() {
  
  #if EGG_NATIVE_ROM_MODE==NATIVE
    egg_client_quit();
    
  #elif EGG_NATIVE_ROM_MODE==BUNDLED || EGG_NATIVE_ROM_MODE==EXTERNAL
    fprintf(stderr,"%s:%d:TODO: Locate ROM's farewell hook and call it.\n",__FILE__,__LINE__);
    
  #endif
}

/* Conveniences to read from the "details" resource.
 * These don't really belong here, just I consider them an extension of the ROM file.
 */
 
void egg_native_rom_get_framebuffer(int *w,int *h) {
  char tmp[32];
  int tmpc=egg_native_rom_get_detail(tmp,sizeof(tmp),"framebuffer",11);
  if ((tmpc<1)||(tmpc>sizeof(tmp))) return;
  int tw=0,tmpp=0;
  while ((tmpp<tmpc)&&(tmp[tmpp]>='0')&&(tmp[tmpp]<='9')) {
    tw*=10;
    tw+=tmp[tmpp++]-'0';
  }
  if ((tmpp>=tmpc)||(tmp[tmpp++]!='x')) return;
  int th=0;
  while ((tmpp<tmpc)&&(tmp[tmpp]>='0')&&(tmp[tmpp]<='9')) {
    th*=10;
    th+=tmp[tmpp++]-'0';
  }
  if (tmpp<tmpc) return;
  *w=tw;
  *h=th;
}

int egg_native_rom_get_storageSize() {
  char tmp[32];
  int tmpc=egg_native_rom_get_detail(tmp,sizeof(tmp),"storageSize",11);
  if ((tmpc<1)||(tmpc>sizeof(tmp))) return 0;
  int n=0;
  if (sr_int_eval(&n,tmp,tmpc)<2) return 0;
  return n;
}

static int egg_details_get(char *dst,int dsta,const uint8_t *src,int srcc,const char *k,int kc) {
  int srcp=0,dstc=0;
  while (srcp<srcc) {
    int fkc=src[srcp++];
    if (srcp>srcc-fkc) break;
    const uint8_t *fk=src+srcp;
    srcp+=fkc;
    int fvc,seqlen;
    if ((seqlen=sr_vlq_decode(&fvc,src+srcp,srcc-srcp))<1) break;
    srcp+=seqlen;
    if (srcp>srcc-fvc) break;
    const uint8_t *fv=src+srcp;
    srcp+=fvc;
    if ((fkc==kc)&&!memcmp(fk,k,kc)) {
      if (fvc<=dsta) memcpy(dst,fv,fvc);
      else memcpy(dst,fv,dsta);
      dstc=fvc;
      break;
    }
  }
  if (dstc<dsta) dst[dstc]=0;
  return dstc;
}

int egg_native_rom_get_detail(char *dst,int dsta,const char *k,int kc) {
  if (!dst||(dsta<0)) dsta=0;
  if (!k) kc=0; else if (kc<0) { kc=0; while (k[kc]) kc++; }
  const uint8_t *src=0;
  int srcc=arr_get(&src,&egg.arr,"details",7);
  if (srcc>0) return egg_details_get(dst,dsta,src,srcc,k,kc);
  if (dsta>0) dst[0]=0;
  return 0;
}
