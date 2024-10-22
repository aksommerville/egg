#include "synth_formats.h"
#include "opt/serial/serial.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <stdlib.h>

/* MIDI reader context.
 */
 
struct synth_midi_reader {
  const char *path; // OPTIONAL, For logging.
  int division; // ticks/qnote
  int usperqnote;
  double mspertick;
  double fdelay; // Fractional carry. Important, when division is high (eg Logic uses 480 by default).
  struct synth_midi_track {
    const uint8_t *v;
    int c;
    int p;
    uint8_t status;
    int delay; // <0 if we need to read it.
    int chpfx;
  } *trackv;
  int trackc,tracka;
};

void synth_midi_reader_del(struct synth_midi_reader *reader) {
  if (!reader) return;
  if (reader->trackv) free(reader->trackv);
  free(reader);
}

/* Decode the MIDI file and prepare track readers.
 */
 
static int synth_midi_reader_dechunk(struct synth_midi_reader *reader,const uint8_t *src,int srcc) {
  int srcp=0;
  while (srcp<srcc) {
    if (srcp>srcc-8) return -1;
    const void *chunkid=src+srcp;
    int chunklen=(src[srcp+4]<<24)|(src[srcp+5]<<16)|(src[srcp+6]<<8)|src[srcp+7];
    srcp+=8;
    if ((chunklen<0)||(srcp>srcc-chunklen)) return -1;
    
    if (!memcmp(chunkid,"MTrk",4)) {
      if (reader->trackc>=reader->tracka) {
        int na=reader->tracka+8;
        if (na>INT_MAX/sizeof(struct synth_midi_track)) return -1;
        void *nv=realloc(reader->trackv,sizeof(struct synth_midi_track)*na);
        if (!nv) return -1;
        reader->trackv=nv;
        reader->tracka=na;
      }
      struct synth_midi_track *track=reader->trackv+reader->trackc++;
      memset(track,0,sizeof(struct synth_midi_track));
      track->v=src+srcp;
      track->c=chunklen;
      track->delay=-1;
      track->chpfx=0xff;
    
    } else if (!memcmp(chunkid,"MThd",4)) {
      if (chunklen<6) return -1;
      // Ignore Format[0,1] and TrackCount[2,4].
      reader->division=(src[srcp+4]<<8)|src[srcp+5];
      
    }
    srcp+=chunklen;
  }
  if (!reader->division||(reader->division&0x8000)||!reader->trackc) return -1;
  reader->mspertick=(double)reader->usperqnote/((double)reader->division*1000.0);
  return 0;
}

/* New MIDI reader.
 */
 
struct synth_midi_reader *synth_midi_reader_new(const void *src,int srcc) {
  struct synth_midi_reader *reader=calloc(1,sizeof(struct synth_midi_reader));
  if (!reader) return 0;
  reader->usperqnote=500000;
  if (synth_midi_reader_dechunk(reader,src,srcc)<0) {
    synth_midi_reader_del(reader);
    return 0;
  }
  return reader;
}

/* Reset MIDI reader.
 */
 
void synth_midi_reader_reset(struct synth_midi_reader *reader) {
  reader->usperqnote=500000;
  reader->mspertick=(double)reader->usperqnote/((double)reader->division*1000.0);
  struct synth_midi_track *track=reader->trackv;
  int i=reader->trackc;
  for (;i-->0;track++) {
    track->p=0;
    track->status=0;
    track->delay=-1;
    track->chpfx=0xff;
  }
}

/* Examine each event and update any reader state that needs it.
 * These events will still go to the owner; we can't and shouldn't interfere with that.
 */
 
