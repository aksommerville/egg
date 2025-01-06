#include "eggdev_internal.h"
#include "opt/synth/synth_formats.h"

#define HOLD_LIMIT 32

struct midi_hold {
  uint8_t chid,noteid;
  int dstp; // MIDI=>EGS, event's position in output.
  int rlst; // EGS=>MIDI, absolute release time in ms.
};

/* Generate default EGS headers for a MIDI file that didn't have them.
 */
 
struct chcfg {
  uint8_t master; // 0..255
  int fqpid; // 21 bits, low 7 are Program Change
  int notec;
  uint8_t notebits[16]; // indexed by noteid little-endianly: [0]&0x01 is noteid zero.
};
 
static int eggdev_egs_headers_from_midi(struct sr_encoder *dst,const struct chcfg *chcfg/*16*/,const char *refname) {
  int chid=0,err;
  for (;chid<0x10;chid++,chcfg++) {
  
    // Master explicitly zero, or no notes, skip it.
    if (!chcfg->master||!chcfg->notec) continue;
    
    if (sr_encode_u8(dst,chid)<0) return -1;
    if (sr_encode_u8(dst,chcfg->master)<0) return -1;
    
    /* If there's an unqualified program id in the GM range (ie no Bank Select), use one of our canned instruments.
     * Note that 0 is a perfectly valid and common fqpid, but we can't distinguish it from "unset".
     * That should be ok, since GM 0 will be the final default too.
     */
    if ((chcfg->fqpid>0)&&(chcfg->fqpid<0x80)) {
      if ((err=eggdev_encode_gm_instrument(dst,chcfg->fqpid))<0) return err;
      continue;
    }
    
    /* If it's channel 9 and no GM program set, assume it's a drum kit.
     * That's a fairly strong convention across MIDI.
     */
    if (chid==0x09) {
      if ((err=eggdev_encode_gm_drums(dst,chcfg->notebits))<0) return err;
      continue;
    }
    
    /* Finally, take the low 7 bits of fqpid as a GM instrument.
     * If unset, it's zero, Acoustic Grand Piano.
     */
    if ((err=eggdev_encode_gm_instrument(dst,chcfg->fqpid&0x7f))<0) return err;
  }
  return 0;
}

/* Emit explicit EGS headers, but if a channel is missing, maybe generate a default config for it.
 * Our editor has a fake channel mode "GM" which causes it to omit channels from the EGS header, and let Program Change and Control Change take over.
 * If the user actually wants a channel dropped, they must define it with mode "NOOP".
 */
 
static int eggdev_egs_headers_merge(struct sr_encoder *dst,const uint8_t *src,int srcc,const struct chcfg *chcfg/*16*/,const char *refname) {
  /* Luckily EGS doesn't require us to emit channel headers in order.
   * Emit the verbatim ones one at a time, noting which channels we've seen.
   * Then anything unseen, where (chcfg) has something, emit the default.
  .*/
  struct chcfg altcfg[16];
  memcpy(altcfg,chcfg,sizeof(altcfg));
  int srcp=0;
  while (srcp<srcc) {
    if (srcp>srcc-6) return -1;
    uint8_t chid=src[srcp];
    if (chid<0x10) altcfg[chid].notec=0; // forces skip
    if (sr_encode_raw(dst,src+srcp,6)<0) return -1;
    int paylen=(src[srcp+3]<<16)|(src[srcp+4]<<8)|src[srcp+5];
    srcp+=6;
    if (srcp>srcc-paylen) return -1;
    if (sr_encode_raw(dst,src+srcp,paylen)<0) return -1;
    srcp+=paylen;
  }
  // Now (altcfg) is the members of (chcfg) which were not configured by (src). Encode them like we do in full-default cases.
  return eggdev_egs_headers_from_midi(dst,altcfg,refname);
}

/* EGS from MIDI.
 */
 
