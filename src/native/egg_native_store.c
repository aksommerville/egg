#include "egg_native_internal.h"
#include "opt/serial/serial.h"
#include "opt/fs/fs.h"

/* Search by key.
 */
 
static int egg_native_store_search(const char *k,int kc) {
  int lo=0,hi=egg.storec;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
    const struct egg_store_entry *entry=egg.storev+ck;
         if (kc<entry->kc) hi=ck;
    else if (kc>entry->kc) lo=ck+1;
    else {
      int cmp=memcmp(k,entry->k,kc);
           if (cmp<0) hi=ck;
      else if (cmp>0) lo=ck+1;
      else return ck;
    }
  }
  return -lo-1;
}

/* Get from in-memory store.
 */
 
static int egg_native_store_get(void *vpp,const char *k,int kc) {
  int p=egg_native_store_search(k,kc);
  if (p<0) return 0;
  const struct egg_store_entry *entry=egg.storev+p;
  *(void**)vpp=entry->v;
  return entry->vc;
}

/* Insert to in-memory store.
 */
 
static int egg_native_store_insert(int p,const char *k,int kc,const void *v,int vc) {
  if ((p<0)||(p>egg.storec)||!k||(kc<1)||!v||(vc<1)) return -1;
  if (egg.storec>=egg.storea) {
    int na=egg.storea+16;
    if (na>INT_MAX/sizeof(struct egg_store_entry)) return -1;
    void *nv=realloc(egg.storev,sizeof(struct egg_store_entry)*na);
    if (!nv) return -1;
    egg.storev=nv;
    egg.storea=na;
  }
  char *nk=malloc(kc);
  if (!nk) return -1;
  void *nv=malloc(vc);
  if (!nv) { free(nk); return -1; }
  memcpy(nk,k,kc);
  memcpy(nv,v,vc);
  struct egg_store_entry *entry=egg.storev+p;
  memmove(entry+1,entry,sizeof(struct egg_store_entry)*(egg.storec-p));
  egg.storec++;
  entry->k=nk;
  entry->kc=kc;
  entry->v=nv;
  entry->vc=vc;
  return 0;
}

/* Replace value in memory.
 */
 
static int egg_native_store_replace(int p,const void *v,int vc) {
  if ((p<0)||(p>=egg.storec)||!v||(vc<1)) return -1;
  struct egg_store_entry *entry=egg.storev+p;
  void *nv=malloc(vc);
  if (!nv) return -1;
  memcpy(nv,v,vc);
  if (entry->v) free(entry->v);
  entry->v=nv;
  entry->vc=vc;
  return 0;
}

/* Remove entry from store.
 */
 
static void egg_native_store_remove(int p) {
  if ((p<0)||(p>=egg.storec)) return;
  struct egg_store_entry *entry=egg.storev+p;
  if (entry->k) free(entry->k);
  if (entry->v) free(entry->v);
  egg.storec--;
  memmove(entry,entry+1,sizeof(struct egg_store_entry)*(egg.storec-p));
}

/* Set value in in-memory store.
 */
 
static int egg_native_store_set(const char *k,int kc,const void *v,int vc) {
  int p=egg_native_store_search(k,kc);
  if (p<0) {
    if (!vc) return 0;
    if (egg_native_store_insert(-p-1,k,kc,v,vc)<0) return -1;
  } else if (!vc) {
    egg_native_store_remove(p);
  } else {
    if (egg_native_store_replace(p,v,vc)<0) return -1;
  }
  egg.store_dirty=1;
  return 0;
}

/* Validate key.
 */
 
static int egg_store_key_valid(const char *k,int kc) {
  if (!k) return 0;
  if ((kc<1)||(kc>32)) return 0;
  if ((k[0]>='0')&&(k[0]<='9')) return 0;
  for (;kc-->0;k++) {
    if ((*k>='a')&&(*k<='z')) continue;
    if ((*k>='A')&&(*k<='Z')) continue;
    if ((*k>='0')&&(*k<='9')) continue;
    if (*k=='_') continue;
    return 0;
  }
  return 1;
}

/* Load store initially, from egg.storepath.
 */
 
