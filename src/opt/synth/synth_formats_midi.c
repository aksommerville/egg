#include "synth_formats.h"
#include "opt/serial/serial.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <stdlib.h>

/* Facilities for tracking held notes.
 * Both directions of conversion will need this.
 */
 
#define MIDI_NOTE_LIMIT 256 /* Arbitrary limit on simultaneously held notes. */

struct midi_hold {
  uint8_t chid;
  uint8_t noteid;
  int time; // End time or start time, depending which direction you're converting.
  int dstp; // Output position. Only used during MIDI=>Egg.
};

static int midi_hold_find_lowest_time(const struct midi_hold *holdv,int holdc) {
  int bestp=-1,besttime=INT_MAX,i=0;
  for (;i<holdc;i++) {
    const struct midi_hold *hold=holdv+i;
    if (hold->time<besttime) {
      bestp=i;
      besttime=hold->time;
    }
  }
  return bestp;
}

static int midi_hold_find_note(const struct midi_hold *holdv,int holdc,uint8_t chid,uint8_t noteid) {
  int i=0;
  for (;i<holdc;i++,holdv++) {
    if (holdv->chid!=chid) continue;
    if (holdv->noteid!=noteid) continue;
    return i;
  }
  return -1;
}

/* Transient concept of an EGS Channel Header, converting from MIDI.
 */
 
struct channel {
  const void *v; // Raw header from Meta 0xf0.
  int c;
  int volume; // Control 0x07, if after Meta 0xf0. <0 by default.
  int pan; // Control 0x0a, if after Meta 0xf0, <0 by default.
  int pid; // Fully qualified (include Bank MSB and Bank LSB). Used only if Meta 0xf0 absent. <0 by default.
  int notec; // How many Note On events. No matter what else, a channel with
};

/* EGS binary from MIDI: Single Channel Header.
 * Caller takes care of the leading length, and must tolerate empty output.
 * May modify (channel).
 */
 
static int synth_egg_from_midi_header(
  struct sr_encoder *dst,
  struct channel *channel,const char *path,
  int (*cb_program)(void *dstpp,int fqpid,void *userdata),
  void *userdata
) {

  if (!channel->c&&(channel->pid<0)) {
    // If there's no explicit configuration or pid, call it Acoustic Grand Piano.
    channel->pid=0;
  }

  if ((channel->pid>=0)&&cb_program) {
    // If a pid is present and it matches something, replace (channel->v) entirely.
    const void *nv=0;
    int nc=cb_program(&nv,channel->pid,userdata);
    if (nv&&(nc>0)) {
      if (channel->c&&path) {
        fprintf(stderr,"%s:WARNING: %d-byte Channel Header is ignored due to Program Change (%d)\n",path,channel->c,channel->pid);
      }
      channel->v=nv;
      channel->c=nc;
    }
  }
  
  if (!channel->c&&channel->pid&&cb_program) {
    // Still no config, and pid is not zero? Try looking up shared config for program zero.
    const void *nv=0;
    int nc=cb_program(&nv,0,userdata);
    if (nv&&(nc>0)) {
      if (path) {
        if (channel->pid) fprintf(stderr,"%s:WARNING: Substituting Program 0, as Program %d was not found.\n",path,channel->pid);
        else fprintf(stderr,"%s:WARNING: Using Program 0 for unconfigured channel.\n",path);
      }
      channel->v=nv;
      channel->c=nc;
    }
  }
  
  if ((channel->volume>=0)||(channel->pan>=0)) {
    // Emit the provided header field by field and insert Volume and Pan from these MIDI events.
    struct synth_song_channel_reader reader={.src=channel->v,.srcc=channel->c};
    for (;;) {
      const void *v=0;
      int opcode=0;
      int vc=synth_song_channel_reader_next(&opcode,&v,&reader);
      if (vc<0) break;
      if (!opcode) continue; // noop, skip it.
      
      if ((opcode==0x01)&&(channel->volume>=0)) { // master
        if (sr_encode_u8(dst,0x01)<0) return -1;
        if (sr_encode_u8(dst,1)<0) return -1;
        if (sr_encode_u8(dst,(channel->volume<<1)|(channel->volume>>6))<0) return -1;
        continue;
      }
      
      if ((opcode==0x02)&&(channel->pan>=0)) { // pan. Leave the LSB always zero (so 0x40 becomes 0x80).
        if (sr_encode_u8(dst,0x02)<0) return -1;
        if (sr_encode_u8(dst,1)<0) return -1;
        if (sr_encode_u8(dst,channel->pan<<1)<0) return -1;
        continue;
      }
      
      // Everything else, shovel it thru verbatim.
      if (sr_encode_u8(dst,opcode)<0) return -1;
      if (sr_encode_u8(dst,vc)<0) return -1;
      if (sr_encode_raw(dst,v,vc)<0) return -1;
    }
    
  } else {
    // No need to override things. Emit the Channel Header verbatim. Empty is OK too.
    if (sr_encode_raw(dst,channel->v,channel->c)<0) return -1;
  }

  return 0;
}