static int synth_midi_reader_event(
  struct synth_midi_reader *reader,
  struct synth_midi_track *track,
  const struct synth_midi_event *event
) {
  switch (event->opcode) {
    case 0xff: switch (event->a) { // Meta
        case 0x20: { // MIDI Channel Prefix.
            if (event->c>=1) {
              track->chpfx=((uint8_t*)event->v)[0];
            }
          } break;
        case 0x51: { // Set Tempo.
            if (event->c>=3) {
              const uint8_t *b=event->v;
              reader->usperqnote=(b[0]<<16)|(b[1]<<8)|b[2];
              reader->mspertick=(double)reader->usperqnote/((double)reader->division*1000.0);
              if (reader->mspertick<0.001) reader->mspertick=0.001;
            }
          } break;
      } break;
  }
  return 0;
}

/* Read event from one track, update track state, and return zero on success.
 */
 
static int synth_midi_read_event(
  struct synth_midi_event *event,
  struct synth_midi_reader *reader,
  struct synth_midi_track *track
) {
  track->delay=-1;
  if (track->p>=track->c) return -1;
  uint8_t lead=track->v[track->p];
  if (lead&0x80) track->p++;
  else if (track->status) lead=track->status;
  else return -1;
  track->status=lead;
  event->chid=lead&0x0f;
  event->opcode=lead&0xf0;
  event->a=0;
  event->b=0;
  event->v=0;
  event->c=0;
  switch (lead&0xf0) {
    case 0x80: if (track->p>track->c-2) return -1; event->a=track->v[track->p++]; event->b=track->v[track->p++]; break;
    case 0x90: {
        if (track->p>track->c-2) return -1;
        event->a=track->v[track->p++];
        event->b=track->v[track->p++];
        if (!event->b) { // Note On with velocity zero becomes Note Off with velocity 0.5
          event->opcode=0x80;
          event->b=0x40;
        }
      } break;
    case 0xa0: if (track->p>track->c-2) return -1; event->a=track->v[track->p++]; event->b=track->v[track->p++]; break;
    case 0xb0: if (track->p>track->c-2) return -1; event->a=track->v[track->p++]; event->b=track->v[track->p++]; break;
    case 0xc0: if (track->p>track->c-1) return -1; event->a=track->v[track->p++]; break;
    case 0xd0: if (track->p>track->c-1) return -1; event->a=track->v[track->p++]; break;
    case 0xe0: if (track->p>track->c-2) return -1; event->a=track->v[track->p++]; event->b=track->v[track->p++]; break;
    case 0xf0: {
        track->status=0;
        event->opcode=lead;
        event->chid=track->chpfx;
        if (lead==0xff) {
          if (track->p>=track->c) return -1;
          event->a=track->v[track->p++];
        }
        int err=sr_vlq_decode(&event->c,track->v+track->p,track->c-track->p);
        if (err<1) return -1;
        track->p+=err;
        if (track->p>track->c-event->c) return -1;
        event->v=track->v+track->p;
        track->p+=event->c;
      } break;
    default: return -1;
  }
  return synth_midi_reader_event(reader,track,event);
}

/* Next event from MIDI reader.
 */
 
int synth_midi_reader_next(
  struct synth_midi_event *event,
  struct synth_midi_reader *reader
) {
  if (!event||!reader) return -1;
  struct synth_midi_track *track=reader->trackv;
  int mindelay=0x10000000; // Not expressible as VLQ.
  int i=reader->trackc,err;
  for (;i-->0;track++) {
    if (track->p>=track->c) continue;
    if (track->delay<0) {
      if ((err=sr_vlq_decode(&track->delay,track->v+track->p,track->c-track->p))<1) return -1;
      track->p+=err;
    }
    if (!track->delay) return synth_midi_read_event(event,reader,track);
    if (track->delay<mindelay) mindelay=track->delay;
  }
  if (mindelay>=0x10000000) return SYNTH_MIDI_EOF; // No events or delays -- we're done.
  reader->fdelay+=reader->mspertick*(double)mindelay;
  int ms=(int)reader->fdelay;
  if (ms<1) ms=1;
  reader->fdelay-=(double)ms;
  for (track=reader->trackv,i=reader->trackc;i-->0;track++) {
    if (track->p>=track->c) continue;
    if (track->delay<0) continue;
    track->delay-=mindelay;
  }
  return ms;
}
