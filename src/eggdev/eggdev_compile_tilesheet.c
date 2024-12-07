#include "eggdev_internal.h"

/* Binary from text.
 */
 
static int eggdev_tilesheet_bin_from_text(struct sr_encoder *dst,const char *src,int srcc,const char *path,struct eggdev_rom *rom) {
  if (sr_encode_raw(dst,"\0ETS",4)<0) return -1;
  struct sr_decoder decoder={.v=src,.c=srcc};
  const char *line;
  int linec,lineno=1,tbid=-1,tmpp=0;
  uint8_t tmp[256];
  const char *tbname=0;
  int tbnamec=0;
  for (;(linec=sr_decode_line(&line,&decoder))>0;lineno++) {
    while (linec&&((unsigned char)line[linec-1]<=0x20)) linec--;
    while (linec&&((unsigned char)line[0]<=0x20)) { linec--; line++; }
    if (!linec||(line[0]=='#')) continue;
    
    /* If we're outside a table, the entire line is the name of the upcoming table.
     */
    if (tbid<0) {
      tbname=line;
      tbnamec=linec;
      if ((sr_int_eval(&tbid,line,linec)>=2)&&(tbid>=0)&&(tbid<=0xff)) {
        // Numeric is cool.
      } else {
        int err=eggdev_namespace_lookup(&tbid,"tilesheet",9,line,linec,rom);
        if (err<0) {
          fprintf(stderr,"%s:%d: Unknown tilesheet table '%.*s'\n",path,lineno,linec,line);
          return -2;
        }
      }
      tmpp=0;
      continue;
    }
    
    /* Expect 32 hex digits, decode them into (tmp).
     */
    if (linec!=32) {
      fprintf(stderr,"%s:%d: Expected 32 hex digits for row %d of table '%.*s', found %d bytes.\n",path,lineno,tmpp>>4,tbnamec,tbname,linec);
      return -2;
    }
    int i=0; for (;i<linec;) {
      int hi=sr_digit_eval(line[i++]);
      int lo=sr_digit_eval(line[i++]);
      if ((hi<0)||(hi>0xf)||(lo<0)||(lo>0xf)) {
        fprintf(stderr,"%s:%d: Expected hexadecimal byte, found '%.2s'\n",path,lineno,line+i-2);
        return -2;
      }
      tmp[tmpp++]=(hi<<4)|lo;
    }
    
    /* Did we complete the table?
     * Emit in chunks that don't begin or end with a zero, and don't contain more than 3 consecutive zeroes.
     * If our table ID is zero, skip it even if there is content.
     * Likewise, a table containing all zeroes will emit nothing.
     */
    if (tmpp>=0x100) {
      if (tbid) {
        int p=0;
        while (p<0x100) {
          if (!tmp[p]) { p++; continue; }
          int c=1;
          while (p+c<0x100) {
            if (tmp[p+c]) { c++; continue; }
            if (p+c+4>=0x100) { c=0x100-p; break; }
            if (!memcmp(tmp+p+c,"\0\0\0\0",4)) break;
            c++;
          }
          while (c&&!tmp[p+c-1]) c--;
          if (c) {
            if (
              (sr_encode_u8(dst,tbid)<0)||
              (sr_encode_u8(dst,p)<0)||
              (sr_encode_u8(dst,c-1)<0)||
              (sr_encode_raw(dst,tmp+p,c)<0)
            ) return -1;
          }
          p+=c;
        }
      }
      tbid=-1;
      tmpp=0;
    }
  }
  if (tbid>=0) {
    fprintf(stderr,"%s: Table '%.*s' was not finished\n",path,tbnamec,tbname);
    return -2;
  }
  return 0;
}

/* Text from binary.
 */
 
static int eggdev_tilesheet_text_from_bin(struct sr_encoder *dst,const uint8_t *src,int srcc,const char *path,struct eggdev_rom *rom) {
  if (!src||(srcc<4)||memcmp(src,"\0ETS",4)) return -1;
  int srcp=4,tablec=0,tablea=4;
  struct table {
    uint8_t id;
    uint8_t v[256];
  } *tablev=malloc(sizeof(struct table)*tablea);
  if (!tablev) return -1;
  
  // Read everything first into a temporary model.
  while (srcp<srcc) {
    int id=src[srcp++];
    if (!id) break;
    if (srcp>srcc-2) { free(tablev); return -1; }
    int tileid=src[srcp++];
    int tilec=src[srcp++]+1;
    if (tileid+tilec>0x100) { free(tablev); return -1; }
    if (srcp>srcc-tilec) { free(tablev); return -1; }
    
    struct table *table=0;
    int i=tablec;
    while (i-->0) {
      if (tablev[i].id==id) {
        table=tablev+i;
        break;
      }
    }
    if (!table) {
      if (tablec>=tablea) {
        int na=tablea+8;
        void *nv=realloc(tablev,sizeof(struct table)*na);
        if (!nv) { free(tablev); return -1; }
        tablev=nv;
        tablea=na;
      }
      table=tablev+tablec++;
      memset(table,0,sizeof(struct table));
      table->id=id;
    }
    memcpy(table->v+tileid,src+srcp,tilec);
    srcp+=tilec;
  }
  
  // Encode from the temporary model.
  int ti=tablec;
  struct table *table=tablev;
  for (;ti-->0;table++) {
    
    const char *tbname=0;
    int tbnamec=eggdev_namespace_name_from_id(&tbname,"tilesheet",9,table->id,rom);
    if (tbnamec>0) {
      if (sr_encode_raw(dst,tbname,tbnamec)<0) return -1;
    } else {
      if (sr_encode_fmt(dst,"%d",table->id)<0) return -1;
    }
    if (sr_encode_u8(dst,0x0a)<0) return -1;
    
    const uint8_t *tb=table->v;
    int yi=16; while (yi-->0) {
      int xi=16; for (;xi-->0;tb++) {
        if (sr_encode_fmt(dst,"%02x",*tb)<0) return -1;
      }
      if (sr_encode_u8(dst,0x0a)<0) return -1;
    }
    
    if (ti) {
      if (sr_encode_u8(dst,0x0a)<0) return -1;
    }
  }
  
  free(tablev);
  return 0;
}

/* Tilesheet resource, main entry point.
 */
 
int eggdev_compile_tilesheet(struct eggdev_res *res,struct eggdev_rom *rom) {
  if ((res->serialc>=4)&&!memcmp(res->serial,"\0ETS",4)) return 0;
  struct sr_encoder dst={0};
  int err=eggdev_tilesheet_bin_from_text(&dst,res->serial,res->serialc,res->path,rom);
  if (err<0) {
    sr_encoder_cleanup(&dst);
    return err;
  }
  eggdev_res_handoff_serial(res,dst.v,dst.c);
  return 0;
}

int eggdev_uncompile_tilesheet(struct eggdev_res *res,struct eggdev_rom *rom) {
  if ((res->serialc>=4)&&!memcmp(res->serial,"\0ETS",4)) {
    struct sr_encoder dst={0};
    int err=eggdev_tilesheet_text_from_bin(&dst,res->serial,res->serialc,res->path,rom);
    if (err<0) {
      sr_encoder_cleanup(&dst);
      return err;
    }
    eggdev_res_handoff_serial(res,dst.v,dst.c);
    return 0;
  }
  return 0;
}