static int eggdev_egs_from_midi_inner(struct sr_encoder *dst,struct synth_midi_reader *reader,const char *refname) {
  struct synth_midi_event event;

  /* Read all the way through once without emitting anything.
   * Record config events at time zero, warn about config events at nonzero time (and don't capture).
   * Record which channels have note events.
   */
  struct chcfg chcfgv[16]={0};
  struct chcfg *chcfg=chcfgv;
  int i=16; for (;i-->0;chcfg++) {
    chcfg->master=0x80;
  }
  const void *egshdr=0;
  int egshdrc=0;
  int now=0,err;
  #define WARN_NONZERO if (now) { \
    fprintf(stderr,"%s:WARNING: Ignoring configuration event at time %d. This is only valid at time zero.\n",refname,now); \
  }
  for (;;) {
    int delay=synth_midi_reader_next(&event,reader);
    if (delay>0) { now+=delay; continue; }
    if (delay==SYNTH_MIDI_EOF) break;
    if (delay<0) return delay;
    if ((event.opcode==0xff)&&(event.a==0x7f)) {
      WARN_NONZERO
      else if (egshdr) {
        fprintf(stderr,"%s:WARNING: Multiple EGS headers. Using the first.\n",refname);
      } else {
        egshdr=event.v;
        egshdrc=event.c;
      }
    }
    if (event.chid>=0x10) continue;
    switch (event.opcode) {
      case 0x90: chcfgv[event.chid].notec++; chcfgv[event.chid].notebits[(event.a>>3)&15]|=1<<(event.a&7); break;
      case 0xb0: switch (event.a) {
          case 0x07: WARN_NONZERO else { chcfgv[event.chid].master=(event.b<<1)|(event.b>>6); } break;
          case 0x00: WARN_NONZERO else { chcfgv[event.chid].fqpid=(chcfgv[event.chid].fqpid&0x003fff)|(event.b<<14); } break;
          case 0x20: WARN_NONZERO else { chcfgv[event.chid].fqpid=(chcfgv[event.chid].fqpid&0x1fc07f)|(event.b<<7); } break;
        } break;
      case 0xc0: WARN_NONZERO else { chcfgv[event.chid].fqpid=(chcfgv[event.chid].fqpid&0x1fff80)|event.a; } break;
      case 0xff: switch (event.a) {
          case 0x51: WARN_NONZERO break;
        } break;
    }
  }
  int songdur=now;
  #undef WARN_NONZERO
  
  /* If an explicit EGS header is present, use it but also insert default programs for unlisted channels.
   */
  if (egshdr) {
    if ((err=eggdev_egs_headers_merge(dst,egshdr,egshdrc,chcfgv,refname))<0) return err;
    
  /* No EGS header, log a warning and make something up based on the config events we just gathered.
   */
  } else {
    fprintf(stderr,"%s:WARNING: No EGS header. Generating from defaults and MIDI config events.\n",refname);
    if ((err=eggdev_egs_headers_from_midi(dst,chcfgv,refname))<0) return err;
  }
  
  if (sr_encode_u8(dst,0xff)<0) return -1;
  
  /* Return to the start and now skip config events, and emit notes.
   */
  #define FLUSH_DELAY { \
    int delay=now-donetime; \
    donetime=now; \
    while (delay>=64) { \
      int n=(delay>>6)-1; \
      if (n>0x3f) n=0x3f; \
      if (sr_encode_u8(dst,0x40|n)<0) return -1; \
      delay-=(n+1)<<6; \
    } \
    if (delay>0) { \
      if (sr_encode_u8(dst,delay)<0) return -1; \
    } \
  }
  struct midi_hold holdv[HOLD_LIMIT];
  int holdc=0;
  uint8_t wheelv[16];
  memset(wheelv,0x80,16);
  synth_midi_reader_reset(reader);
  int donetime=0;
  for (now=0;;) {
    int delay=synth_midi_reader_next(&event,reader);
    if (delay>0) { now+=delay; continue; }
    if (delay==SYNTH_MIDI_EOF) break;
    if (delay<0) return delay;
    if (event.chid>=0x10) continue;
    chcfg=chcfgv+event.chid;
    if (!chcfg->master) continue;
    switch (event.opcode) {
      case 0x80: {
          int i=holdc;
          while (i-->0) {
            struct midi_hold *hold=holdv+i;
            if (hold->chid!=event.chid) continue;
            if (hold->noteid!=event.a) continue;
            int dur=now-hold->rlst;
            uint8_t *enc=((uint8_t*)(dst->v))+hold->dstp;
            /* The event was provisionally written as Short Note.
             * >=512 ms, change to Long Note.
             * >=64 ms, change to Medium Note.
             * `1000cccc nnnnnnnv vvvvvvtt`: Short Note. (t)*16 ms. Typically zero.
             * `1001cccc nnnnnnnv vvvttttt`: Medium Note. (t+1)*64 ms.
             * `1010cccc nnnnnnnv vvvttttt`: Long Note. (t+1)*512 ms. Note that Medium and Long overlap a little.
             */
            if (dur>=512) {
              dur=(dur>>9)-1;
              if (dur<0) dur=0;
              else if (dur>0x1f) dur=0x1f;
              enc[0]=0xa0|(enc[0]&0x0f);
              enc[2]=(enc[2]&0xe0)|dur;
            } else if (dur>=64) {
              dur=(dur>>6)-1;
              if (dur<0) dur=0;
              else if (dur>0x1f) dur=0x1f;
              enc[0]=0x90|(enc[0]&0x0f);
              enc[2]=(enc[2]&0xe0)|dur;
            } else {
              dur>>=4;
              if (dur<0) dur=0;
              else if (dur>0x03) dur=0x03;
              enc[2]=(enc[2]&0xfc)|dur;
            }
            holdc--;
            memmove(hold,hold+1,sizeof(struct midi_hold)*(holdc-i));
            break;
          }
        } break;
      case 0x90: {
          FLUSH_DELAY
          if (holdc>=HOLD_LIMIT) {
            fprintf(stderr,
              "%s:WARNING: Note 0x%02x on channel %d around %d/%d ms, dropping duration to zero due to too many held notes.\n",
              refname,event.a,event.chid,now,songdur
            );
          } else {
            struct midi_hold *hold=holdv+holdc++;
            hold->chid=event.chid;
            hold->noteid=event.a;
            hold->dstp=dst->c;
            hold->rlst=now;
          }
          // Emit provisionally as Short Note with zero duration.
          // The other two forms for Note can trivially overwrite that.
          if (sr_encode_u8(dst,0x80|event.chid)<0) return -1;
          if (sr_encode_u8(dst,(event.a<<1)|(event.b>>6))<0) return -1;
          if (sr_encode_u8(dst,event.b<<2)<0) return -1;
        } break;
    }
  }
  FLUSH_DELAY
  #undef FLUSH_DELAY
  
  // Finally, issue a warning if any notes were not released.
  if (holdc) {
    fprintf(stderr,"%s:WARNING: %d notes were never released (emitted with duration zero):\n",refname,holdc);
    int i=holdc;
    struct midi_hold *hold=holdv;
    for (;i-->0;hold++) {
      fprintf(stderr,"  0x%02x on channel %d around %d/%d ms\n",hold->noteid,hold->chid,hold->rlst,songdur);
    }
  }
  
  return 0;
}
 
