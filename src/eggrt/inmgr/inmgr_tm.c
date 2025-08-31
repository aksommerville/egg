#include "inmgr_internal.h"
#include <stdarg.h>

/* Cleanup.
 */
 
void inmgr_tmbtn_cleanup(struct inmgr_tmbtn *tmbtn) {
  if (tmbtn->comment) free(tmbtn->comment);
}
 
void inmgr_tm_cleanup(struct inmgr_tm *tm) {
  if (tm->name) free(tm->name);
  if (tm->comment) free(tm->comment);
  if (tm->tmbtnv) {
    while (tm->tmbtnc-->0) inmgr_tmbtn_cleanup(tm->tmbtnv+tm->tmbtnc);
    free(tm->tmbtnv);
  }
}

/* Search buttons in template.
 */
 
static int inmgr_tmbtnv_search(const struct inmgr_tm *tm,int srcbtnid) {
  int lo=0,hi=tm->tmbtnc;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
    int q=tm->tmbtnv[ck].srcbtnid;
         if (srcbtnid<q) hi=ck;
    else if (srcbtnid>q) lo=ck+1;
    else return ck;
  }
  return -lo-1;
}

struct inmgr_tmbtn *inmgr_tmbtn_for_srcbtnid(const struct inmgr_tm *tm,int srcbtnid) {
  if (!tm) return 0;
  int p=inmgr_tmbtnv_search(tm,srcbtnid);
  if (p<0) return 0;
  return tm->tmbtnv+p;
}

/* Error reporting for decoding the config file.
 */
 
static int inmgr_cfg_error(const char *src,int srcc,const char *ref,const char *fmt,...) {
  
  int lineno=0;
  if ((ref>=src)&&(ref<=src+srcc)) {
    lineno=1;
    int i=ref-src;
    for (;i-->0;src++) if (*src==0x0a) lineno++;
  }
  char path[1024];
  int pathc=inmgr_get_input_path(path,sizeof(path));
  if ((pathc<1)||(pathc>=sizeof(path))) pathc=0;
  
  char msg[256];
  va_list vargs;
  va_start(vargs,fmt);
  int msgc=vsnprintf(msg,sizeof(msg),fmt,vargs);
  if ((msgc<1)||(msgc>=sizeof(msg))) msgc=0;
  while (msgc&&(msg[msgc-1]==0x0a)) msgc--;
  
  fprintf(stderr,"%.*s:%d:ERROR: %.*s\n",pathc,path,lineno,msgc,msg);
  return -2;
}

/* Set strings.
 */
 
static int inmgr_tm_set_name(struct inmgr_tm *tm,const char *src,int srcc) {
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  char *nv=0;
  if (srcc) {
    if (!(nv=malloc(srcc+1))) return -1;
    memcpy(nv,src,srcc);
    nv[srcc]=0;
  }
  if (tm->name) free(tm->name);
  tm->name=nv;
  tm->namec=srcc;
  return 0;
}
 
static int inmgr_tm_set_comment(struct inmgr_tm *tm,const char *src,int srcc) {
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  char *nv=0;
  if (srcc) {
    if (!(nv=malloc(srcc+1))) return -1;
    memcpy(nv,src,srcc);
    nv[srcc]=0;
  }
  if (tm->comment) free(tm->comment);
  tm->comment=nv;
  tm->commentc=srcc;
  return 0;
}
 
static int inmgr_tmbtn_set_comment(struct inmgr_tmbtn *tmbtn,const char *src,int srcc) {
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  char *nv=0;
  if (srcc) {
    if (!(nv=malloc(srcc+1))) return -1;
    memcpy(nv,src,srcc);
    nv[srcc]=0;
  }
  if (tmbtn->comment) free(tmbtn->comment);
  tmbtn->comment=nv;
  tmbtn->commentc=srcc;
  return 0;
}

/* Add button to template, from config file's text.
 * Caller evaluates the first token (srcbtnid).
 */
 
