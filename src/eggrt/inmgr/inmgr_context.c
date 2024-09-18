#include "inmgr_internal.h"

/* Delete.
 */
 
void inmgr_del(struct inmgr *inmgr) {
  if (!inmgr) return;
  if (inmgr->playerv) free(inmgr->playerv);
  if (inmgr->devicev) {
    while (inmgr->devicec-->0) inmgr_device_del(inmgr->devicev[inmgr->devicec]);
    free(inmgr->devicev);
  }
  if (inmgr->tmv) {
    while (inmgr->tmc-->0) inmgr_tm_del(inmgr->tmv[inmgr->tmc]);
    free(inmgr->tmv);
  }
  free(inmgr);
}

/* New.
 */
 
struct inmgr *inmgr_new() {
  struct inmgr *inmgr=calloc(1,sizeof(struct inmgr));
  if (!inmgr) return 0;
  
  // Determine the player count. 1 if unspecified.
  const char *players=0;
  int playersc=rom_lookup_metadata(&players,eggrt.romserial,eggrt.romserialc,"players",7,0);
  if (playersc>0) {
    // We're only interested in the last part of the string, which must be a decimal integer.
    int i=playersc;
    while (i-->0) {
      char ch=players[i];
      if ((ch<'0')||(ch>'9')) break;
      inmgr->playerc*=10;
      inmgr->playerc+=ch-'0';
    }
    inmgr->playerc++; // Metadata discusses human players. There's a special "player zero" too.
    if (inmgr->playerc<0) inmgr->playerc=0;
    else if (inmgr->playerc>INMGR_PLAYER_LIMIT) inmgr->playerc=INMGR_PLAYER_LIMIT;
  }
  if (!inmgr->playerc) inmgr->playerc=1;
  if (!(inmgr->playerv=calloc(sizeof(int),inmgr->playerc))) {
    inmgr_del(inmgr);
    return 0;
  }
  
  if (eggrt.inmgr_path) {
    char *src=0;
    int srcc=file_read(&src,eggrt.inmgr_path);
    if (srcc>=0) {
      int err=inmgr_decode_tmv(inmgr,src,srcc,eggrt.inmgr_path);
      free(src);
      if (err<0) {
        if (err!=-2) fprintf(stderr,"%s:WARNING: Failed to decode input config, %d bytes.\n",eggrt.inmgr_path,srcc);
      } else {
        fprintf(stderr,"%s: Acquired input config.\n",eggrt.inmgr_path);
      }
    } else {
      fprintf(stderr,"%s: Failed to read input config, will guess for each device.\n",eggrt.inmgr_path);
    }
  }
  
  return inmgr;
}

/* Update.
 */
 
int inmgr_update(struct inmgr *inmgr) {
  if (!inmgr) return 0;
  if (inmgr->tmv_dirty) {
    inmgr->tmv_dirty=0;
    if (eggrt.inmgr_path) {
      struct sr_encoder encoder={0};
      if (inmgr_encode_tmv(&encoder,inmgr)>=0) {
        if (file_write(eggrt.inmgr_path,encoder.v,encoder.c)>=0) {
          fprintf(stderr,"%s: Saved input config, %d bytes.\n",eggrt.inmgr_path,encoder.c);
        } else {
          fprintf(stderr,"%s:WARNING: Failed to write input config, %d bytes.\n",eggrt.inmgr_path,encoder.c);
        }
      }
      sr_encoder_cleanup(&encoder);
    }
  }
  return 0;
}

/* Set override.
 */
 
void inmgr_override_all(struct inmgr *inmgr,void (*cb)(struct hostio_input *driver,int devid,int btnid,int value,void *userdata),void *userdata) {
  inmgr->cb_override=cb;
  inmgr->userdata_override=userdata;
  // Zero all player states, except CD.
  // Do this every time, whether beginning or ending the override.
  int *v=inmgr->playerv;
  int i=inmgr->playerc;
  for (;i-->0;v++) (*v)&=EGG_BTN_CD;
}

/* Button names.
 */
 
int inmgr_button_repr(char *dst,int dsta,int btnid) {
  const char *src=0;
  int srcc=0;
  switch (btnid) {
    #define _(tag) case EGG_BTN_##tag: src=#tag; srcc=sizeof(#tag)-1; break;
    EGG_BTN_FOR_EACH
    #undef _
    #define _(tag) case EGG_SIGNAL_##tag: src=#tag; srcc=sizeof(#tag)-1; break;
    EGG_SIGNAL_FOR_EACH
    #undef _
    case EGG_BTN_LEFT|EGG_BTN_RIGHT: src="HORZ"; srcc=4; break;
    case EGG_BTN_UP|EGG_BTN_DOWN: src="VERT"; srcc=4; break;
    case EGG_BTN_LEFT|EGG_BTN_RIGHT|EGG_BTN_UP|EGG_BTN_DOWN: src="DPAD"; srcc=4; break;
  }
  if (src) {
    if (srcc<=dsta) {
      memcpy(dst,src,srcc);
      if (srcc<dsta) dst[srcc]=0;
    }
    return srcc;
  }
  return sr_decuint_repr(dst,dsta,btnid,0);
}

int inmgr_button_eval(int *btnid,const char *src,int srcc) {
  if (!src) return -1;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  // Canonical names are all uppercase, and I think 32 should be more than plenty.
  char norm[32];
  if (srcc<=sizeof(norm)) {
    int i=srcc; while (i-->0) {
      if ((src[i]>='a')&&(src[i]<='z')) norm[i]=src[i]-0x20;
      else norm[i]=src[i];
    }
    src=norm;
  }
  #define _(tag) if ((srcc==sizeof(#tag)-1)&&!memcmp(#tag,src,srcc)) { *btnid=EGG_BTN_##tag; return 0; }
  EGG_BTN_FOR_EACH
  #undef _
  #define _(tag) if ((srcc==sizeof(#tag)-1)&&!memcmp(#tag,src,srcc)) { *btnid=EGG_SIGNAL_##tag; return 0; }
  EGG_SIGNAL_FOR_EACH
  #undef _
  if ((srcc==4)&&!memcmp(norm,"HORZ",4)) { *btnid=EGG_BTN_LEFT|EGG_BTN_RIGHT; return 0; }
  if ((srcc==4)&&!memcmp(norm,"VERT",4)) { *btnid=EGG_BTN_UP|EGG_BTN_DOWN; return 0; }
  if ((srcc==4)&&!memcmp(norm,"DPAD",4)) { *btnid=EGG_BTN_LEFT|EGG_BTN_RIGHT|EGG_BTN_UP|EGG_BTN_DOWN; return 0; }
  if (sr_int_eval(btnid,src,srcc)>=1) return 0;
  return -1;
}
