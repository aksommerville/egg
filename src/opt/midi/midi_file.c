#include "midi.h"
#include "opt/serial/serial.h"
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>

/* Delete.
 */

void midi_file_del(struct midi_file *file) {
  if (!file) return;
  if (file->trackv) free(file->trackv);
  free(file);
}

/* Populate (framespertick) based on (rate,usperqnote).
 */

static void midi_file_recalculate_tempo(struct midi_file *file) {
  if (file->rate>0) {
    double sperqnote=(double)file->usperqnote/1000000.0;
    double fperqnote=(double)file->rate*sperqnote;
    file->framespertick=fperqnote/(double)file->division;
    // Should come out in the hundreds or thousands. As a safety measure, keep it above zero.
    // If I'm calling the rate 1000, to get timing in milliseconds, and using a Logic MIDI with the default division 480, 
    // it does sometimes drop below 1.
    if (file->framespertick<0.1) file->framespertick=0.1;
  } else {
    file->framespertick=1.0; // we won't use it, but just to be consistent
  }
}

/* Receive MThd.
 */
 
static int midi_file_decode_MThd(struct midi_file *file,const uint8_t *src,int srcc) {
  if (file->division) return -1; // Multiple MThd.
  if (srcc<6) return -1;
  
  file->format=(src[0]<<8)|src[1];
  file->track_count=(src[2]<<8)|src[3];
  file->division=(src[4]<<8)|src[5];
  
  // (format) should be 1 ie multiple tracks playing simultaneously. I don't know what to do about others, but let them thru.
  // (track_count) I'm not even sure what it's for.
  // (division) must be in 1..0x7fff. 0x8000.. is for SMPTE timing, which we don't support.
  // (division) serves as our "MThd present" flag, so it can't be zero. (also it just can't; that's mathematically incoherent).
  if (!file->division) return -1;
  if (file->division&0x8000) return -1;
  
  return 0;
}

/* Receive MTrk.
 */
 
static int midi_file_decode_MTrk(struct midi_file *file,const void *src,int srcc) {
  if (file->trackc>=file->tracka) {
    int na=file->tracka+8;
    if (na>INT_MAX/sizeof(struct midi_track)) return -1;
    void *nv=realloc(file->trackv,sizeof(struct midi_track)*na);
    if (!nv) return -1;
    file->trackv=nv;
    file->tracka=na;
  }
  struct midi_track *track=file->trackv+file->trackc++;
  memset(track,0,sizeof(struct midi_track));
  track->v=src; // BORROW
  track->c=srcc;
  track->delay=-1;
  return 0;
}

/* Finish decode.
 * Assert that we got at least one each of MThd and MTrk.
 */
 
static int midi_file_decode_finish(struct midi_file *file) {
  if (!file->division) return -1; // no MThd
  if (!file->trackc) return -1; // no MTrk
  midi_file_recalculate_tempo(file);
  return 0;
}

/* Decode.
 */
 
static int midi_file_decode(struct midi_file *file,const uint8_t *src,int srcc) {
  int srcp=0;
  while (srcp<srcc) {
    if (srcp>srcc-8) return -1;
    const void *chunkid=src+srcp;
    int chunklen=(src[srcp+4]<<24)|(src[srcp+5]<<16)|(src[srcp+6]<<8)|src[srcp+7];
    srcp+=8;
    if ((chunklen<0)||(srcp>srcc-chunklen)) return -1;
    const void *chunk=src+srcp;
    srcp+=chunklen;
    if (!memcmp(chunkid,"MThd",4)) {
      if (midi_file_decode_MThd(file,chunk,chunklen)<0) return -1;
    } else if (!memcmp(chunkid,"MTrk",4)) {
      if (midi_file_decode_MTrk(file,chunk,chunklen)<0) return -1;
    } else {
      // Unknown chunk, ignore it.
    }
  }
  return midi_file_decode_finish(file);
}

/* New.
 */

struct midi_file *midi_file_new(const void *src,int srcc,int rate) {
  struct midi_file *file=calloc(1,sizeof(struct midi_file));
  if (!file) return 0;
  file->rate=rate;
  file->usperqnote=500000;
  if (midi_file_decode(file,src,srcc)<0) {
    midi_file_del(file);
    return 0;
  }
  return file;
}

/* Read event and update track.
 */
 