static int inmgr_tmbtn_add(struct inmgr_tm *tm,int srcbtnid,const char *src,int srcc) {
  int srcp=0;
  while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
  const char *token=src+srcp;
  int tokenc=0;
  while ((srcp<srcc)&&((unsigned char)src[srcp++]>0x20)) tokenc++;
  while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
  int dstbtnid;
  if (inmgr_btnid_eval(&dstbtnid,token,tokenc)<0) {
    // Unknown dstbtnid is legal. Include it in the comment.
    token=src;
    tokenc=srcc;
  } else {
    token=src+srcp;
    tokenc=srcc-srcp;
  }
  while (tokenc&&((unsigned char)token[tokenc-1]<=0x20)) tokenc--;
  while (tokenc&&((unsigned char)token[0]<=0x20)) { token++; tokenc--; }
  
  int tmbtnp=inmgr_tmbtnv_search(tm,srcbtnid);
  if (tmbtnp>=0) return 0; // Duplicate srcbtnid. Keep the first only.
  tmbtnp=-tmbtnp-1;
  if (tm->tmbtnc>=tm->tmbtna) {
    int na=tm->tmbtna+16;
    if (na>INT_MAX/sizeof(struct inmgr_tmbtn)) return -1;
    void *nv=realloc(tm->tmbtnv,sizeof(struct inmgr_tmbtn)*na);
    if (!nv) return -1;
    tm->tmbtnv=nv;
    tm->tmbtna=na;
  }
  struct inmgr_tmbtn *tmbtn=tm->tmbtnv+tmbtnp;
  memmove(tmbtn+1,tmbtn,sizeof(struct inmgr_tmbtn)*(tm->tmbtnc-tmbtnp));
  tm->tmbtnc++;
  memset(tmbtn,0,sizeof(struct inmgr_tmbtn));
  tmbtn->srcbtnid=srcbtnid;
  tmbtn->dstbtnid=dstbtnid;
  if (tokenc) {
    if (inmgr_tmbtn_set_comment(tmbtn,token,tokenc)<0) return -1;
  }
  return 0;
}

/* Add a new template, from config file's text.
 * Caller strips the introducer ">>>".
 */
 
static struct inmgr_tm *inmgr_tm_add(const char *src,int srcc) {
  
  if (inmgr.tmc>=inmgr.tma) {
    int na=inmgr.tma+16;
    if (na>INT_MAX/sizeof(struct inmgr_tm)) return 0;
    void *nv=realloc(inmgr.tmv,sizeof(struct inmgr_tm)*na);
    if (!nv) return 0;
    inmgr.tmv=nv;
    inmgr.tma=na;
  }
  struct inmgr_tm *tm=inmgr.tmv+inmgr.tmc++;
  memset(tm,0,sizeof(struct inmgr_tm));
  if (inmgr.tmc>1) tm->tmid=tm[-1].tmid+1;
  else tm->tmid=1;
  
  int srcp=0;
  while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
  const char *token;
  int tokenc;
  #define NEXTTOKEN { \
    token=src+srcp; \
    tokenc=0; \
    if ((srcp<srcc)&&(src[srcp]=='"')) { \
      srcp++; \
      tokenc++; \
      for (;;) { \
        if (srcp>=srcc) return 0; \
        tokenc++; \
        if (src[srcp++]=='"') break; \
      } \
    } else { \
      while ((srcp<srcc)&&((unsigned char)src[srcp++]>0x20)) tokenc++; \
      while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++; \
    } \
  }
  NEXTTOKEN
  
  if (tokenc&&(token[0]!='"')) {
    if (inmgr_hexuint_eval(&tm->vid,token,tokenc)<0) return 0;
    NEXTTOKEN
  }
  if (tokenc&&(token[0]!='"')) {
    if (inmgr_hexuint_eval(&tm->pid,token,tokenc)<0) return 0;
    NEXTTOKEN
  }
  if (tokenc&&(token[0]!='"')) {
    if (inmgr_hexuint_eval(&tm->version,token,tokenc)<0) return 0;
    NEXTTOKEN
  }
  
  if ((tokenc>=2)&&(token[0]=='"')) {
    if (inmgr_tm_set_name(tm,token+1,tokenc-2)<0) return 0;
    NEXTTOKEN
  }
  #undef NEXTTOKEN
  
  // Extend this last token to the end.
  tokenc+=srcc-srcp;
  while (tokenc&&((unsigned char)token[tokenc-1]<=0x20)) tokenc--;
  if (tokenc) {
    if (inmgr_tm_set_comment(tm,token,tokenc)<0) return 0;
  }
  
  return tm;
}

