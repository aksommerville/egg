#include "eggdev_internal.h"
#include "opt/synth/synth_formats.h"

#define HOLD_LIMIT 32

struct midi_hold {
  uint8_t chid,noteid;
  int dstp; // MIDI=>EGS, event's position in output.
  int rlst; // EGS=>MIDI, absolute release time in ms.
};

static const uint8_t eggdev_song_default_channel_header[]={
  0x02, // Mode 2 Flat Wave
  0x00,0x20, // Config length
    0x01,0x00, // Wheel range
    0x20,0x10, // Attack time
    0x80,0xff, // Attack level
    0x20,0x20, // Decay time
    0x20,0x40, // Sustain level
    0x01,0x00, 0x02,0x00, // Release time
    0x80,0x00, 0x80,0x00, // Pitch bend initial
    0x80,0x00, 0x80,0x00, // Pitch bend attack
    0x80,0x00, 0x80,0x00, // Pitch bend sustain
    0x80,0x00, 0x80,0x00, // Pitch bend final
    0x01, // Shape sine
    0x00, // Coefficient count, must be zero due to shape.
  0x00,0x00, // Post length
};

/* Confirm that the (param) and (post) lengths encoded here match the expected length.
 * When we read them from MIDI, they could be anything.
 * But would be disastrous* if we read as EGS with incorrect lengths.
 * [*] As in, the song wouldn't work. It can't cause OOB reads or anything really dangerous like that.
 */
 
static int eggdev_validate_partial_channel_header(const uint8_t *v,int c) {
  if (c<5) return -1;
  int configc=(v[1]<<8)|v[2];
  int p=3;
  if (p>c-configc) return -1;
  p+=configc;
  if (p>c-2) return -1;
  int postc=(v[p]<<8)|v[p+1];
  p+=2;
  if (p>c-postc) return -1;
  p+=postc;
  if (p!=c) return -1;
  return 0;
}

/* EGS from MIDI.
 */
 
// Returns tempo in ms/qnote
static int eggdev_egs_from_midi_inner(struct sr_encoder *dst,struct synth_midi_reader *reader,struct eggdev_res *res) {
  int tempo=500;
  struct synth_midi_event event;

  /* Read all the way through once without emitting anything.
   * Record config events at time zero, warn about config events at nonzero time (and don't capture).
   * Record which channels have note events.
   */
  struct chcfg {
    uint8_t master;
    uint8_t pan;
    const void *bin;
    int binc;
    int notec;
  } chcfgv[16]={0};
  struct chcfg *chcfg=chcfgv;
  int i=16; for (;i-->0;chcfg++) {
    chcfg->master=0x80;
    chcfg->pan=0x80;
  }
  int now=0;
  #define WARN_NONZERO if (now) { \
    fprintf(stderr,"%s:WARNING: Ignoring configuration event at time %d. This is only valid at time zero.\n",res->path,now); \
  }
  for (;;) {
    int delay=synth_midi_reader_next(&event,reader);
    if (delay>0) { now+=delay; continue; }
    if (delay==SYNTH_MIDI_EOF) break;
    if (delay<0) return delay;
    if (event.chid>=0x10) continue;
    switch (event.opcode) {
      case 0x90: chcfgv[event.chid].notec++; break;
      case 0xb0: switch (event.a) {
          case 0x07: WARN_NONZERO else { chcfgv[event.chid].master=(event.b<<1)|(event.b>>6); } break;
          case 0x0a: WARN_NONZERO else { chcfgv[event.chid].pan=(event.b<<1)|((event.b&0x20)?1:0); } break;
        } break;
      case 0xff: switch (event.a) {
          case 0xf0: WARN_NONZERO else { chcfgv[event.chid].bin=event.v; chcfgv[event.chid].binc=event.c; } break;
          case 0x51: WARN_NONZERO else if (event.c==3) tempo=((event.v[0]<16)|(event.v[1]<<8)|event.v[2])/1000; break;
        } break;
    }
  }
  int songdur=now;
  #undef WARN_NONZERO
  
  /* With the config acquired, determine which channels should be kept, default as needed, and emit channel headers.
   */
  int chid=0; for (chcfg=chcfgv;chid<16;chid++,chcfg++) {
    if (!chcfg->master) { // Master explicitly zero. Drop the channel.
      memset(chcfg,0,sizeof(struct chcfg));
    } else if (chcfg->notec) { // Notes present. Keep the channel even if unconfigured.
      if (!chcfg->binc) {
        fprintf(stderr,"%s:WARNING: Using default channel config for channel %d. Please open this song in Egg's song editor to configure manually.\n",res->path,chid);
        chcfg->bin=eggdev_song_default_channel_header;
        chcfg->binc=sizeof(eggdev_song_default_channel_header);
      }
      if (eggdev_validate_partial_channel_header(chcfg->bin,chcfg->binc)<0) {
        fprintf(stderr,"%s: Header for channel %d contains inconsistent lengths.\n",res->path,chid);
        return -2;
      }
      if (sr_encode_u8(dst,chid)<0) return -1;
      if (sr_encode_u8(dst,chcfg->master)<0) return -1;
      if (sr_encode_u8(dst,chcfg->pan)<0) return -1;
      if (sr_encode_raw(dst,chcfg->bin,chcfg->binc)<0) return -1;
    } else { // No notes. Drop the channel even if configured.
      memset(chcfg,0,sizeof(struct chcfg));
    }
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
              res->path,event.a,event.chid,now,songdur
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
      case 0xe0: {
          int v=event.a|(event.b<<7);
          v>>=6;
          if (v==wheelv[event.chid]) continue;
          wheelv[event.chid]=v;
          FLUSH_DELAY
          if (sr_encode_u8(dst,0xb0|event.chid)<0) return -1;
          if (sr_encode_u8(dst,v)<0) return -1;
        } break;
    }
  }
  FLUSH_DELAY
  #undef FLUSH_DELAY
  
  // Finally, issue a warning if any notes were not released.
  if (holdc) {
    fprintf(stderr,"%s:WARNING: %d notes were never released (emitted with duration zero):\n",res->path,holdc);
    int i=holdc;
    struct midi_hold *hold=holdv;
    for (;i-->0;hold++) {
      fprintf(stderr,"  0x%02x on channel %d around %d/%d ms\n",hold->noteid,hold->chid,hold->rlst,songdur);
    }
  }
  
  return tempo;
}
 