int eggdev_song_egs_from_midi(struct sr_encoder *dst,const void *src,int srcc,const char *refname) {
  if (sr_encode_raw(dst,"\0EGS",4)<0) return -1;
  struct synth_midi_reader *reader=synth_midi_reader_new(src,srcc);
  if (!reader) return -1;
  int err=eggdev_egs_from_midi_inner(dst,reader,refname);
  synth_midi_reader_del(reader);
  if (err<0) return err;
  return 0;
}

/* MIDI from EGS.
 */
 
int eggdev_song_midi_from_egs(struct sr_encoder *dst,const void *src,int srcc,const char *refname) {
  int err;
  struct synth_egs_reader reader;
  if (synth_egs_reader_init(&reader,src,srcc)<0) return -1;
  
  int mspertick=10;
  int division=96; // Fairly high tick/qnote since we don't know the natural timing.
  if (sr_encode_raw(dst,"MThd\0\0\0\6",8)<0) return -1;
  if (sr_encode_raw(dst,"\0\1\0\1",4)<0) return -1; // format=1 trackCount=1
  if (sr_encode_intbe(dst,division,2)<0) return -1;
  if (sr_encode_raw(dst,"MTrk",4)<0) return -1;
  int lenp=dst->c;
  if (sr_encode_raw(dst,"\0\0\0\0",4)<0) return -1;
  
  // Emit channel headers at time zero.
  const void *headers=0;
  int headersc=synth_egs_reader_all_channels(&headers,&reader);
  if (headersc<0) return -1;
  if (sr_encode_raw(dst,"\0\xff\x7f",3)<0) return -1;
  if (sr_encode_vlq(dst,headersc)<0) return -1;
  if (sr_encode_raw(dst,headers,headersc)<0) return -1;
  
  // Emit a Set Tempo based on our timing selected arbitrarily above.
  int usperqnote=mspertick*division*1000;
  if (sr_encode_raw(dst,"\0\xff\x51\x03",4)<0) return -1;
  if (sr_encode_intbe(dst,usperqnote,3)<0) return -1;
  
  int delayms=0,now=0;
  struct midi_hold holdv[HOLD_LIMIT];
  int holdc=0;
  
  /* Advance time by up to (delayms).
   * Emits Note Off events as needed.
   * Updates (delayms,now).
   * Emits a delay -- caller must follow with an event.
   */
  #define FLUSH_DELAY { \
    while (delayms>=mspertick) { \
      int nexttime=now+delayms; \
      int holdp=-1,i=holdc; \
      while (i-->0) { \
        if (holdv[i].rlst<=nexttime) { \
          holdp=i; \
          nexttime=holdv[i].rlst; \
        } \
      } \
      if (holdp>=0) { \
        struct midi_hold *hold=holdv+holdp; \
        int delay=hold->rlst-now; \
        if (delay<0) delay=0; /* Possible due to rounding. */ \
        delay=(delay+(mspertick>>1))/mspertick; \
        if (sr_encode_vlq(dst,delay)<0) return -1; \
        if (sr_encode_u8(dst,0x80|hold->chid)<0) return -1; \
        if (sr_encode_u8(dst,hold->noteid)<0) return -1; \
        if (sr_encode_u8(dst,0x40)<0) return -1; /* EGS doesn't do release velocities. */ \
        delay*=mspertick; \
        now+=delay; \
        delayms-=delay; \
        holdc--; \
        memmove(hold,hold+1,sizeof(struct midi_hold)*(holdc-holdp)); \
      } else { \
        break; \
      } \
    } \
    int tickc=delayms/mspertick; \
    if (tickc<0) tickc=0; \
    if (sr_encode_vlq(dst,tickc)<0) return -1; \
    int ms=tickc*mspertick; \
    delayms-=ms; \
    now+=ms; \
  }
  
  // Process events.
  struct synth_egs_event event;
  while ((err=synth_egs_reader_next_event(&event,&reader))>0) {
    if (event.delay) {
      delayms+=event.delay;
    } else if (event.noteid<0x80) {
      FLUSH_DELAY
      if (sr_encode_u8(dst,0x90|event.chid)<0) return -1;
      if (sr_encode_u8(dst,event.noteid)<0) return -1;
      if (sr_encode_u8(dst,event.velocity)<0) return -1;
      if (event.duration&&(holdc<HOLD_LIMIT)) {
        struct midi_hold *hold=holdv+holdc++;
        hold->chid=event.chid;
        hold->noteid=event.noteid;
        hold->rlst=now+event.duration;
      } else if (event.duration) {
        fprintf(stderr,"%s:WARNING: Note 0x%02x on channel %d, duration dropping from %d ms to 0 due to too many held notes.\n",refname,event.noteid,event.chid,event.duration);
      }
    }
  }
  if (err<0) return -1;
  
  // Flush any remaining delay, panic and emit Note Offs as needed, then end the track.
  FLUSH_DELAY
  while (holdc>0) {
    holdc--;
    struct midi_hold *hold=holdv+holdc;
    if (sr_encode_u8(dst,0x80|hold->chid)<0) return -1;
    if (sr_encode_u8(dst,hold->noteid)<0) return -1;
    if (sr_encode_u8(dst,0x40)<0) return -1;
    if (sr_encode_u8(dst,0)<0) return -1; // delay at the end, we stole EOT's delay.
  }
  #undef FLUSH_DELAY
  if (sr_encode_raw(dst,"\xff\x2f\x00",3)<0) return -1; // Meta 0x2f End of Track
  
  int len=dst->c-lenp-4;
  uint8_t *ld=((uint8_t*)(dst->v))+lenp;
  *ld++=len>>24;
  *ld++=len>>16;
  *ld++=len>>8;
  *ld++=len;
  
  return 0;
}

