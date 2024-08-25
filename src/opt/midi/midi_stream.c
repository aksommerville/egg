#include "midi.h"
#include <string.h>

/* Prepare input.
 */
 
int midi_stream_receive(struct midi_stream *stream,const void *src,int srcc) {
  stream->src=src;
  stream->srcc=(src<0)?0:srcc;
  return 0;
}

/* Populate event and return encoded length.
 */
 
static int midi_stream_next_internal(struct midi_event *event,struct midi_stream *stream,const uint8_t *src,int srcc) {
  event->opcode=0;
  event->trackid=-1;
  if (srcc<1) return -1;
  
  /* Reading Sysex is its own thing entirely.
   * I'm not sure we do this right, either. None of my equipment sends Sysex events, I've never tested it.
   */
  if (stream->status==0xf0) {
    int termp=0;
    while ((termp<srcc)&&(src[termp]!=0xf7)) termp++;
    event->chid=0xff;
    event->a=0;
    event->b=0;
    event->v=src;
    event->c=termp;
    if (termp<srcc) {
      event->opcode=MIDI_OPCODE_SYSEX_FINAL;
      stream->status=0;
      return termp+1;
    } else {
      event->opcode=MIDI_OPCODE_SYSEX_MORE;
      return termp;
    }
  }
  
  /* Acquire leading byte.
   */
  int srcp=0;
  uint8_t lead;
  if (src[srcp]&0x80) lead=stream->status=src[srcp++];
  else if (stream->status) lead=stream->status;
  else return -1;
  
  event->opcode=lead&0xf0;
  event->chid=lead&0x0f;
  event->v=0;
  event->c=0;
  
  switch (event->opcode) {
    #define A { if (srcp>srcc-1) return srcp+1; event->a=src[srcp++]; event->b=0; }
    #define AB { if (srcp>srcc-2) return srcp+2; event->a=src[srcp++]; event->b=src[srcp++]; }
    case MIDI_OPCODE_NOTE_OFF: AB break;
    case MIDI_OPCODE_NOTE_ON: AB if (event->b==0) { event->opcode=MIDI_OPCODE_NOTE_OFF; event->b=0x40; } break;
    case MIDI_OPCODE_NOTE_ADJUST: AB break;
    case MIDI_OPCODE_CONTROL: AB break;
    case MIDI_OPCODE_PROGRAM: A break;
    case MIDI_OPCODE_PRESSURE: A break;
    case MIDI_OPCODE_WHEEL: AB break;
    case 0xf0: {
        stream->status=0;
        event->opcode=lead;
        event->chid=0xff;
        event->a=0;
        event->b=0;
        if (event->opcode==MIDI_OPCODE_SYSEX_MORE) {
          stream->status=0xf0;
          event->v=src+srcp;
          while ((srcp<srcc)&&(src[srcp]!=0xf7)) {
            event->c++;
            srcp++;
          }
          if ((srcp<srcc)&&(src[srcp]==0xf7)) {
            event->opcode=MIDI_OPCODE_SYSEX_FINAL;
            srcp++;
            stream->status=0;
          }
        }
      } break;
    default: return -1;
    #undef A
    #undef AB
  }
  return srcp;
}

/* Next event.
 */

int midi_stream_next(struct midi_event *event,struct midi_stream *stream) {

  /* Something buffered?
   * Painstakingly add from (src) until something clicks.
   */
  while (stream->bufc>0) {
    int err=midi_stream_next_internal(event,stream,stream->buf,stream->bufc);
    if (err<=0) { // fault! skip a byte
      stream->faultc++;
      stream->bufc--;
      memmove(stream->buf,stream->buf+1,stream->bufc);
    } else if (err<=stream->bufc) { // cool, consume and report it
      if (stream->bufc-=err) memmove(stream->buf,stream->buf+err,stream->bufc);
      return 1;
    } else if (err>sizeof(stream->buf)) { // too big. shouldn't happen, but drop the buffer cold if it does.
      stream->faultc+=stream->bufc;
      stream->bufc=0;
    } else { // add what we can from (src)
      int cpc=err-stream->bufc;
      if (cpc>stream->srcc) cpc=stream->srcc;
      memcpy(stream->buf+stream->bufc,stream->src,cpc);
      stream->bufc+=cpc;
      stream->src+=cpc;
      stream->srcc-=cpc;
    }
  }
  
  /* Try to read an event off (src).
   */
  while (stream->srcc>0) {
    int err=midi_stream_next_internal(event,stream,stream->src,stream->srcc);
    if (err<=0) { // fault! skip a byte
      stream->faultc++;
      stream->src++;
      stream->srcc--;
    } else if (err<=stream->srcc) { // got it, normal case
      stream->src+=err;
      stream->srcc-=err;
      return 1;
    } else if (stream->srcc<=sizeof(stream->buf)) { // buffer whatever is there, we'll finish it next time.
      memcpy(stream->buf,stream->src,stream->srcc);
      stream->bufc=stream->srcc;
      stream->src=0;
      stream->srcc=0;
      return 0;
    } else { // partial event doesn't fit in the buffer. this shouldn't happen.
      stream->faultc+=stream->srcc;
      stream->src=0;
      stream->srcc=0;
      return 0;
    }
  }
  
  return 0;
}