static int eggdev_song_egs_from_midi(struct sr_encoder *dst,struct eggdev_res *res) {
  if (sr_encode_raw(dst,"\0EGS",4)<0) return -1;
  int tempop=dst->c;
  if (sr_encode_raw(dst,"\x02\x00",2)<0) return -1;
  struct synth_midi_reader *reader=synth_midi_reader_new(res->serial,res->serialc);
  if (!reader) return -1;
  int tempo=eggdev_egs_from_midi_inner(dst,reader,res);
  synth_midi_reader_del(reader);
  if (tempo<0) return tempo;
  ((uint8_t*)dst->v)[tempop]=tempo>>8;
  ((uint8_t*)dst->v)[tempop+1]=tempo;
  return 0;
}

/* MIDI from EGS.
 */
 
static int eggdev_song_midi_from_egs(struct sr_encoder *dst,struct eggdev_res *res) {
  int err;
  struct synth_egs_reader reader;
  if (synth_egs_reader_init(&reader,res->serial,res->serialc)<0) return -1;
  
  int mspertick=10;
  int division=reader.tempo/mspertick;
  if (sr_encode_raw(dst,"MThd\0\0\0\6",8)<0) return -1;
  if (sr_encode_raw(dst,"\0\1\0\1",4)<0) return -1; // format=1 trackCount=1
  if (sr_encode_intbe(dst,division,2)<0) return -1;
  if (sr_encode_raw(dst,"MTrk",4)<0) return -1;
  int lenp=dst->c;
  if (sr_encode_raw(dst,"\0\0\0\0",4)<0) return -1;
  
  // Emit channel headers at time zero.
  struct synth_egs_channel channel;
  while ((err=synth_egs_reader_next_channel(&channel,&reader))>0) {
    if (channel.master==0) continue;
    if (channel.mode==EGS_MODE_NOOP) continue;
    if (sr_encode_raw(dst,"\0\xff\x20\x01",4)<0) return -1; // Meta 0x20 MIDI Channel Prefix
    if (sr_encode_u8(dst,channel.chid)<0) return -1;
    if (sr_encode_raw(dst,"\0\xff\xf0",3)<0) return -1; // Meta 0xf0 Egg-Specific EGS Channel Header
    if (sr_encode_vlq(dst,5+channel.configc+channel.postc)<0) return -1;
    if (sr_encode_u8(dst,channel.mode)<0) return -1;
    if (sr_encode_intbe(dst,channel.configc,2)<0) return -1;
    if (sr_encode_raw(dst,channel.config,channel.configc)<0) return -1;
    if (sr_encode_intbe(dst,channel.postc,2)<0) return -1;
    if (sr_encode_raw(dst,channel.post,channel.postc)<0) return -1;
    if (sr_encode_u8(dst,0)<0) return -1; // Delay
    if (sr_encode_u8(dst,0xb0|channel.chid)<0) return -1; // Control 0x07 Volume
    if (sr_encode_u8(dst,0x07)<0) return -1;
    if (sr_encode_u8(dst,channel.master>>1)<0) return -1;
    if (sr_encode_u8(dst,0)<0) return -1; // Delay
    if (sr_encode_u8(dst,0xb0|channel.chid)<0) return -1; // Control 0x0a Pan
    if (sr_encode_u8(dst,0x0a)<0) return -1;
    if (sr_encode_u8(dst,channel.pan>>1)<0) return -1;
  }
  if (err<0) return -1;
  
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
    } else {
      FLUSH_DELAY
      if (event.velocity) {
        if (sr_encode_u8(dst,0x90|event.chid)<0) return -1;
        if (sr_encode_u8(dst,event.noteid)<0) return -1;
        if (sr_encode_u8(dst,event.velocity)<0) return -1;
        if (event.duration&&(holdc<HOLD_LIMIT)) {
          struct midi_hold *hold=holdv+holdc++;
          hold->chid=event.chid;
          hold->noteid=event.noteid;
          hold->rlst=now+event.duration;
        } else if (event.duration) {
          fprintf(stderr,"%s:WARNING: Note 0x%02x on channel %d, duration dropping from %d ms to 0 due to too many held notes.\n",res->path,event.noteid,event.chid,event.duration);
        }
      } else {
        if (sr_encode_u8(dst,0xe0|event.chid)<0) return -1;
        int fourteen=event.wheel<<6;
        fourteen|=(fourteen&0x1fe0)>>7; // Don't duplicate the top bit. Must have 0x00=>0x0000, 0x80=>0x2000, 0xff=>0x3fff.
        if (sr_encode_u8(dst,fourteen&0x7f)<0) return -1;
        if (sr_encode_u8(dst,fourteen>>7)<0) return -1;
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

/* PCM from WAV.
 */
 
static int eggdev_song_pcm_from_wav(struct sr_encoder *dst,struct eggdev_res *res) {
  const uint8_t *src=res->serial;
  int srcc=res->serialc;
  if ((srcc<12)||memcmp(src,"RIFF",4)||memcmp(src+8,"WAVE",4)) return -1;
  int srcp=12,fmtok=0,chanc,rate;
  while (srcp<srcc) {
    if (srcp>srcc-8) return -1;
    const uint8_t *chunkid=src+srcp;
    int chunklen=src[srcp+4]|(src[srcp+5]<<8)|(src[srcp+6]<<16)|(src[srcp+7]<<24);
    srcp+=8;
    if ((chunklen<0)||(srcp>srcc-chunklen)) return -1;
    const uint8_t *chunk=src+srcp;
    srcp+=chunklen;
    
    if (!memcmp(chunkid,"fmt ",4)) {
      if (fmtok) return -1; // Multiple "fmt "
      fmtok=1;
      if (chunklen<16) return -1;
      int format=chunk[0]|(chunk[1]<<8);
      chanc=chunk[2]|(chunk[3]<<8);
      rate=chunk[4]|(chunk[5]<<8)|(chunk[6]<<16)|(chunk[7]<<24);
      int samplesize=chunk[14]|(chunk[15]<<8);
      if (format!=1) {
        fprintf(stderr,"%s: WAV format %d not supported. Must be 1 = LPCM\n",res->path,format);
        return -2;
      }
      if (samplesize!=16) {
        // Wouldn't be too big a deal to support others, but others are rare.
        fprintf(stderr,"%s: Only 16-bit samples are supported. Found %d\n",res->path,samplesize);
        return -2;
      }
      if ((rate<200)||(rate>200000)) {
        fprintf(stderr,"%s: Unreasonable sample rate %d. Must be in 200..200000\n",res->path,rate);
        return -2;
      }
      if ((chanc<1)||(chanc>16)) {
        fprintf(stderr,"%s: Unreasonable channel count %d\n",res->path,chanc);
        return -2;
      }
      if (chanc!=1) {
        fprintf(stderr,"%s:WARNING: %d channels in WAV file. Keeping only the first.\n",res->path,chanc);
      }
      if (sr_encode_raw(dst,"\0PCM",4)<0) return -1;
      if (sr_encode_intbe(dst,rate,4)<0) return -1;
      
    } else if (!memcmp(chunkid,"data",4)) {
      if (!fmtok) {
        fprintf(stderr,"%s: 'data' before 'fmt '\n",res->path);
        return -2;
      }
      int stride=chanc*2;
      int samplec=chunklen/stride;
      for (;samplec-->0;chunk+=stride) {
        if (sr_encode_raw(dst,chunk,2)<0) return -1;
      }
      
    }
  }
  if (!fmtok) {
    fprintf(stderr,"%s: No data\n",res->path);
    return -2;
  }
  return 0;
}

/* WAV from PCM.
 */
 
static int eggdev_song_wav_from_pcm(struct sr_encoder *dst,struct eggdev_res *res) {
  const uint8_t *src=res->serial;
  if (res->serialc<8) return -1;
  if (res->serialc&1) return -1;
  if (memcmp(res->serial,"\0PCM",4)) return -1;
  int rate=(src[4]<<24)|(src[5]<<16)|(src[6]<<8)|src[7];
  if ((rate<200)||(rate>200000)) return -1;
  if (sr_encode_raw(dst,"RIFF",4)<0) return -1;
  if (sr_encode_intle(dst,4+8+16+8+res->serialc-8,4)<0) return -1;
  if (sr_encode_raw(dst,"WAVEfmt \x10\0\0\0",12)<0) return -1;
  if (sr_encode_raw(dst,"\1\0\1\0",4)<0) return -1; // format,chanc
  if (sr_encode_intle(dst,rate,4)<0) return -1;
  if (sr_encode_intle(dst,rate*2,4)<0) return -1;
  if (sr_encode_raw(dst,"\2\0\x10\0",4)<0) return -1; // framesize,samplesize
  if (sr_encode_raw(dst,"data",4)<0) return -1;
  if (sr_encode_intle(dst,res->serialc-8,4)<0) return -1;
  if (sr_encode_raw(dst,src+8,res->serialc-8)<0) return -1;
  return 0;
}

/* Main entry points.
 * Our 'sound' and 'song' resources are so similar, I like to keep both in the same place.
 */
 
int eggdev_compile_song(struct eggdev_res *res,struct eggdev_rom *rom) {
  struct sr_encoder dst={0};
  int err=0;
  
  // Already EGS, keep it.
  if ((res->serialc>=4)&&!memcmp(res->serial,"\0EGS",4)) {
    return 0;
  
  // MIDI compiles to EGS.
  } else if ((res->serialc>=4)&&!memcmp(res->serial,"MThd",4)) {
    err=eggdev_song_egs_from_midi(&dst,res);
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

int eggdev_uncompile_song(struct eggdev_res *res,struct eggdev_rom *rom) {
  struct sr_encoder dst={0};
  int err=0;
  
  // Already MIDI, keep it.
  if ((res->serialc>=4)&&!memcmp(res->serial,"MThd",4)) {
    return 0;
    
  // EGS uncompiles to MIDI.
  } else if ((res->serialc>=4)&&!memcmp(res->serial,"\0EGS",4)) {
    err=eggdev_song_midi_from_egs(&dst,res);
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

int eggdev_compile_sound(struct eggdev_res *res,struct eggdev_rom *rom_DONT_USE) {
  struct sr_encoder dst={0};
  int err=0;
  
  // Already EGS, keep it.
  if ((res->serialc>=4)&&!memcmp(res->serial,"\0EGS",4)) {
    return 0;
    
  // Already PCM, keep it.
  } else if ((res->serialc>=4)&&!memcmp(res->serial,"\0PCM",4)) {
    return 0;
  
  // MIDI compiles to EGS.
  } else if ((res->serialc>=4)&&!memcmp(res->serial,"MThd",4)) {
    err=eggdev_song_egs_from_midi(&dst,res);
    eggdev_res_set_format(res,"egs",3);
    
  // WAV compiles to PCM.
  } else if ((res->serialc>=4)&&!memcmp(res->serial,"RIFF",4)) {
    err=eggdev_song_pcm_from_wav(&dst,res);
    eggdev_res_set_format(res,"pcm",3);
    
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

int eggdev_uncompile_sound(struct eggdev_res *res,struct eggdev_rom *rom) {
  struct sr_encoder dst={0};
  int err=0;
  
  // Already MIDI, keep it.
  if ((res->serialc>=4)&&!memcmp(res->serial,"MThd",4)) {
    return 0;
    
  // Already WAV, keep it.
  } else if ((res->serialc>=4)&&!memcmp(res->serial,"RIFF",4)) {
    return 0;
    
  // EGS uncompiles to MIDI.
  } else if ((res->serialc>=4)&&!memcmp(res->serial,"\0EGS",4)) {
    err=eggdev_song_midi_from_egs(&dst,res);
    eggdev_res_set_format(res,"mid",3);
    
  // PCM uncompiles to WAV.
  } else if ((res->serialc>=4)&&!memcmp(res->serial,"\0PCM",4)) {
    err=eggdev_song_wav_from_pcm(&dst,res);
    eggdev_res_set_format(res,"wav",3);
    
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