/* EGS binary from MIDI: Headers only.
 * On success, returns a 16-bit mask of channels in use, little-endian.
 */
 
static int synth_egg_from_midi_headers(
  struct sr_encoder *dst,
  struct synth_midi_reader *reader,const char *path,
  int (*cb_program)(void *dstpp,int fqpid,void *userdata),
  void *userdata
) {
  if (sr_encode_raw(dst,"\0EGS",4)<0) return -1;
  
  struct channel channelv[16]={0};
  struct channel *channel=channelv;
  int i=16;
  for (;i-->0;channel++) {
    channel->volume=-1;
    channel->pan=-1;
    channel->pid=-1;
  }
  
  /* Read all time-zero events and update our state in (channelv) before writing anything.
   * Unfortunately we also have to read the rest of the song too, to determine whether Note On events exist.
   */
  for (;;) {
    struct synth_midi_event event={0};
    if (synth_midi_reader_next(&event,reader)<0) break; // Error or EOF.
    if (event.chid>=0x10) continue; // Only interested in events with a valid channel.
    channel=channelv+event.chid;
    switch (event.opcode) {
      case 0x90: channel->notec++; break; // Note On.
      case 0xb0: switch (event.a) { // Control Change.
          case 0x00: { // Bank MSB.
              if (channel->pid<0) channel->pid=0;
              channel->pid=(channel->pid&0x003fff)|(event.b<<14);
            } break;
          case 0x20: { // Bank LSB.
              if (channel->pid<0) channel->pid=0;
              channel->pid=(channel->pid&0x1fc07f)|(event.b<<7);
            } break;
          case 0x07: { // Volume.
              channel->volume=event.b;
            } break;
          case 0x0a: { // Pan.
              channel->pan=event.b;
            } break;
        } break;
      case 0xc0: { // Program Change.
          if (channel->pid<0) channel->pid=0;
          channel->pid=(channel->pid&0x1fff80)|event.a;
        } break;
      case 0xff: switch (event.a) { // Meta.
          case 0xf0: { // Egg Channel Header.
              channel->v=event.v;
              channel->c=event.c;
            } break;
        } break;
    }
  }
  
  /* Determine how many channels are actually being configured.
   */
  int channelc=16;
  while (channelc) {
    channel=channelv+channelc-1;
    if (!channel->notec) channel->volume=0; // No notes, we don't care whether they claim to configure it.
    if (!channel->volume) { // If they explicitly set its volume zero, don't configure it.
      channelc--;
      continue;
    }
    // Volume default or positive, call the channel "in use" if anything is set.
    if (channel->c) break;
    if (channel->volume>=0) break;
    if (channel->pan>=0) break;
    if (channel->pid>=0) break;
    if (channel->notec) break;
    channelc--;
  }
  int chusage=0;
  for (i=0,channel=channelv;i<channelc;i++,channel++) {
    if (!channel->volume) continue;
    if (channel->c||(channel->volume>=0)||(channel->pan>=0)||(channel->pid>=0)||channel->notec) chusage|=1<<i;
  }
  
  /* Emit each Channel Header.
   * EGS Channel Headers are read by index, so if there's a gap we must emit a non-empty dummy.
   */
  for (i=0,channel=channelv;i<channelc;i++,channel++) {
    if (!(chusage&(1<<i))) {
      if (sr_encode_raw(dst,"\0\2\0\0",4)<0) return -1; // noop
    } else {
      int lenp=dst->c;
      if (sr_encode_raw(dst,"\0\0",2)<0) return -1;
      int err=synth_egg_from_midi_header(dst,channel,path,cb_program,userdata);
      if (err<0) return err;
      int len=dst->c-lenp-2;
      if ((len<0)||(len>0xffff)) return -1;
      if (!len) { // Length can't be zero. Append a noop if necessary.
        if (sr_encode_raw(dst,"\0\0",2)<0) return -1;
        len=2;
      }
      ((uint8_t*)dst->v)[lenp]=len>>8;
      ((uint8_t*)dst->v)[lenp+1]=len;
    }
  }
  
  // And finally, emit the Channel Headers terminator.
  if (sr_encode_raw(dst,"\0\0",2)<0) return -1;
  return chusage;
}