/* Load templates from text.
 */
 
static int inmgr_tmv_decode(const char *src,int srcc) {
  int srcp=0;
  struct inmgr_tm *tm=0;
  while (srcp<srcc) {
    const char *line=src+srcp;
    int linec=0;
    while ((srcp<srcc)&&(src[srcp++]!=0x0a)) linec++;
    while (linec&&((unsigned char)line[linec-1]<=0x20)) linec--;
    while (linec&&((unsigned char)line[0]<=0x20)) { line++; linec--; }
    if (!linec||(line[0]=='#')) continue;
    
    const char *lead=line;
    int leadc=0,linep=0;
    while ((linep<linec)&&((unsigned char)line[linep++]>0x20)) leadc++;
    while ((linep<linec)&&((unsigned char)line[linep]<=0x20)) linep++;
    
    if ((leadc==3)&&!memcmp(lead,">>>",3)) {
      if (!(tm=inmgr_tm_add(line+linep,linec-linep))) return inmgr_cfg_error(src,srcc,lead,"Malformed device introducer.");
      continue;
    }
    
    if (!tm) return inmgr_cfg_error(src,srcc,lead,"Unexpected leading token '%.*s' outside device block.",leadc,lead);
    
    int srcbtnid;
    if (inmgr_hexuint_eval(&srcbtnid,lead,leadc)<0) {
      return inmgr_cfg_error(src,srcc,lead,"Expected '>>>' or an unprefixed hexadecimal integer, found '%.*s'.",leadc,lead);
    }
    if (inmgr_tmbtn_add(tm,srcbtnid,line+linep,linec-linep)<0) {
      return inmgr_cfg_error(src,srcc,line+linep,"Error adding device button.");
    }
  }
  return 0;
}

/* Load templates from config file.
 */
 
int inmgr_load() {
  
  /* First drop all templates.
   * This should be noop, since we should only be called during init.
   * If it does happen while running, existing devices will become dissociated from their template.
   * That is a solvable problem, but I haven't done because I don't think we'll ever reload config during run.
   */
  while (inmgr.tmc>0) {
    inmgr.tmc--;
    inmgr_tm_cleanup(inmgr.tmv+inmgr.tmc);
  }
  struct inmgr_device *device=inmgr.devicev;
  int i=inmgr.devicec;
  for (;i-->0;device++) device->tmid=0;
  
  /* Read the config file's text.
   * If it fails to read, eg doesn't exist, no problem.
   */
  char *src=0;
  int srcc=inmgr_config_read(&src,0,0,"input",5);
  if (srcc<0) return 0;
  int err=inmgr_tmv_decode(src,srcc);
  free(src);
  return err;
}

/* Support for encoding the config file.
 * We could require the 'serial' unit, but I don't want to because that's kind of a lot to ask, and we'd only need it for this one task.
 */
 
struct inmgr_encoder {
  char *v;
  int c,a; // (a<0) to signal errors.
};

static void inmgr_encoder_cleanup(struct inmgr_encoder *encoder) {
  if (encoder->v) free(encoder->v);
}

static int inmgr_encoder_require(struct inmgr_encoder *encoder,int addc) {
  if (encoder->a<0) return -1;
  if (addc<1) return 0;
  if (encoder->c>INT_MAX-addc) return encoder->a=-1;
  int na=encoder->c+addc;
  if (na<=encoder->a) return 0;
  if (na<INT_MAX-1024) na=(na+1024)&~1023;
  void *nv=realloc(encoder->v,na);
  if (!nv) return encoder->a=-1;
  encoder->v=nv;
  encoder->a=na;
  return 0;
}

static int inmgr_encode_raw(struct inmgr_encoder *encoder,const char *src,int srcc) {
  if (encoder->a<0) return -1;
  if (!src) return 0;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  if (inmgr_encoder_require(encoder,srcc)<0) return -1;
  memcpy(encoder->v+encoder->c,src,srcc);
  encoder->c+=srcc;
  return 0;
}

