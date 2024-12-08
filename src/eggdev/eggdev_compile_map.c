#include "eggdev_internal.h"

/* Binary from text.
 */
 
static int eggdev_map_bin_from_text(struct sr_encoder *dst,const char *src,int srcc,const char *path) {
  struct sr_decoder decoder={.v=src,.c=srcc};
  const char *line;
  int linec,lineno=1;
  
  /* First line establishes the width.
   */
  linec=sr_decode_line(&line,&decoder);
  while (linec&&((unsigned char)line[linec-1]<=0x20)) linec--;
  while (linec&&((unsigned char)line[0]<=0x20)) { line++; linec--; }
  if ((linec<2)||(linec&1)) {
    fprintf(stderr,"%s:%d: Expected an even count of hex digits at least 2. Found %d bytes.\n",path,lineno,linec);
    return -2;
  }
  int w=linec>>1,h=0;
  if (sr_encode_raw(dst,"\0EMP",4)<0) return -1;
  if (sr_encode_intbe(dst,linec>>1,2)<0) return -1;
  int hp=dst->c; // Return here with height once known.
  if (sr_encode_raw(dst,"\0\0",2)<0) return -1;
  
  /* Unread that first line, and read the entire cells image.
   */
  decoder.p=0;
  for (;(linec=sr_decode_line(&line,&decoder))>0;lineno++) {
    while (linec&&((unsigned char)line[linec-1]<=0x20)) linec--;
    while (linec&&((unsigned char)line[0]<=0x20)) { line++; linec--; }
    if (!linec||(line[0]=='#')) { lineno++; break; } // First empty or comment line ends the cells image.
    if (linec!=w<<1) {
      fprintf(stderr,"%s:%d: Expected %d hex digits, found %d bytes.\n",path,lineno,w<<1,linec);
      return -2;
    }
    int linep=0;
    for (;linep<linec;) {
      int hi=sr_digit_eval(line[linep++]);
      int lo=sr_digit_eval(line[linep++]);
      if ((hi<0)||(lo<0)||(hi>15)||(lo>15)) {
        fprintf(stderr,"%s:%d: Expected hexadecimal byte, found '%.2s'\n",path,lineno,line+linep-2);
        return -2;
      }
      if (sr_encode_u8(dst,(hi<<4)|lo)<0) return -1;
    }
    h++;
  }
  
  /* Fill in height.
   */
  if ((w<0)||(h<0)||(w>0xffff)||(h>0xffff)) return -1;
  ((uint8_t*)(dst->v))[hp++]=h>>8;
  ((uint8_t*)(dst->v))[hp]=h;
  
  /* Compile commands.
   */
  const struct eggdev_ns *ns=eggdev_ns_by_tid(EGG_TID_map);
  return eggdev_command_list_compile(dst,src+decoder.p,srcc-decoder.p,path,lineno,ns);
}

/* Text from binary.
 */
 
static int eggdev_map_text_from_bin(struct sr_encoder *dst,const uint8_t *src,int srcc,const char *path) {
  if (!src||(srcc<8)||memcmp(src,"\0EMP",4)) return -1;
  int srcp=4,err;
  int w=(src[srcp]<<8)|src[srcp+1]; srcp+=2;
  int h=(src[srcp]<<8)|src[srcp+1]; srcp+=2;
  int cellc=w*h;
  if (srcp>srcc-cellc) return -1;
  int yi=h; for (;yi-->0;) {
    int xi=w; for (;xi-->0;srcp++) {
      if (sr_encode_u8(dst,"0123456789abcdef"[src[srcp]>>4])<0) return -1;
      if (sr_encode_u8(dst,"0123456789abcdef"[src[srcp]&15])<0) return -1;
    }
    if (sr_encode_u8(dst,0x0a)<0) return -1;
  }
  if (sr_encode_u8(dst,0x0a)<0) return -1;
  if (srcp<srcc) {
    const struct eggdev_ns *ns=eggdev_ns_by_tid(EGG_TID_map);
    if ((err=eggdev_command_list_uncompile(dst,src+srcp,srcc-srcp,path,ns))<0) return err;
  }
  return 0;
}

/* Map resource, main entry point.
 */
 
int eggdev_compile_map(struct eggdev_res *res) {
  if ((res->serialc>=4)&&!memcmp(res->serial,"\0EMP",4)) return 0;
  struct sr_encoder dst={0};
  if (eggdev_map_bin_from_text(&dst,res->serial,res->serialc,res->path)>=0) {
    eggdev_res_handoff_serial(res,dst.v,dst.c);
    return 0;
  }
  sr_encoder_cleanup(&dst);
  fprintf(stderr,"%s: Unable to detect map format.\n",res->path);
  return -2;
}

int eggdev_uncompile_map(struct eggdev_res *res) {
  int err;
  if ((res->serialc>=4)&&!memcmp(res->serial,"\0EMP",4)) {
    struct sr_encoder dst={0};
    if ((err=eggdev_map_text_from_bin(&dst,res->serial,res->serialc,res->path))<0) {
      if (err!=-2) fprintf(stderr,"%s: Failed to uncompile map.\n",res->path);
      sr_encoder_cleanup(&dst);
      return -2;
    }
    eggdev_res_handoff_serial(res,dst.v,dst.c);
    return 0;
  }
  // Assume that anything else is already in text format.
  return 0;
}