/* EGS binary from MIDI: Events only.
 */
 
static int synth_egg_from_midi_events(struct sr_encoder *dst,struct synth_midi_reader *reader,int chusage,const char *path) {

  uint8_t wheelstate[16]={0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80};
  struct midi_hold holdv[MIDI_NOTE_LIMIT];
  int holdc=0;
  int time=0; // MIDI reader's time. It reports in ms, for our convenience.
  int donetime=0; // How much time emitted.
  int note_limit_warned=0;
  
  // 00tttttt : Fine Delay, (t) ms. (t) nonzero.
  // 01tttttt : Coarse Delay, ((t+1)*64) ms.
  // The edge cases here matter a lot, be careful:
  #define FLUSH_DELAY if (donetime<time) { \
    int ms=time-donetime; \
    while (ms>4096) { \
      if (sr_encode_u8(dst,0x3f)<0) return -1; \
      ms-=4096; \
    } \
    if (ms>=64) { \
      if (sr_encode_u8(dst,0x40|((ms>>6)-1))<0) return -1; \
      ms&=0x3f; \
    } \
    if (ms) { \
      if (sr_encode_u8(dst,ms)<0) return -1; \
    } \
    donetime=time; \
  }

  for (;;) {
    struct synth_midi_event event={0};
    int delay=synth_midi_reader_next(&event,reader);
    if (delay==SYNTH_MIDI_EOF) break;
    if (delay<0) return -1;
    if (delay>0) {
      time+=delay;
      continue;
    }
    if (event.chid>=0x10) continue; // Only interested in events with a valid channel.
    if (!(chusage&(1<<event.chid))) continue; // ''
    switch (event.opcode) {

      case 0x80: { // Note Off.
          int holdp=midi_hold_find_note(holdv,holdc,event.chid,event.a);
          if (holdp<0) continue;
          /* Notes are written initially as Fire-and-Forget.
           * Encoding was designed such that we can easily overwrite them, after the duration is known.
           * How convenient! This format must have been designed by somebody very very clever.
           * 1000cccc nnnnnnnv vvvvvvxx : Fire and Forget. (c) chid, (n) noteid, (v) velocity.
           * 1001cccc nnnnnnnv vvvttttt : Short Note. (c) chid, (n) noteid, (v) velocity 4-bit, (t) hold time (t+1)*16ms
           * 1010cccc nnnnnnnv vvvttttt : Long Note. (c) chid, (n) noteid, (v) velocity 4-bit, (t) hold time (t+1)*128ms
           */
          FLUSH_DELAY
          struct midi_hold *hold=holdv+holdp;
          if ((hold->dstp<0)||(hold->dstp>dst->c-3)) return -1;
          uint8_t *dstb=(uint8_t*)dst->v+hold->dstp;
          int dur=time-hold->time;
          if (dur>=16) { // Could be zero, and in that case no change. Short Notes quantize duration to 16 ms.
            if (dur<=512) { // Short Note.
              dstb[0]=(dstb[0]&0x0f)|0x90;
              dstb[2]=(dstb[2]&0xe0)|((dur/16)-1);
            } else { // Long Note.
              dur=dur/128-1;
              if (dur>0x1f) { // Possible. On the MIDI side, there's no limit.
                if (path) fprintf(stderr,
                  "%s:WARNING: Truncating duration of note 0x%02x on channel %d from %d to %d ms.\n",
                  path,event.a,event.chid,(dur+1)*128,0x20*128
                );
                dur=0x1f;
              }
              dstb[0]=(dstb[0]&0x0f)|0xa0;
              dstb[2]=(dstb[2]&0xe0)|dur;
            }
          }
          holdc--;
          memmove(hold,hold+1,sizeof(struct midi_hold)*(holdc-holdp));
        } break;
        
      case 0x90: { // Note On.
          FLUSH_DELAY
          int dstp=dst->c;
          if (sr_encode_u8(dst,0x80|event.chid)<0) return -1;
          if (sr_encode_u8(dst,(event.a<<1)|(event.b>>6))<0) return -1;
          if (sr_encode_u8(dst,event.b<<2)<0) return -1;
          if (holdc>=MIDI_NOTE_LIMIT) {
            if (path&&!note_limit_warned) {
              note_limit_warned=1;
              fprintf(stderr,
                "%s:WARNING: Exceeded %d held notes. At least one note will be emitted incorrectly with zero duration.\n",
                path,MIDI_NOTE_LIMIT
              );
            }
          } else {
            struct midi_hold *hold=holdv+holdc++;
            hold->chid=event.chid;
            hold->noteid=event.a;
            hold->time=time;
            hold->dstp=dstp;
          }
        } break;
        
      case 0xe0: { // Wheel.
          // MIDI Wheel events are 14 bits, and EGS are 8.
          // We track the most recent EGS value so as to avoid emitting changes that only impact those 6 discarded low bits.
          int v=event.a|(event.b<<7);
          v>>=6;
          if (v==wheelstate[event.chid]) break;
          wheelstate[event.chid]=v;
          FLUSH_DELAY
          if (sr_encode_u8(dst,0xb0|event.chid)<0) return -1;
          if (sr_encode_u8(dst,v)<0) return -1;
        } break;
        
      // Note Adjust, Control Change, Program Change, Channel Pressure, Meta, Sysex: Not interesting.
    }
  }
  
  // Flush any remaining delay and issue a warning for held notes.
  // We could extend those notes to end of song, perfectly reasonable, but the missing Note Off is an error, so do the easier thing: nothing.
  FLUSH_DELAY
  if (holdc&&path) fprintf(stderr,"%s:WARNING: Song ended with %d notes unreleased.\n",path,holdc);
  
  #undef FLUSH_DELAY
  return 0;
}

