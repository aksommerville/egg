#include "archive_internal.h"

/* Process details.
 * Safe to add and remove resources -- if there's an "icon.ico", we yoink and remove it.
 * For the most part, we copy key and value verbatim from the source file.
 */
 
int res_details_process(struct arw_res *res) {
  struct sr_encoder dst={0};
  struct sr_decoder src={.v=res->serial,.c=res->serialc};
  int lineno=0,linec;
  const char *line;
  while ((linec=sr_decode_line(&line,&src))>0) {
    lineno++;
    while (linec&&((unsigned char)line[linec-1]<=0x20)) linec--;
    while (linec&&((unsigned char)line[0]<=0x20)) { line++; linec--; }
    if (!linec) continue;
    if (line[0]=='#') continue;
    
    const char *k=line;
    int kc=0,linep=0;
    while ((linep<linec)&&(line[linep]!=':')&&(line[linep]!='=')) { linep++; kc++; }
    while (kc&&((unsigned char)k[kc-1]<=0x20)) kc--;
    const char *v=0;
    int vc=0;
    if (linep<linec) {
      linep++;
      while ((linep<linec)&&((unsigned char)line[linep]<=0x20)) linep++;
      v=line+linep;
      vc=linec-linep;
    }
    
    if ((kc==4)&&!memcmp(k,"icon",4)) {
      fprintf(stderr,"%.*s:%d: Icon must be supplied as a separate file 'icon.ico'\n",res->pathc,res->path,lineno);
      sr_encoder_cleanup(&dst);
      return -2;
    }
    
    if ((kc<1)||(kc>0xff)) {
      fprintf(stderr,"%.*s:%d: Invalid key.\n",res->pathc,res->path,lineno);
      sr_encoder_cleanup(&dst);
      return -2;
    }
    if (vc>=0x10000000) {
      fprintf(stderr,"%.*s:%d: Invalid length %d. Wait, what?\n",res->pathc,res->path,lineno,vc);
      sr_encoder_cleanup(&dst);
      return -2;
    }
    if (
      (sr_encode_intbelen(&dst,k,kc,1)<0)||
      (sr_encode_vlqlen(&dst,v,vc)<0)
    ) {
      sr_encoder_cleanup(&dst);
      return -1;
    }
  }
  
  // If there's an icon file, add that too.
  struct arw_res *icon=arw_find(&archive.arw,"icon.ico",8);
  if (icon) {
    if (
      (sr_encode_raw(&dst,"\x04icon",5)<0)||
      (sr_encode_vlqlen(&dst,icon->serial,icon->serialc)<0)
    ) {
      sr_encoder_cleanup(&dst);
      return -1;
    }
  }
  
  if (arw_replace_handoff(&archive.arw,res->path,res->pathc,dst.v,dst.c)<0) {
    sr_encoder_cleanup(&dst);
    return -1;
  }
  dst.v=0; // handoff to arw
  if (icon) arw_remove(&archive.arw,"icon.ico",8);
  return 0;
}