static void egg_native_store_load() {
  uint8_t *serial=0;
  int serialc=file_read(&serial,egg.storepath);
  if (serialc<0) return; // doesn't exist or can't read, that's fine
  int serialp=0;
  while (serialp<serialc) {
    int kc=serial[serialp++];
    if (serialp>serialc-kc) break;
    const char *k=(char*)serial+serialp;
    serialp+=kc;
    
    int vc=0,seqlen;
    if ((seqlen=sr_vlq_decode(&vc,serial+serialp,serialc-serialp))<1) break;
    serialp+=seqlen;
    if (serialp>serialc-vc) break;
    if (egg_store_key_valid(k,kc)) {
      egg_native_store_set(k,kc,serial+serialp,vc);
    }
    serialp+=vc;
  }
  free(serial);
  egg.store_dirty=0;
}

/* Save if dirty.
 */
 
static int egg_native_store_encode(struct sr_encoder *dst) {
  const struct egg_store_entry *entry=egg.storev;
  int i=egg.storec;
  for (;i-->0;entry++) {
    if (sr_encode_u8(dst,entry->kc)<0) return -1;
    if (sr_encode_raw(dst,entry->k,entry->kc)<0) return -1;
    if (sr_encode_vlqlen(dst,entry->v,entry->vc)<0) return -1;
  }
  return 0;
}
 
int egg_native_store_update() {
  if (!egg.store_dirty) return 0;
  egg.store_dirty=0;
  if (!egg.storepath||!egg.storepath[0]) return 0;
  struct sr_encoder serial={0};
  if (egg_native_store_encode(&serial)>=0) {
    if (file_write(egg.storepath,serial.v,serial.c)<0) {
      fprintf(stderr,"%s: Failed to save game.\n",egg.storepath);
    }
  } else {
    fprintf(stderr,"%s: Failed to encode game save data.\n",egg.exename);
  }
  sr_encoder_cleanup(&serial);
}

/* Populate egg.storepath, it is currently null.
 * If we determine there should be no store, populate it with an empty string.
 */
 
static void egg_native_acquire_storepath() {
  
  /* Fetch the title and aggressively sanitize it:
   *  - Limit 32 bytes.
   *  - Space, dash, and underscore become dash.
   *  - Digits preserve.
   *  - G0 letters become lowercase.
   *  - Drop leading and trailing dashes.
   *  - Drop leading "the-", "a-", "an-".
   *  - Everything else gets erased.
   * This should yield a token recognizable to the user but also safe for file names.
   * It's OK if it ends up empty.
   */
  char title[256];
  int titlec=egg_native_rom_get_detail(title,sizeof(title),"title",5);
  if ((titlec<0)||(titlec>sizeof(title))) titlec=0;
  int i=titlec; while (i-->0) {
    char ch=title[i];
         if ((ch==' ')||(ch=='-')||(ch=='_')) ch='-';
    else if ((ch>='a')&&(ch<='z')) ;
    else if ((ch>='0')&&(ch<='9')) ;
    else if ((ch>='A')&&(ch<='Z')) ch+=0x20;
    else ch=0;
    title[i]=ch;
  }
  char ttoken[32];
  int ttokenc=0;
  for (i=0;i<titlec;i++) {
    if (!title[i]) continue;
    if (title[i]=='-') {
      if (!ttokenc) continue;
      if (ttoken[ttokenc-1]=='-') continue;
    }
    ttoken[ttokenc++]=title[i];
         if ((ttokenc==2)&&!memcmp(ttoken,"a-",2)) ttokenc=0;
    else if ((ttokenc==3)&&!memcmp(ttoken,"an-",3)) ttokenc=0;
    else if ((ttokenc==4)&&!memcmp(ttoken,"the-",4)) ttokenc=0;
    if (ttokenc>=sizeof(ttoken)) break;
  }
  while (ttokenc&&(ttoken[ttokenc-1]=='-')) ttokenc--;
  while (ttokenc&&(ttoken[0]=='-')) { ttokenc--; memmove(ttoken,ttoken+1,ttokenc); }
  
  /* Basename starts with the ROM hash hexadecimal.
   * But 40 bytes feels excessive, so XOR it against itself to a smaller size.
   */
  uint8_t hash[10]={0};
  int hashc=0;
  for (i=0;i<sizeof(egg.romhash);i++) {
    hash[hashc]^=egg.romhash[i];
    hashc++;
    if (hashc>=sizeof(hash)) hashc=0;
  }
  char hexhash[20];
  for (i=0;i<sizeof(hash);i++) {
    hexhash[(i<<1)  ]="0123456789abcdef"[hash[i]>>4];
    hexhash[(i<<1)+1]="0123456789abcdef"[hash[i]&15];
  }
  
  /* Save files will go in a directory called "save" adjacent to the executable.
   * TODO Directory for save files must be configurable, and we should probably add some platform-dependent defaults.
   * TODO Also, "adjacent to the executable" is a very bad choice for MacOS; that's inside the application bundle.
   * TODO Also, the path separator under MS Windows is not '/'.
   * eg ~/.egg/save, /usr/local/share/egg/save, ?
   */
  int exenamec=0;
  while (egg.exename[exenamec]) exenamec++;
  while (exenamec&&(egg.exename[exenamec-1]!='/')) exenamec--;
  int pathc=exenamec+sizeof(hexhash)+1+ttokenc+4; // PFX HASH - TITLE .sav
  if (egg.storepath=malloc(pathc+1)) {
    memcpy(egg.storepath,egg.exename,exenamec);
    memcpy(egg.storepath+exenamec,hexhash,sizeof(hexhash));
    egg.storepath[exenamec+sizeof(hexhash)]='-';
    memcpy(egg.storepath+exenamec+sizeof(hexhash)+1,ttoken,ttokenc);
    memcpy(egg.storepath+exenamec+sizeof(hexhash)+1+ttokenc,".sav",5); // sic 5 not 4, include the terminator
    
    // Finally, make that directory if it doesn't exist.
    if (dir_mkdirp_parent(egg.storepath)<0) {
      egg.storepath[0]=0;
    }
  } else {
    egg.storepath=calloc(1,1);
  }
}

