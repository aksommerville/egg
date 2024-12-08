#include "eggdev_internal.h"

/* Guess MIME type.
 */
 
const char *eggdev_guess_mime_type(const char *path,const void *src,int srcc) {

  /* If it ends with a known suffix, case-insensitively, trust it.
   */
  if (path) {
    const char *pre=0;
    int prec=0,pathc=0;
    for (;path[pathc];pathc++) {
      if (path[pathc]=='/') {
        pre=0;
        prec=0;
      } else if (path[pathc]=='.') {
        pre=path+pathc+1;
        prec=0;
      } else if (pre) {
        prec++;
      }
    }
    char sfx[16];
    if ((prec>0)&&(prec<=sizeof(sfx))) {
      int i=prec; while (i-->0) {
        if ((pre[i]>='A')&&(pre[i]<='Z')) sfx[i]=pre[i]+0x20;
        else sfx[i]=pre[i];
      }
      switch (prec) {
        case 2: {
            if (!memcmp(sfx,"js",2)) return "application/javascript";
          } break;
        case 3: {
            if (!memcmp(sfx,"css",3)) return "text/css";
            if (!memcmp(sfx,"png",3)) return "image/png";
            if (!memcmp(sfx,"gif",3)) return "image/gif";
            if (!memcmp(sfx,"ico",3)) return "image/x-icon";
            if (!memcmp(sfx,"mid",3)) return "audio/midi";
            if (!memcmp(sfx,"wav",3)) return "audio/wave";
            if (!memcmp(sfx,"egg",3)) return "application/x-egg-rom";
            if (!memcmp(sfx,"htm",3)) return "text/html";
            if (!memcmp(sfx,"jpg",3)) return "image/jpeg";
          } break;
        case 4: {
            if (!memcmp(sfx,"html",4)) return "text/html";
            if (!memcmp(sfx,"jpeg",4)) return "image/jpeg";
            if (!memcmp(sfx,"wasm",4)) return "application/wasm";
          } break;
      }
    }
  }
  
  /* Check for unambiguous signatures.
   */
  if ((srcc>=8)&&!memcmp(src,"\x89PNG\r\n\x1a\n",8)) return "image/png";
  if ((srcc>=4)&&!memcmp(src,"\0EGG",4)) return "application/x-egg-rom";
  if ((srcc>=4)&&!memcmp(src,"\0asm",4)) return "application/wasm";
  
  /* Check for ambiguous signatures.
   */
  if ((srcc>=6)&&!memcmp(src,"GIF87a",6)) return "image/gif";
  if ((srcc>=6)&&!memcmp(src,"GIF89a",6)) return "image/gif";
  if ((srcc>=14)&&!memcmp(src,"<!DOCTYPE html",14)) return "text/html";
  if ((srcc>=5)&&!memcmp(src,"<?xml",5)) return "application/xml";

  /* Finally, if the first 256 bytes are UTF-8 with no nulls, call it "text/plain".
   * Otherwise "application/octet-stream".
   * The edge case of empty input is "application/octet-stream".
   */
  if (!srcc) return "application/octet-stream";
  const uint8_t *SRC=src;
  int ckc=(srcc>256)?256:srcc;
  int srcp=0;
  while (srcp<ckc) {
    if (!SRC[srcp]) return "application/octet-stream";
    if (SRC[srcp]<0x80) { srcp++; continue; } // Don't call out to UTF-8 decoder in this extremely common case.
    int seqlen,codepoint;
    if ((seqlen=sr_utf8_decode(&codepoint,SRC+srcp,ckc-srcp))<1) {
      if (srcp>252) break; // Might have failed due to our length limit, let it slide.
      return "application/octet-stream";
    }
    srcp+=seqlen;
  }
  return "text/plain";
}

/* Count newlines.
 */
 
int eggdev_lineno(const char *src,int srcc) {
  if (!src) return 1;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  int srcp=0,lineno=1;
  while (srcp<srcc) {
    if (src[srcp++]==0x0a) lineno++;
  }
  return lineno;
}

/* Get string from resource.
 */
 
int eggdev_strings_get(void *dstpp,int rid,int index) {
  if (index<0) return 0;
  if (!eggdev.rom) return 0;
  int resp=eggdev_rom_search(eggdev.rom,EGG_TID_strings,rid);
  if (resp<0) return 0;
  const struct eggdev_res *res=eggdev.rom->resv+resp;
  const uint8_t *src=res->serial;
  int srcc=res->serialc;
  if ((srcc<4)||memcmp(src,"\0ES\xff",4)) return 0;
  int srcp=4;
  while (srcp<srcc) {
    int len=src[srcp++];
    if (len&0x80) {
      if (srcp>=srcc) return 0;
      len&=0x7f;
      len<<=8;
      len|=src[srcp++];
    }
    if (srcp>srcc-len) return 0;
    if (!index--) {
      *(const void**)dstpp=src+srcp;
      return len;
    }
    srcp+=len;
  }
  return 0;
}

/* Hex dump to stderr.
 */
 
void eggdev_hexdump(const void *src,int srcc) {
  if ((srcc<0)||(srcc&&!src)) srcc=0;
  int srcp=0,rowlen=16;
  for (;srcp<srcc;srcp+=rowlen) {
    fprintf(stderr,"%08x |",srcp);
    int i=0; for (;i<rowlen;i++) {
      int p=srcp+i;
      if (p>=srcc) break;
      fprintf(stderr," %02x",((uint8_t*)src)[p]);
    }
    fprintf(stderr,"\n");
  }
  fprintf(stderr,"%08x\n",srcc);
}

/* Normalize path suffix.
 */
 
int eggdev_normalize_suffix(char *dst,int dsta,const char *src,int srcc) {
  if (!dst||(dsta<1)) return 0;
  if (!src) return 0;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  const char *sfx=src+srcc;
  int sfxc=0;
  while (sfxc<srcc) {
    if (sfx[-1]=='.') break;
    if (sfx[-1]=='/') return 0;
    sfx--;
    sfxc++;
  }
  if (sfxc>dsta) return 0;
  int i=sfxc;
  while (i-->0) {
    char ch=sfx[i];
    if ((ch>='A')&&(ch<='Z')) dst[i]=ch+0x20;
    else dst[i]=ch;
  }
  if (sfxc<dsta) dst[sfxc]=0;
  return sfxc;
}