/* WAV for silence.
 */

static int eggdev_song_compile_silence(struct sr_encoder *dst) {
  return sr_encode_raw(dst,
    "RIFF\x26\0\0\0WAVE" // [4..7] file length
    "fmt \x10\0\0\0"
      "\1\0\1\0" // format, chanc
      "\x44\xac\x00\x00" // [24..27] rate = 44100
      "\x88\x58\x01\x00" // [28..31] byte rate (rate*2) = 88200
      "\2\0\x10\0" // framesize, samplesize
    "data\2\0\0\0" // [40..43] data length
      "\0\0" // PCM
  ,46);
}

/* WAV from WAV, sanitizing.
 */
 
int eggdev_song_sanitize_wav(struct sr_encoder *dst,const uint8_t *src,int srcc,const char *refname) {
  #define FAIL(fmt,...) { fprintf(stderr,"%s:ERROR: "fmt"\n",refname,##__VA_ARGS__); return -2; }
  if (!src||(srcc<12)||memcmp(src,"RIFF",4)||memcmp(src+8,"WAVE",4)) FAIL("WAV signature mismatch")
  
  /* Emit provisional 44-byte header.
   * We'll come back later to fill in file length, data length, and rate.
   */
  int dstc0=dst->c;
  if (sr_encode_raw(dst,
    "RIFF\0\0\0\0WAVE" // [4..7] file length
    "fmt \x10\0\0\0"
      "\1\0\1\0" // format, chanc (constant)
      "\0\0\0\0" // [24..27] rate
      "\0\0\0\0" // [28..31] byte rate (rate*2)
      "\2\0\x10\0" // framesize, samplesize (constant)
    "data\0\0\0\0" // [40..43] data length
  ,44)<0) return -1;
  
  /* Read chunks and emit data as soon as we find it.
   * There must be exactly one "fmt " chunk before any "data" chunks.
   * We accept any channel count, and any multiple of 8 sample size 16 or greater.
   * (ie anything that we can copy verbatim samplewise).
   */
  int rate=0; // hz
  int stride=0; // bytes frame to frame
  int offset=0; // where to start reading within frame
  int verbatim=0; // Nonzero if input is mono s16le, ie we can copy entire data chunks at once.
  int srcp=12;
  while (srcp<srcc) {
    if (srcp>srcc-8) FAIL("Unexpected EOF")
    const uint8_t *chunkid=src+srcp;
    srcp+=4;
    int chunklen=src[srcp]|(src[srcp+1]<<8)|(src[srcp+2]<<16)|(src[srcp+3]<<24);
    srcp+=4;
    if ((chunklen<0)||(srcp>srcc-chunklen)) FAIL("Invalid chunk length %d around %d/%d",chunklen,srcp-8,srcc)
    const uint8_t *v=src+srcp;
    srcp+=chunklen;
    // WAV chunks must align to 2 bytes:
    if (srcp&1) srcp++;
    
    if (!memcmp(chunkid,"fmt ",4)) {
      if (rate) FAIL("Multiple 'fmt ' chunks")
      if (chunklen<16) FAIL("Invalid size %d for 'fmt ' chunk",chunklen)
      int format=v[0]|(v[1]<<8);
      int chanc=v[2]|(v[3]<<8);
      rate=v[4]|(v[5]<<8)|(v[6]<<16)|(v[7]<<24);
      int byterate=v[8]|(v[9]<<8)|(v[10]<<16)|(v[11]<<24);
      stride=v[12]|(v[13]<<8);
      int samplesize=v[14]|(v[15]<<8);
      if (format!=1) FAIL("Unsupported WAV format %d. Must be 1 (Linear PCM)",format)
      if (chanc<1) FAIL("Invalid channel count 0")
      if ((rate<200)||(rate>200000)) FAIL("Improbable sample rate %d hz. Must be in 200..200000",rate)
      if ((samplesize<16)||(samplesize&7)) FAIL("Unsupported sample size %d bits. Must be a multiple of 8, at least 16.",samplesize)
      if (stride<(samplesize*chanc)>>3) FAIL("Invalid frame size %d for samplesize=%d and chanc=%d",stride,samplesize,chanc)
      offset=(samplesize-16)>>3; // Samples are little-endian. Take the 16 most significant bits of the first channel.
      if ((stride==2)&&!offset) verbatim=1;
      continue;
    }
    
    if (!memcmp(chunkid,"data",4)) {
      if (!rate) FAIL("'data' chunk with no prior 'fmt '")
      if (verbatim) {
        chunklen&=~1;
        if (sr_encode_raw(dst,v,chunklen)<0) return -1;
      } else {
        int i=chunklen/stride;
        v+=offset;
        for (;i-->0;v+=stride) {
          if (sr_encode_raw(dst,v,2)<0) return -1;
        }
      }
      continue;
    }
  }
  
  /* If 'fmt ' or 'data' was missing, default to 1 silent sample at 44.1 kHz.
   * No need to choke on empty files.
   */
  if (!rate) rate=44100;
  int datalen=dst->c-dstc0-44;
  if (datalen<0) return -1;
  if (!datalen) {
    if (sr_encode_raw(dst,"\0\0",2)<0) return -1;
    datalen=2;
  }
  
  /* Fill in lengths etc.
   */
  int filelen=36+datalen;
  int brate=rate<<1;
  ((uint8_t*)dst->v)[dstc0+4]=filelen;
  ((uint8_t*)dst->v)[dstc0+5]=filelen>>8;
  ((uint8_t*)dst->v)[dstc0+6]=filelen>>16;
  ((uint8_t*)dst->v)[dstc0+7]=filelen>>24;
  ((uint8_t*)dst->v)[dstc0+24]=rate;
  ((uint8_t*)dst->v)[dstc0+25]=rate>>8;
  ((uint8_t*)dst->v)[dstc0+26]=rate>>16;
  ((uint8_t*)dst->v)[dstc0+27]=rate>>24;
  ((uint8_t*)dst->v)[dstc0+28]=brate;
  ((uint8_t*)dst->v)[dstc0+29]=brate>>8;
  ((uint8_t*)dst->v)[dstc0+30]=brate>>16;
  ((uint8_t*)dst->v)[dstc0+31]=brate>>24;
  ((uint8_t*)dst->v)[dstc0+40]=datalen;
  ((uint8_t*)dst->v)[dstc0+41]=datalen>>8;
  ((uint8_t*)dst->v)[dstc0+42]=datalen>>16;
  ((uint8_t*)dst->v)[dstc0+43]=datalen>>24;
  
  #undef FAIL
  return 0;
}