/* Load the store if we haven't yet.
 * If this fails, short-circuit fail egg_store_get and egg_store_set.
 * Do not fake the store in memory, even though we easily could.
 * That would create the false impression that data is being persisted.
 * No need to lie about it.
 */
 
static int egg_native_store_require() {
  if (egg.storepath&&egg.storepath[0]) return 0; // Yup we're running already.
  if (!egg.storepath) egg_native_acquire_storepath();
  if (!egg.storepath) return -1;
  if (!egg.storepath[0]) return -1;
  egg_native_store_load();
  return 0; // Even if load fails.
}

/* Store public API.
 */

int egg_store_get(char *v,int va,const char *k,int kc) {
  if (!k) kc=0; else if (kc<0) { kc=0; while (k[kc]) kc++; }
  if (!v||(va<0)) va=0;
  if (!egg_store_key_valid(k,kc)) return 0;
  if (egg_native_store_require()<0) return 0;
  const void *src=0;
  int srcc=egg_native_store_get(&src,k,kc);
  if (srcc<1) return 0;
  if (srcc<=va) {
    memcpy(v,src,srcc);
    if (srcc<va) v[srcc]=0;
  }
  return srcc;
}

int egg_store_set(const char *k,int kc,const char *v,int vc) {
  if (!k) kc=0; else if (kc<0) { kc=0; while (k[kc]) kc++; }
  if (!v) vc=0; else if (vc<0) { vc=0; while (v[vc]) vc++; }
  if (!egg_store_key_valid(k,kc)) return -1;
  if (egg_native_store_require()<0) return -1;
  return egg_native_store_set(k,kc,v,vc);
}

int egg_store_key_by_index(char *k,int ka,int p) {
  if (p<0) return 0;
  if (p>=egg.storec) return 0;
  const struct egg_store_entry *entry=egg.storev+p;
  if (k&&(entry->kc<=ka)) {
    memcpy(k,entry->k,entry->kc);
    if (entry->kc<ka) k[entry->kc]=0;
  }
  return entry->kc;
}

/* Public access to resources.
 */

int egg_res_copy(void *dst,int dsta,const char *path) {
  if (!dst) dsta=0;
  const void *src=0;
  int srcc=arr_get(&src,&egg.arr,path,-1);
  if (srcc<=dsta) memcpy(dst,src,srcc);
  return srcc;
}

int egg_res_path_by_index(char *path,int patha,int p) {
  if (!path||(patha<0)) patha=0;
  int srcc=0;
  if ((p>=0)&&(p<egg.arr.resc)) {
    const char *src=egg.arr.resv[p].path;
    srcc=egg.arr.resv[p].pathc;
    if (srcc<=patha) {
      memcpy(path,src,srcc);
    }
  }
  if (srcc<patha) path[srcc]=0;
  return srcc;
}