/* EGS binary from MIDI.
 */
 
int synth_egg_from_midi(
  struct sr_encoder *dst,
  const void *src,int srcc,const char *path,
  int (*cb_program)(void *dstpp,int fqpid,void *userdata),
  void *userdata
) {
  /* Header content and Events can be interleaved at time zero.
   * There's nothing we can do about that: In a multi-track file, we get all the time-zero events for the first MTrk before anything from the second MTrk.
   * So we're going to do one pass with the reader, picking off Header things only, until it leaves time zero.
   * Then reset it and start reading again for Events.
   */
  struct synth_midi_reader *reader=synth_midi_reader_new(src,srcc);
  if (!reader) return -1;
  int chusage=synth_egg_from_midi_headers(dst,reader,path,cb_program,userdata);
  if (chusage<0) {
    synth_midi_reader_del(reader);
    return chusage;
  }
  synth_midi_reader_reset(reader);
  int err=synth_egg_from_midi_events(dst,reader,chusage,path);
  if (err<0) {
    synth_midi_reader_del(reader);
    return err;
  }
  synth_midi_reader_del(reader);
  return 0;
}

/* Emit Channel Headers as MIDI Meta events.
 */
 
static int synth_midi_from_egg_headers(struct sr_encoder *dst,const struct synth_song_channel_part *channel) {
  int chid=0;
  for (;chid<16;chid++,channel++) {
    const uint8_t *src=channel->v;
    int srcc=channel->c;
    
    // Trim leading noop events. It's not unusual for channels to exist with nothing but noop, in order to skip their ID.
    while ((srcc>=2)&&(src[0]==0x00)) {
      int fldlen=src[1];
      src+=2+fldlen;
      srcc-=2+fldlen;
    }
    if (srcc<1) continue;
    
    // MIDI Channel Prefix.
    if (sr_encode_raw(dst,"\0\xff\x20\1",4)<0) return -1;
    if (sr_encode_u8(dst,chid)<0) return -1;
    
    // Custom Meta event 0xf0.
    if (sr_encode_raw(dst,"\0\xff\xf0",3)<0) return -1;
    if (sr_encode_vlq(dst,srcc)<0) return -1;
    if (sr_encode_raw(dst,src,srcc)<0) return -1;
    
    // We could search for Volume and Pan and emit those as Control Change, if we wanted to be more portable.
    // Could maybe also reverse-search the predefined instruments and turn that into a Program Change, to really reach for the brass ring.
    // I don't think the extra portability from those would be worth the trouble.
  }
  return 0;
}