/* Main entry points.
 * Our 'sound' and 'song' resources are so similar, I like to keep both in the same place.
 */
 
int eggdev_compile_song(struct eggdev_res *res) {
  struct sr_encoder dst={0};
  int err=0;
  
  // Already EGS, keep it.
  if ((res->serialc>=4)&&!memcmp(res->serial,"\0EGS",4)) {
    eggdev_res_set_format(res,"egs",3);
    return 0;
  
  // MIDI compiles to EGS.
  } else if ((res->serialc>=4)&&!memcmp(res->serial,"MThd",4)) {
    err=eggdev_song_egs_from_midi(&dst,res->serial,res->serialc,res->path);
    eggdev_res_set_format(res,"egs",3);
    
  // Grudgingly allow WAV. This is important for editor playback, where the difference between "sound" and "song" is ambiguous.
  } else if ((res->serialc>=4)&&!memcmp(res->serial,"RIFF",4)) {
    err=eggdev_song_sanitize_wav(&dst,res->serial,res->serialc,res->path);
    eggdev_res_set_format(res,"wav",3);
    
  // Empty is ok, produce a short silent EGS.
  } else if (!res->serialc) {
    err=sr_encode_raw(&dst,"\0EGS\xff\x7f",6); // No channel config, and just one event: delay 4096 ms
    eggdev_res_set_format(res,"egs",3);
    
  // Nothing else is allowed.
  } else {
    fprintf(stderr,"%s: Unknown format for song. Expected MIDI.\n",res->path);
    err=-2;
  }
  
  if (err<0) {
    sr_encoder_cleanup(&dst);
    return err;
  }
  eggdev_res_handoff_serial(res,dst.v,dst.c);
  return 0;
}

