#include "eggrt_internal.h"

/* Globals.
 */
 
static struct {
  struct sr_encoder rec;
  int rec_framec;
  int rec_state;
  uint8_t *pb;
  int pbp,pbc;
  int pb_framec;
  int pb_state;
} eggrt_record={0};

/* Quit.
 */
 
void eggrt_record_quit() {

  if (eggrt.record_path) {
    sr_encode_intbe(&eggrt_record.rec,eggrt_record.rec_state,2);
    sr_encode_u8(&eggrt_record.rec,eggrt_record.rec_framec);
    if (file_write(eggrt.record_path,eggrt_record.rec.v,eggrt_record.rec.c)<0) {
      fprintf(stderr,"%s: Failed to save recording, %d bytes.\n",eggrt.record_path,eggrt_record.rec.c);
    }
  }

  sr_encoder_cleanup(&eggrt_record.rec);
  if (eggrt_record.pb) free(eggrt_record.pb);
  memset(&eggrt_record,0,sizeof(eggrt_record));
}

/* Init.
 */
 
int eggrt_record_init() {
  if (eggrt.record_path) {
  }
  if (eggrt.playback_path) {
    if ((eggrt_record.pbc=file_read(&eggrt_record.pb,eggrt.playback_path))<0) {
      fprintf(stderr,"%s: Failed to read file.\n",eggrt.playback_path);
      return -2;
    }
  }
  return 0;
}

/* Update record or playback, main entry point.
 */
 
double eggrt_record_update(double elapsed) {

  if (eggrt.playback_path) {
    elapsed=EGGRT_RECORDING_UPDATE_INTERVAL;
    if (eggrt_record.pb_framec>0) {
      eggrt_record.pb_framec--;
    } else if (eggrt_record.pbp>eggrt_record.pbc-3) {
      eggrt_record.pb_framec=INT_MAX;
      eggrt_record.pb_state=0;
      eggrt.terminate=1;
    } else {
      eggrt_record.pb_state=eggrt_record.pb[eggrt_record.pbp++]<<8;
      eggrt_record.pb_state|=eggrt_record.pb[eggrt_record.pbp++];
      eggrt_record.pb_framec=eggrt_record.pb[eggrt_record.pbp++];
    }
    if (eggrt.inmgr) {
      int i=eggrt.inmgr->playerc;
      while (i-->0) eggrt.inmgr->playerv[i]=eggrt_record.pb_state;
    }
  }
  
  if (eggrt.record_path) {
    elapsed=EGGRT_RECORDING_UPDATE_INTERVAL;
    int state=0;
    if (eggrt.inmgr&&(eggrt.inmgr->playerc>=1)) state=eggrt.inmgr->playerv[0];
    if ((state==eggrt_record.rec_state)&&(eggrt_record.rec_framec<0xff)) {
      eggrt_record.rec_framec++;
    } else {
      if (sr_encode_intbe(&eggrt_record.rec,eggrt_record.rec_state,2)<0) return elapsed;
      if (sr_encode_u8(&eggrt_record.rec,eggrt_record.rec_framec)<0) return elapsed;
      eggrt_record.rec_framec=1;
      eggrt_record.rec_state=state;
    }
  }
  
  return elapsed;
}