/* Convert song events from Egg to MIDI.
 * All events pack into one track.
 */
 
static int synth_midi_from_egg_events(struct sr_encoder *dst,const uint8_t *src,int srcc) {
  struct midi_hold holdv[MIDI_NOTE_LIMIT];
  int srcp=0,holdc=0,err;
  int time=0; // Amount of emitted delay.
  int pending_delay=0; // Delays consumed but not yet emitted.
  while (srcp<srcc) {
    struct synth_song_event event;
    if ((err=synth_song_event_next(&event,src+srcp,srcc-srcp))<1) break;
    srcp+=err;
    switch (event.opcode) {
    
      case 0: { // Delay. Only emit events if a hold needs released.
          pending_delay+=event.duration;
          while (pending_delay>0) {
            int holdp=midi_hold_find_lowest_time(holdv,holdc);
            if (holdp<0) break;
            struct midi_hold *hold=holdv+holdp;
            if (hold->time>time+pending_delay) break;
            if (sr_encode_vlq(dst,hold->time-time)<0) return -1;
            pending_delay-=hold->time-time;
            time=hold->time;
            if (sr_encode_u8(dst,0x80|hold->chid)<0) return -1;
            if (sr_encode_u8(dst,hold->noteid)<0) return -1;
            if (sr_encode_u8(dst,0x40)<0) return -1; // EGS doesn't record Off Velocity.
            holdc--;
            memmove(hold,hold+1,sizeof(struct midi_hold)*(holdc-holdp));
          }
        } break;
        
      case 0x90: { // Note
          if (sr_encode_vlq(dst,pending_delay)<0) return -1;
          time+=pending_delay;
          pending_delay=0;
          if (sr_encode_u8(dst,0x90|event.chid)<0) return -1;
          if (sr_encode_u8(dst,event.noteid)<0) return -1;
          if (sr_encode_u8(dst,event.velocity)<0) return -1;
          if ((event.duration<1)||(holdc>=MIDI_NOTE_LIMIT)) {
            if (sr_encode_u8(dst,0)<0) return -1; // No delay
            if (sr_encode_u8(dst,0x80|event.chid)<0) return -1;
            if (sr_encode_u8(dst,event.noteid)<0) return -1;
            if (sr_encode_u8(dst,0x40)<0) return -1;
          } else {
            struct midi_hold *hold=holdv+holdc++;
            hold->chid=event.chid;
            hold->noteid=event.noteid;
            hold->time=time+event.duration;
          }
        } break;
        
      case 0xe0: { // Wheel
          if (sr_encode_vlq(dst,pending_delay)<0) return -1;
          time+=pending_delay;
          pending_delay=0;
          if (sr_encode_u8(dst,0xe0|event.chid)<0) return -1;
          if (sr_encode_u8(dst,event.wheel&0x7f)<0) return -1;
          if (sr_encode_u8(dst,event.wheel>>7)<0) return -1;
        } break;
        
      default: return -1;
    }
  }
  // Flush held notes, exactly what we do on Delay events, except clamp them to the end time declared by Delays.
  while (holdc>0) {
    int holdp=midi_hold_find_lowest_time(holdv,holdc);
    if (holdp<0) return -1;
    struct midi_hold *hold=holdv+holdp;
    if (hold->time>time+pending_delay) hold->time=time+pending_delay;
    if (sr_encode_vlq(dst,hold->time-time)<0) return -1;
    pending_delay-=hold->time-time;
    time=hold->time;
    if (sr_encode_u8(dst,0x80|hold->chid)<0) return -1;
    if (sr_encode_u8(dst,hold->noteid)<0) return -1;
    if (sr_encode_u8(dst,0x40)<0) return -1; // EGS doesn't record Off Velocity.
    holdc--;
    memmove(hold,hold+1,sizeof(struct midi_hold)*(holdc-holdp));
  }
  // Flush any remaining delay under the guise of Meta EOT.
  if (sr_encode_vlq(dst,pending_delay)<0) return -1;
  if (sr_encode_raw(dst,"\xff\x2f\0",3)<0) return -1;
  return 0;
}

