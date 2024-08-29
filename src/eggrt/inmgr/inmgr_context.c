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
  
  //TODO Load templates.
  
  return inmgr;
}

/* Update.
 */
 
int inmgr_update(struct inmgr *inmgr) {
  if (!inmgr) return 0;
  if (inmgr->tmv_dirty) {
    inmgr->tmv_dirty=0;
    //TODO Save templates, if we have a save path.
  }
  return 0;
}