int eggdev_uncompile_song(struct eggdev_res *res) {
  struct sr_encoder dst={0};
  int err=0;
  
  // Already MIDI, keep it.
  if ((res->serialc>=4)&&!memcmp(res->serial,"MThd",4)) {
    eggdev_res_set_format(res,"mid",3);
    return 0;
    
  // EGS uncompiles to MIDI.
  } else if ((res->serialc>=4)&&!memcmp(res->serial,"\0EGS",4)) {
    err=eggdev_song_midi_from_egs(&dst,res->serial,res->serialc,res->path);
    eggdev_res_set_format(res,"mid",3);
    
  // Nothing else is allowed, but we'll keep it verbatim.
  } else {
    return 0;
  }
  
  if (err<0) {
    sr_encoder_cleanup(&dst);
    return err;
  }
  eggdev_res_handoff_serial(res,dst.v,dst.c);
  return 0;
}

int eggdev_compile_sound(struct eggdev_res *res) {
  struct sr_encoder dst={0};
  int err=0;
  
  // Already EGS, keep it.
  if ((res->serialc>=4)&&!memcmp(res->serial,"\0EGS",4)) {
    eggdev_res_set_format(res,"egs",3);
    return 0;
  
  // MIDI compiles to EGS.
  } else if ((res->serialc>=4)&&!memcmp(res->serial,"MThd",4)) {
    err=eggdev_song_egs_from_midi(&dst,res->serial,res->serialc,res->path);
    eggdev_res_set_format(res,"egs",3);
    
  // WAV is fine, but we have to sanitize it.
  } else if ((res->serialc>=4)&&!memcmp(res->serial,"RIFF",4)) {
    err=eggdev_song_sanitize_wav(&dst,res->serial,res->serialc,res->path);
    eggdev_res_set_format(res,"wav",3);
    
  // Empty is ok, produce a WAV with one sample of silence.
  } else if (!res->serialc) {
    err=eggdev_song_compile_silence(&dst);
    eggdev_res_set_format(res,"wav",3);
    
  // Nothing else is allowed.
  } else {
    fprintf(stderr,"%s: Unknown format for sound. Expected MIDI or WAV.\n",res->path);
    err=-2;
  }
  
  if (err<0) {
    sr_encoder_cleanup(&dst);
    return err;
  }
  eggdev_res_handoff_serial(res,dst.v,dst.c);
  return 0;
}

int eggdev_uncompile_sound(struct eggdev_res *res) {
  struct sr_encoder dst={0};
  int err=0;
  
  // Already MIDI, keep it.
  if ((res->serialc>=4)&&!memcmp(res->serial,"MThd",4)) {
    eggdev_res_set_format(res,"mid",3);
    return 0;
    
  // Already WAV, keep it.
  } else if ((res->serialc>=4)&&!memcmp(res->serial,"RIFF",4)) {
    eggdev_res_set_format(res,"wav",3);
    return 0;
    
  // EGS uncompiles to MIDI.
  } else if ((res->serialc>=4)&&!memcmp(res->serial,"\0EGS",4)) {
    err=eggdev_song_midi_from_egs(&dst,res->serial,res->serialc,res->path);
    eggdev_res_set_format(res,"mid",3);
    
  // Nothing else is allowed, but we'll keep it verbatim.
  } else {
    return 0;
  }
  
  if (err<0) {
    sr_encoder_cleanup(&dst);
    return err;
  }
  eggdev_res_handoff_serial(res,dst.v,dst.c);
  return 0;
}