/* MIDI from single binary format.
 * We will always use division=100 and SetTempo=100000, ie ticks are milliseconds.
 */
 
int synth_midi_from_egg(struct sr_encoder *dst,const void *src,int srcc,const char *path) {
  int err;
  if (!src||(srcc<4)||memcmp(src,"\0EGS",4)) return -1;
  
  if (sr_encode_raw(dst,"MThd\0\0\0\6",8)<0) return -1;
  if (sr_encode_intbe(dst,1,2)<0) return -1; // Format
  if (sr_encode_intbe(dst,1,2)<0) return -1; // TrackCount
  if (sr_encode_intbe(dst,100,2)<0) return -1; // Division
  
  if (sr_encode_raw(dst,"MTrk",4)<0) return -1;
  int mtrk_lenp=dst->c;
  if (sr_encode_raw(dst,"\0\0\0\0",4)<0) return -1;
  if (sr_encode_raw(dst,"\0\xff\x51\3\x01\x86\xa0",7)<0) return -1; // Delay zero, Set Tempo 100 ms/qnote.
  
  struct synth_song_parts parts={0};
  if (synth_song_split(&parts,src,srcc)<0) return -1;
  if ((err=synth_midi_from_egg_headers(dst,parts.channels))<0) return err;
  if ((err=synth_midi_from_egg_events(dst,parts.events,parts.eventsc))<0) return err;
  
  int len=dst->c-mtrk_lenp-4;
  if (len<0) return -1;
  uint8_t *p=((uint8_t*)dst->v)+mtrk_lenp;
  *(p++)=len>>24;
  *(p++)=len>>16;
  *(p++)=len>>8;
  *(p++)=len;
  return 0;
}

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