static int inmgr_encode_hexuint(struct inmgr_encoder *encoder,int src) {
  for (;;) {
    int err=inmgr_hexuint_repr(encoder->v+encoder->c,encoder->a-encoder->c,src);
    if (err<0) return encoder->a=-1; // huh?
    if (encoder->c<=encoder->a-err) {
      encoder->c+=err;
      return 0;
    }
    if (inmgr_encoder_require(encoder,err)<0) return -1;
  }
}

static int inmgr_encode_btnid(struct inmgr_encoder *encoder,int src) {
  for (;;) {
    int err=inmgr_btnid_repr(encoder->v+encoder->c,encoder->a-encoder->c,src);
    if (err<0) return encoder->a=-1; // huh?
    if (encoder->c<=encoder->a-err) {
      encoder->c+=err;
      return 0;
    }
    if (inmgr_encoder_require(encoder,err)<0) return -1;
  }
}

/* Encode template header (just the device introducer line).
 * Must leave (encoder.a<0) on any error.
 */
 
static int inmgr_tm_encode_header(struct inmgr_encoder *encoder,const struct inmgr_tm *tm) {
  // An extra LF on the front for visual separation. Not syntatically meaningful.
  inmgr_encode_raw(encoder,"\n>>> ",5);
  inmgr_encode_hexuint(encoder,tm->vid);
  inmgr_encode_raw(encoder," ",1);
  inmgr_encode_hexuint(encoder,tm->pid);
  inmgr_encode_raw(encoder," ",1);
  inmgr_encode_hexuint(encoder,tm->version);
  inmgr_encode_raw(encoder," \"",2);
  inmgr_encode_raw(encoder,tm->name,tm->namec);
  if (tm->commentc) {
    inmgr_encode_raw(encoder,"\" ",2);
    inmgr_encode_raw(encoder,tm->comment,tm->commentc);
    inmgr_encode_raw(encoder,"\n",1);
  } else {
    inmgr_encode_raw(encoder,"\"\n",2);
  }
  return encoder->a;
}

/* Encode one button rule.
 * Must leave (encoder.a<0) on any error.
 */
 
static int inmgr_tmbtn_encode(struct inmgr_encoder *encoder,const struct inmgr_tmbtn *tmbtn) {
  inmgr_encode_raw(encoder,"  ",2);
  inmgr_encode_hexuint(encoder,tmbtn->srcbtnid);
  inmgr_encode_raw(encoder," ",1);
  inmgr_encode_btnid(encoder,tmbtn->dstbtnid);
  if (tmbtn->commentc) {
    inmgr_encode_raw(encoder," ",1);
    inmgr_encode_raw(encoder,tmbtn->comment,tmbtn->commentc);
  }
  inmgr_encode_raw(encoder,"\n",1);
  return encoder->a;
}

/* Save config file.
 */

int inmgr_save() {
  struct inmgr_encoder encoder={0};
  if (inmgr_encode_raw(&encoder,"# This file may be rewritten automatically. Comments will not be preserved.\n",-1)>=0) {
    const struct inmgr_tm *tm=inmgr.tmv;
    int tmi=inmgr.tmc;
    for (;tmi-->0;tm++) {
      if (inmgr_tm_encode_header(&encoder,tm)<0) break;
      const struct inmgr_tmbtn *tmbtn=tm->tmbtnv;
      int tmbtni=tm->tmbtnc;
      for (;tmbtni-->0;tmbtn++) {
        if (inmgr_tmbtn_encode(&encoder,tmbtn)<0) break;
      }
    }
  }
  int err=-1;
  if (encoder.a>=0) {
    err=inmgr_config_write(0,0,"input",5,encoder.v,encoder.c);
  }
  inmgr_encoder_cleanup(&encoder);
  return err;
}

/* Get a template from the registry.
 */
 