static int midi_track_read_event(struct midi_event *event,struct midi_file *file,struct midi_track *track) {
  event->trackid=track-file->trackv;
  event->a=0;
  event->b=0;
  event->v=0;
  event->c=0;
  
  uint8_t lead;
  if (track->p>=track->c) return -1;
  if (track->v[track->p]&0x80) lead=track->v[track->p++];
  else if (track->status) lead=track->status;
  else return -1;
  
  track->status=lead;
  event->opcode=lead&0xf0;
  event->chid=lead&0x0f;
  track->delay=-1;
  
  switch (event->opcode) {
    #define A if (track->p>track->c-1) return -1; event->a=track->v[track->p++]; 
    #define AB if (track->p>track->c-2) return -1; event->a=track->v[track->p++]; event->b=track->v[track->p++];
    case MIDI_OPCODE_NOTE_OFF: AB return 0;
    case MIDI_OPCODE_NOTE_ON: AB if (!event->b) { event->opcode=MIDI_OPCODE_NOTE_OFF; event->b=0x40; } return 0;
    case MIDI_OPCODE_NOTE_ADJUST: AB return 0;
    case MIDI_OPCODE_CONTROL: AB return 0;
    case MIDI_OPCODE_PROGRAM: A return 0;
    case MIDI_OPCODE_PRESSURE: A return 0;
    case MIDI_OPCODE_WHEEL: AB return 0;
    default: {
        track->status=0;
        event->opcode=lead;
        event->chid=0xff;
        switch (event->opcode) {
          case 0xff: event->opcode=MIDI_OPCODE_META; A // Meta. Identical to Sysex except we get a "type" byte first, outputting as (a).
          case 0xf0: case 0xf7: { // Sysex, or Meta payload
              int paylen,seqlen;
              if ((seqlen=sr_vlq_decode(&paylen,track->v+track->p,track->c-track->p))<1) return -1;
              track->p+=seqlen;
              if (track->p>track->c-paylen) return -1;
              event->v=track->v+track->p;
              event->c=paylen;
              track->p+=paylen;
            } return 0;
          default: return -1;
        }
      } break;
    #undef A
    #undef AB
  }
  return 0;
}

/* Read delay and update track.
 */
 
static int midi_track_acquire_delay(struct midi_file *file,struct midi_track *track) {
  int tickc,seqlen;
  if ((seqlen=sr_vlq_decode(&tickc,track->v+track->p,track->c-track->p))<1) return -1;
  track->p+=seqlen;
  track->delay=tickc;
  return 0;
}

/* Check for events we need to use for our own purposes.
 * I believe this will only be Meta Set Tempo.
 * We can't prevent an event from reporting to the user from here; they'll see it too.
 */
 
static void midi_file_process_local_event(struct midi_file *file,const struct midi_event *event) {
  switch (event->opcode) {
    case MIDI_OPCODE_META: switch (event->a) {
        case MIDI_META_SET_TEMPO: if (event->c==3) {
            const uint8_t *SRC=event->v;
            int v=(SRC[0]<<16)|(SRC[1]<<8)|SRC[2];
            if ((v>0)&&(v!=file->usperqnote)) {
              file->usperqnote=v;
              midi_file_recalculate_tempo(file);
            }
          } break;
      } break;
  }
}

/* Next event if ready, or time to next.
 */

int midi_file_next(struct midi_event *event,struct midi_file *file) {
  int shortest_delay=INT_MAX;
  int finished=1;
  struct midi_track *track=file->trackv;
  int i=file->trackc;
  for (;i-->0;track++) {
    if (track->p>=track->c) continue;
    if (track->delay<0) {
      if (midi_track_acquire_delay(file,track)<0) return -1;
      if (track->p>=track->c) continue;
    }
    if (!track->delay) {
      if (midi_track_read_event(event,file,track)<0) return -1;
      midi_file_process_local_event(file,event);
      return 0;
    }
    finished=0;
    if (track->delay<shortest_delay) {
      shortest_delay=track->delay;
    }
  }
  if (finished) return -1;
  int delay_frames=(int)(shortest_delay*file->framespertick);
  if (delay_frames<1) return 1;
  return delay_frames;
}

/* Advance clock.
 */

int midi_file_advance(struct midi_file *file,int framec) {
  if (framec<1) return 0;
  
  double tickcf=framec/file->framespertick;
  file->elapsed_ticks+=tickcf;
  int tickc=(int)file->elapsed_ticks;
  if (tickc<1) return 0;
  
  // Acquire missing delays, and ensure we're not ticking further than any track's delay.
  // That would cause tracks to skew against each other in time.
  struct midi_track *track=file->trackv;
  int i=file->trackc;
  for (;i-->0;track++) {
    if (track->p>=track->c) continue;
    if (track->delay<0) {
      if (midi_track_acquire_delay(file,track)<0) return -1;
      if (track->p>=track->c) continue;
    }
    if (track->delay<tickc) tickc=track->delay;
  }
  if (tickc<1) return 0;
  file->elapsed_ticks-=tickc;
  
  for (track=file->trackv,i=file->trackc;i-->0;track++) {
    if (track->p>=track->c) continue;
    track->delay-=tickc;
  }
  return 0;
}

/* Check EOF.
 */

int midi_file_is_finished(const struct midi_file *file) {
  const struct midi_track *track=file->trackv;
  int i=file->trackc;
  for (;i-->0;track++) {
    if (track->p<track->c) return 0;
  }
  return 1;
}