struct inmgr_tm *inmgr_tm_for_keyboard() {
  struct inmgr_tm *tm=inmgr.tmv;
  int i=inmgr.tmc;
  for (;i-->0;tm++) {
    if (tm->vid) continue;
    if (tm->pid) continue;
    if (tm->version) continue;
    if (tm->namec!=15) continue;
    if (memcmp(tm->name,"System Keyboard",15)) continue;
    return tm;
  }
  return 0;
}

struct inmgr_tm *inmgr_tm_for_device(const struct inmgr_device *device) {
  if (!device) return 0;
  struct inmgr_tm *tm=inmgr.tmv;
  int i=inmgr.tmc;
  for (;i-->0;tm++) {
    if (tm->vid&&(tm->vid!=device->vid)) continue;
    if (tm->pid&&(tm->pid!=device->pid)) continue;
    if (tm->version&&(tm->version!=device->version)) continue;
    if (tm->namec) {
      if (tm->namec!=device->namec) continue;
      if (memcmp(tm->name,device->name,tm->namec)) continue;
    }
    return tm;
  }
  return 0;
}

/* Synthesize a new template for a given device, and add it to the registry.
 */

struct inmgr_tm *inmgr_tm_synthesize_from_device(struct inmgr_device *device) {
  if (!device) return 0;
  
  struct inmgr_tm *tm=inmgr_tm_add(0,0);
  if (!tm) return 0;
  device->tmid=tm->tmid;
  tm->vid=device->vid;
  tm->pid=device->pid;
  tm->version=device->version;
  inmgr_tm_set_name(tm,device->name,device->namec);
  
  const struct inmgr_button *button=device->buttonv;
  int i=device->buttonc;
  for (;i-->0;button++) {
    inmgr_tmbtn_intern(tm,button->srcbtnid,button->dstbtnid);
  }
  inmgr_signal(INMGR_BTN_AUTOMAPPED);
  return tm;
}

/* Modify or add button.
 */
 
int inmgr_tmbtn_intern(struct inmgr_tm *tm,int srcbtnid,int dstbtnid) {
  int p=inmgr_tmbtnv_search(tm,srcbtnid);
  if (p>=0) {
    tm->tmbtnv[p].dstbtnid=dstbtnid;
  } else {
    p=-p-1;
    if (tm->tmbtnc>=tm->tmbtna) {
      int na=tm->tmbtna+16;
      if (na>INT_MAX/sizeof(struct inmgr_tmbtn)) return -1;
      void *nv=realloc(tm->tmbtnv,sizeof(struct inmgr_tmbtn)*na);
      if (!nv) return -1;
      tm->tmbtnv=nv;
      tm->tmbtna=na;
    }
    struct inmgr_tmbtn *tmbtn=tm->tmbtnv+p;
    memmove(tmbtn+1,tmbtn,sizeof(struct inmgr_tmbtn)*(tm->tmbtnc-p));
    tm->tmbtnc++;
    memset(tmbtn,0,sizeof(struct inmgr_tmbtn));
    tmbtn->srcbtnid=srcbtnid;
    tmbtn->dstbtnid=dstbtnid;
  }
  return 0;
}

/* Drop mappings.
 */
 
void inmgr_clear_map(int devid) {
  struct inmgr_device *device=inmgr_device_by_devid(devid);
  if (!device) return;
  
  // Drop live state.
  struct inmgr_button *button=device->buttonv;
  int i=device->buttonc;
  for (;i-->0;button++) {
    button->dstbtnid=0;
  }
  device->state=INMGR_BTN_CD;
  memset(device->extbtnv,0,sizeof(int)*inmgr.extbtnc);
  inmgr.playerv[device->playerid].state=INMGR_BTN_CD;
  inmgr.playerv[0].state=INMGR_BTN_CD;
  
  // If there's a template, drop all its fields.
  struct inmgr_tm *tm=0;
  struct inmgr_tm *q=inmgr.tmv;
  for (i=inmgr.tmc;i-->0;q++) {
    if (q->tmid==device->tmid) {
      tm=q;
      break;
    }
  }
  if (tm) {
    while (tm->tmbtnc>0) {
      tm->tmbtnc--;
      inmgr_tmbtn_cleanup(tm->tmbtnv+tm->tmbtnc);
    }
  }
}
