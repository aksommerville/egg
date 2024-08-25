#include "eggdev_internal.h"
#include "opt/midi/midi.h"

/* Egg from MIDI: Context.
 */

// Arbitrary limit. But the synthesizer will probably have an internal limit too, and probably not more than 32.
#define EGGDEV_SONG_NOTE_LIMIT 32
 
struct eggdev_song_egg_from_midi {
  uint8_t hdr[71]; // 7 + 8 * 8
  struct sr_encoder dst;
  struct eggdev_res *res;
  struct midi_file *midi;
  uint8_t chid_egg_from_midi[16];
  int time;
  int delaycarry; // We can only emit delay in 4ms increments. Overflow 0..3 gets cached here.
  struct note {
    uint8_t chid; // In output.
    uint8_t noteid;
    int dstp;
    int ontime;
  } notev[EGGDEV_SONG_NOTE_LIMIT];
  int notec;
};

static void eggdev_song_egg_from_midi_cleanup(struct eggdev_song_egg_from_midi *ctx) {
  sr_encoder_cleanup(&ctx->dst);
  midi_file_del(ctx->midi);
}

static void eggdev_song_egg_from_midi_init(
  struct eggdev_song_egg_from_midi *ctx,
  struct eggdev_res *res
) {
  memset(ctx,0,sizeof(struct eggdev_song_egg_from_midi));
  ctx->res=res;
  memset(ctx->chid_egg_from_midi,0xff,sizeof(ctx->chid_egg_from_midi));
  // We start with a default tempo of 512 ms/qnote, which is basically the same as MIDI's conventional default of 500.
  memcpy(ctx->hdr,"\0\xbe\xeeP\x02\x00",6);
}

/* After init, all channel headers are straight zeroes.
 * Call this on initializing a channel to set sensible defaults.
 */
 
static void eggdev_song_write_default_header(struct eggdev_song_egg_from_midi *ctx,uint8_t chid) {
  if (chid>=8) return;
  uint8_t *dst=ctx->hdr+7+chid*8;
  dst[0]=0x40; // Volume=0.5
  dst[1]=0x81; // Pan=0 Sustain=1
  dst[2]=0x88; // Low.atkt=0.5 Low.susv=0.5
  dst[3]=0x88; // Low.rlst=0.5 High.atkt=0.5
  dst[4]=0x88; // High.susv=0.5 High.rlst=0.5
  // [5..6] Shape and Modulator: Zeroes are fine, means unmodulated sine.
  // [7] Delay and Overdrive: Zeroes means none.
}

/* Return Egg channel for MIDI channel. >=8 for errors.
 * Allocates if necessary.
 */
 
static int eggdev_song_chid_in_use(const struct eggdev_song_egg_from_midi *ctx,uint8_t echid) {
  const uint8_t *src=ctx->chid_egg_from_midi;
  int i=16; for (;i-->0;src++) if (*src==echid) return 1;
  return 0;
}
 
static uint8_t eggdev_song_resolve_chid(struct eggdev_song_egg_from_midi *ctx,uint8_t mchid) {
  if (mchid>=0x10) return 0xff;
  uint8_t echid=ctx->chid_egg_from_midi[mchid];
  if (echid<8) return echid;
  for (echid=0;echid<8;echid++) {
    if (eggdev_song_chid_in_use(ctx,echid)) continue;
    ctx->chid_egg_from_midi[mchid]=echid;
    eggdev_song_write_default_header(ctx,echid);
    if (echid>=ctx->hdr[6]) ctx->hdr[6]=echid+1;
    return echid;
  }
  // Use the invalid 0xfe to signal that we have logged this dropped channel already.
  if (ctx->chid_egg_from_midi[mchid]==0xfe) return 0xff;
  ctx->chid_egg_from_midi[mchid]=0xfe;
  fprintf(stderr,"%s:WARNING: Dropping MIDI channel %d. Egg songs are limited to 8 channels.\n",ctx->res->path,mchid);
  return 0xff;
}

/* Egg from MIDI: Note Off
 */
 
static int eggdev_song_receive_midi_note_off(
  struct eggdev_song_egg_from_midi *ctx,
  const struct midi_event *event
) {
  uint8_t chid=eggdev_song_resolve_chid(ctx,event->chid);
  if (chid>=8) return 0;
  struct note *note=ctx->notev+ctx->notec-1;
  int i=ctx->notec;
  for (;i-->0;note--) {
    if (note->chid!=chid) continue;
    if (note->noteid!=event->a) continue;
    uint8_t *dst=((uint8_t*)(ctx->dst.v))+note->dstp;
    int dur=ctx->time-note->ontime;
    // Notes are provisionally written as Short, up to 255 ms.
    if (dur<=0xff) {
      dst[2]=dur;
    // We can change to Long and emit in 16ms chunks instead.
    } else {
      dst[0]=(dst[0]&0x1f)|0xc0;
      dur=(dur+8)>>4;
      if (dur>0xff) dur=0xff;
      dst[2]=dur;
    }
    ctx->notec--;
    memmove(note,note+1,sizeof(struct note)*(ctx->notec-i));
    return 0;
  }
  // Note not found. Oh well.
  return 0;
}

/* Egg from MIDI: Note On
 */
 
static int eggdev_song_receive_midi_note_on(
  struct eggdev_song_egg_from_midi *ctx,
  const struct midi_event *event
) {
  uint8_t chid=eggdev_song_resolve_chid(ctx,event->chid);
  if (chid>=8) return 0;
  struct note *note=ctx->notev;
  int i=ctx->notec;
  for (;i-->0;note++) {
    if ((note->chid==chid)&&(note->noteid==event->a)) {
      fprintf(stderr,"%s:WARNING: Ignoring note 0x%02x on channel %d at time %d ms, already held.\n",ctx->res->path,event->a,event->chid,ctx->time);
      return 0;
    }
  }
  if (ctx->notec>=EGGDEV_SONG_NOTE_LIMIT) {
    fprintf(stderr,
      "%s:WARNING: Emitting note 0x%02x on channel %d at time %d ms as zero-duration due to %d notes already held.\n",
      ctx->res->path,event->a,event->chid,ctx->time,EGGDEV_SONG_NOTE_LIMIT
    );
    if (sr_encode_u8(&ctx->dst,0x80|(chid<<2)|(event->a>>5))<0) return -1;
    if (sr_encode_u8(&ctx->dst,(event->a<<3)|(event->b>>4))<0) return -1;
    return 0;
  }
  note=ctx->notev+ctx->notec++;
  note->chid=chid;
  note->noteid=event->a;
  note->dstp=ctx->dst.c;
  note->ontime=ctx->time;
  // Emit provisionally as Short Note. Can change to Long Note by changing the top 3 bits from 0xa0 to 0xc0.
  if (sr_encode_u8(&ctx->dst,0xa0|(chid<<2)|(event->a>>5))<0) return -1;
  if (sr_encode_u8(&ctx->dst,(event->a<<3)|(event->b>>4))<0) return -1;
  if (sr_encode_u8(&ctx->dst,0)<0) return -1;
  return 0;
}

/* Egg from MIDI: Pitch Wheel
 */
 
static int eggdev_song_receive_midi_wheel(
  struct eggdev_song_egg_from_midi *ctx,
  const struct midi_event *event
) {
  uint8_t chid=eggdev_song_resolve_chid(ctx,event->chid);
  if (chid>=8) return 0;
  int v=event->a|(event->b<<7);
  v>>=4; // MIDI uses 14 bits, Egg uses 10.
  if (sr_encode_u8(&ctx->dst,0xe0|(chid<<2)|(v>>8))<0) return -1;
  if (sr_encode_u8(&ctx->dst,v)<0) return -1;
  return 0;
}

/* Egg from MIDI: Channel header.
 * (chid) is MIDI, not Egg.
 * (bitp) is the big-endian bit position where output should start. So no more than (64-7).
 */
 
static int eggdev_song_receive_midi_header(
  struct eggdev_song_egg_from_midi *ctx,
  uint8_t chid,uint8_t v,int bitp
) {
  if ((chid=eggdev_song_resolve_chid(ctx,chid))>=8) return 0;
  if ((bitp<0)||(bitp>57)) return -1;
  uint8_t *dst=ctx->hdr+7+(chid*8)+(bitp>>3);
  uint8_t dstmask=0x80>>(bitp&7);
  uint8_t srcmask=0x40;
  for (;srcmask;srcmask>>=1) {
    if (v&srcmask) (*dst)|=dstmask;
    else (*dst)&=~dstmask;
    if (dstmask==1) { dstmask=0x80; dst++; }
    else dstmask>>=1;
  }
  return 0;
}

/* Egg from MIDI: Tempo change.
 * We'll record the tempo at time zero.
 * Tempo changes after that will work for timing purposes but may skew playhead reporting and effects.
 * We'll issue a warning beyond time zero.
 */
 
static int eggdev_song_receive_midi_tempo(
  struct eggdev_song_egg_from_midi *ctx,
  const uint8_t *src,int srcc
) {
  if (srcc!=3) return 0;
  int oldtempo=(ctx->hdr[4]<<8)|ctx->hdr[5];
  int newtempo=((src[0]<<16)|(src[1]<<8)|src[0])/1000;
  if (newtempo<1) newtempo=1;
  else if (newtempo>0xffff) newtempo=0xffff;
  if (oldtempo==newtempo) return 0;
  if (ctx->time) {
    fprintf(stderr,
      "%s:WARNING: Tempo change at %d ms, from %d ms/qnote to %d ms/qnote, may skew beat reporting and effects.\n",
      ctx->res->path,ctx->time,oldtempo,newtempo
    );
    return 0;
  }
  ctx->hdr[4]=newtempo>>8;
  ctx->hdr[5]=newtempo;
  return 0;
}

/* Egg from MIDI: Delay.
 * TODO For now, we're emitting nonzero delays as soon as we encounter them.
 * Since we discard some events, we might be able to tighten up output by condensing delays.
 * Not worried about it for now.
 */
 
static int eggdev_song_receive_midi_delay(
  struct eggdev_song_egg_from_midi *ctx,
  int ms
) {
  //fprintf(stderr,"%s %d\n",__func__,ms);
  if (ms<1) return 0;
  if (ctx->time>INT_MAX-ms) {
    fprintf(stderr,"%s: Duration >2 Ms, about 24 days, something is broken here.\n",ctx->res->path);
    return -2;
  }
  ctx->time+=ms;
  ms+=ctx->delaycarry;
  ctx->delaycarry=ms&3;
  int c=ms>>2;
  while (c>=0x7f) {
    if (sr_encode_u8(&ctx->dst,0x7f)<0) return -1;
    c-=0x7f;
  }
  if (c>0) {
    if (sr_encode_u8(&ctx->dst,c)<0) return -1;
  }
  return 0;
}

/* Egg from MIDI: Single event.
 */
 
static int eggdev_song_receive_midi_event(struct eggdev_song_egg_from_midi *ctx,const struct midi_event *event) {
  //fprintf(stderr,"%s %02x %02x %02x %02x\n",__func__,event->chid,event->opcode,event->a,event->b);
  switch (event->opcode) {
  
    // Dispatch Channel Voice events.
    case MIDI_OPCODE_NOTE_OFF: return eggdev_song_receive_midi_note_off(ctx,event);
    case MIDI_OPCODE_NOTE_ON: return eggdev_song_receive_midi_note_on(ctx,event);
    case MIDI_OPCODE_WHEEL: return eggdev_song_receive_midi_wheel(ctx,event);
      
    // Apply selected Control Changes and Program Change to channel headers.
    case MIDI_OPCODE_PROGRAM: return eggdev_song_receive_midi_header(ctx,event->chid,event->a,29);
    case MIDI_OPCODE_CONTROL: switch (event->a) {
        case 0x00: return eggdev_song_receive_midi_header(ctx,event->chid,event->b,15);
        case 0x07: return eggdev_song_receive_midi_header(ctx,event->chid,event->b,1);
        case 0x0a: return eggdev_song_receive_midi_header(ctx,event->chid,event->b,8);
        case 0x10: return eggdev_song_receive_midi_header(ctx,event->chid,event->b,36);
        case 0x11: return eggdev_song_receive_midi_header(ctx,event->chid,event->b,43);
        case 0x12: return eggdev_song_receive_midi_header(ctx,event->chid,event->b,50);
        case 0x13: return eggdev_song_receive_midi_header(ctx,event->chid,event->b,57);
        case 0x20: return eggdev_song_receive_midi_header(ctx,event->chid,event->b,22);
      } break;
    
    // Record tempo changes.
    case MIDI_OPCODE_META: switch (event->a) {
        case 0x51: return eggdev_song_receive_midi_tempo(ctx,event->v,event->c);
      } break;
      
    // Everything else, eg Aftertouch and general Control Change, is unused by Egg.
  }
  return 0;
}

/* Egg from MIDI.
 */
 
static int eggdev_song_egg_from_midi(struct eggdev_song_egg_from_midi *ctx) {

  /* Make the MIDI file reader.
   * A rate of 1000 means timing is reported in milliseconds, which is natural for Egg songs.
   */
  if (!(ctx->midi=midi_file_new(ctx->res->serial,ctx->res->serialc,1000))) {
    fprintf(stderr,"%s: Unspecified error decoding MIDI file\n",ctx->res->path);
    return -2;
  }
  
  /* Read events and emit directly into (dst).
   * Update (hdr) as info becomes available.
   */
  for (;;) {
    struct midi_event event;
    int err=midi_file_next(&event,ctx->midi);
    if (err<0) {
      if (midi_file_is_finished(ctx->midi)) break;
      fprintf(stderr,"%s: Error streaming events from MIDI file\n",ctx->res->path);
      return -2;
    }
    if (err) {
      int ms=err;
      if ((err=eggdev_song_receive_midi_delay(ctx,ms))<0) return err;
      if (midi_file_advance(ctx->midi,ms)<0) {
        if (midi_file_is_finished(ctx->midi)) break;
        fprintf(stderr,"%s: Error advancing MIDI stream clock\n",ctx->res->path);
        return -2;
      }
    } else {
      if ((err=eggdev_song_receive_midi_event(ctx,&event))<0) {
        if (err!=-2) fprintf(stderr,
          "%s: Unspecified error applying event {0x%02x,0x%02x,0x%02x,0x%02x}\n",
          ctx->res->path,event.chid,event.opcode,event.a,event.b
        );
        return -2;
      }
    }
  }
  
  if (ctx->notec>0) {
    fprintf(stderr,"%s: %d notes remain held:\n",ctx->res->path,ctx->notec);
    const struct note *note=ctx->notev;
    int i=ctx->notec;
    for (;i-->0;note++) {
      fprintf(stderr,"  %d: 0x%02x @ %d/%d\n",note->chid,note->noteid,note->ontime,ctx->time);
    }
    return -2;
  }
  
  /* Fail if we didn't get any notes or didn't advance the clock at all.
   * Easy to detect by checking hdr[6], the count of allocated channels.
   */
  if (!ctx->hdr[6]) {
    fprintf(stderr,"%s: Song does not contain any notes.\n",ctx->res->path);
    return -2;
  }
  if (ctx->time<1) {
    fprintf(stderr,"%s: Zero duration song.\n",ctx->res->path);
    return -2;
  }
  
  /* Calculate true header length and insert to (dst).
   */
  int hdrlen=7+8*ctx->hdr[6];
  if (hdrlen>sizeof(ctx->hdr)) {
    fprintf(stderr,"%s: Internal error: hdrlen=%d size=%d chanc=%d [%s:%d]\n",ctx->res->path,hdrlen,(int)sizeof(ctx->hdr),ctx->hdr[6],__FILE__,__LINE__);
    return -2;
  }
  if (sr_encoder_insert(&ctx->dst,0,ctx->hdr,hdrlen)<0) return -1;
  
  return 0;
}

/* Emit MIDI events for all non-default fields in an Egg song channel header.
 */
 
static int eggdev_song_emit_midi_header(struct sr_encoder *dst,int chid,const uint8_t *hdr/*8*/) {
  int volume=hdr[0]&0x7f;
  int pan=hdr[1]>>1;
  int bankmsb=((hdr[1]&0x01)<<6)|(hdr[2]>>2);
  int banklsb=((hdr[2]&0x03)<<5)|(hdr[3]>>3);
  int program=((hdr[3]&0x07)<<4)|(hdr[4]>>4);
  int gp1=((hdr[4]&0x0f)<<3)|(hdr[5]>>5);
  int gp2=((hdr[5]&0x1f)<<2)|(hdr[6]>>6);
  int gp3=((hdr[6]&0x3f)<<1)|(hdr[7]>>7);
  int gp4=hdr[7]&0x7f;
  // Emit every field. Don't try to guess about defaults, since are we talking about MIDI defaults or Egg ones?
  #define CTL(k,v) { \
    if (sr_encode_u8(dst,0)<0) return -1; \
    if (sr_encode_u8(dst,0xb0|chid)<0) return -1; \
    if (sr_encode_u8(dst,k)<0) return -1; \
    if (sr_encode_u8(dst,v)<0) return -1; \
  }
  CTL(0x07,volume)
  CTL(0x0a,pan)
  CTL(0x00,bankmsb)
  CTL(0x20,banklsb)
  CTL(0x10,gp1)
  CTL(0x11,gp2)
  CTL(0x12,gp2)
  CTL(0x13,gp2)
  #undef CTL
  if (sr_encode_u8(dst,0)<0) return -1;
  if (sr_encode_u8(dst,0xc0|chid)<0) return -1;
  if (sr_encode_u8(dst,program)<0) return -1;
  return 0;
}

/* MIDI from Egg.
 */
 
static int eggdev_song_midi_from_egg(struct sr_encoder *dst,const uint8_t *src,int srcc,const char *path) {

  /* Validate header.
   */
  if ((srcc<7)||memcmp(src,"\0\xbe\xeeP",4)) {
    fprintf(stderr,"%s: Not an Egg song, signature mismatch\n",path);
    return -2;
  }
  int tempo=(src[4]<<8)|src[5];
  int chanc=src[6];
  if ((tempo<1)||(tempo&0x8000)||(chanc<1)||(chanc>8)) {
    // We're going to use tempo as division, so it is limited to 15 bits.
    // That should never come up: 32.7 seconds per beat? That's ridiculous.
    fprintf(stderr,"%s: Illegal tempo or channel count (%d,%d)\n",path,tempo,chanc);
    return -2;
  }
  int srcp=7+src[6]*8;
  if (srcp>srcc) {
    fprintf(stderr,"%s: Unexpected EOF\n",path);
    return -2;
  }
  
  /* Emit MThd.
   * To keep things simple, we'll use a division equivalent to the Egg song's tempo.
   * So in the output MIDI file, one tick is one millisecond.
   * That's probably finer than we need, and might cause unnecessarily long delay words.
   * Sparing us the hassle of converting between time regimes, I think is worth the excess.
   */
  if (sr_encode_raw(dst,"MThd\0\0\0\6",8)<0) return -1;
  if (sr_encode_intbe(dst,1,2)<0) return -1; // Format: Always 1.
  if (sr_encode_intbe(dst,1,2)<0) return -1; // Track Count: Also always 1.
  if (sr_encode_intbe(dst,tempo,2)<0) return -1; // Division.
  
  /* Begin MTrk and record its start position so we can return to insert length.
   */
  if (sr_encode_raw(dst,"MTrk\0\0\0\0",8)<0) return -1;
  int mtrkp=dst->c;
  
  /* Emit channel header events.
   */
  const uint8_t *hdr=src+7;
  int i=0; for (;i<chanc;i++,hdr+=8) {
    if (eggdev_song_emit_midi_header(dst,i,hdr)<0) return -1;
  }
  
  /* Local notes cache for Note Off events we haven't emitted yet.
   * Sort by time.
   */
  struct onnote {
    int time;
    uint8_t chid;
    uint8_t noteid;
  } onnotev[EGGDEV_SONG_NOTE_LIMIT];
  int onnotec=0;
  int time=0,delay=0; // (time) is last emitted events, (delay) is collected after that
  
  /* Emit the VLQ delay, and add (delay) to (time).
   * Caller must emit an event right after.
   */
  #define SYNCTIME { \
    if (sr_encode_vlq(dst,delay)<0) return -1; \
    time+=delay; \
    delay=0; \
  }
  
  /* Stream and reencode events.
   */
  while (srcp<srcc) {
    uint8_t lead=src[srcp++];
    if (!lead) break;
    if (!(lead&0x80)) {
      delay+=lead;
      continue;
    }
    switch (lead&0xe0) {
    
      case 0x80: { // 100cccnn nnnnnvvv Instant
          if (srcp>srcc-1) return -1;
          int chid=(lead>>2)&7;
          if (chid>=chanc) {
            fprintf(stderr,"%s: Unexpected event on channel %d, header declared only %d channels.\n",path,chid,chanc);
            return -2;
          }
          int noteid=((lead&0x03)<<5)|(src[srcp]>>3);
          int velocity=((src[srcp]&0x07)<<4);
          velocity|=(velocity>>3)|(velocity>>6);
          if (!velocity) velocity=1; // Velocity zero is special in MIDI, we must not emit it.
          srcp+=1;
          SYNCTIME
          if (sr_encode_u8(dst,0x90|chid)<0) return -1;
          if (sr_encode_u8(dst,noteid)<0) return -1;
          if (sr_encode_u8(dst,velocity)<0) return -1;
          if (sr_encode_u8(dst,0)<0) return -1; // Delay to Note Off.
          if (sr_encode_u8(dst,0x80|chid)<0) return -1;
          if (sr_encode_u8(dst,noteid)<0) return -1;
          if (sr_encode_u8(dst,0x40)<0) return -1;
        } break;
        
      case 0xa0: { // 101cccnn nnnnnvvv dddddddd Short (1ms)
          if (srcp>srcc-2) return -1;
          int chid=(lead>>2)&7;
          if (chid>=chanc) {
            fprintf(stderr,"%s: Unexpected event on channel %d, header declared only %d channels.\n",path,chid,chanc);
            return -2;
          }
          int noteid=((lead&0x03)<<5)|(src[srcp]>>3);
          int velocity=((src[srcp]&0x07)<<4);
          velocity|=(velocity>>3)|(velocity>>6);
          if (!velocity) velocity=1; // Velocity zero is special in MIDI, we must not emit it.
          int dur=src[srcp+1];
          srcp+=2;
          SYNCTIME
          if (sr_encode_u8(dst,0x90|chid)<0) return -1;
          if (sr_encode_u8(dst,noteid)<0) return -1;
          if (sr_encode_u8(dst,velocity)<0) return -1;
          if (dur&&(onnotec<EGGDEV_SONG_NOTE_LIMIT)) {
            struct onnote *onnote=onnotev+onnotec++;
            onnote->time=time+dur;
            onnote->chid=chid;
            onnote->noteid=noteid;
          } else {
            if (sr_encode_u8(dst,0)<0) return -1; // Delay to Note Off.
            if (sr_encode_u8(dst,0x80|chid)<0) return -1;
            if (sr_encode_u8(dst,noteid)<0) return -1;
            if (sr_encode_u8(dst,0x40)<0) return -1;
          }
        } break;
        
      case 0xc0: { // 110cccnn nnnnnvvv dddddddd Long (16ms)
          if (srcp>srcc-2) return -1;
          int chid=(lead>>2)&7;
          if (chid>=chanc) {
            fprintf(stderr,"%s: Unexpected event on channel %d, header declared only %d channels.\n",path,chid,chanc);
            return -2;
          }
          int noteid=((lead&0x03)<<5)|(src[srcp]>>3);
          int velocity=((src[srcp]&0x07)<<4);
          velocity|=(velocity>>3)|(velocity>>6);
          if (!velocity) velocity=1; // Velocity zero is special in MIDI, we must not emit it.
          int dur=src[srcp+1]<<4;
          srcp+=2;
          SYNCTIME
          if (sr_encode_u8(dst,0x90|chid)<0) return -1;
          if (sr_encode_u8(dst,noteid)<0) return -1;
          if (sr_encode_u8(dst,velocity)<0) return -1;
          if (dur&&(onnotec<EGGDEV_SONG_NOTE_LIMIT)) {
            struct onnote *onnote=onnotev+onnotec++;
            onnote->time=time+dur;
            onnote->chid=chid;
            onnote->noteid=noteid;
          } else {
            if (sr_encode_u8(dst,0)<0) return -1; // Delay to Note Off.
            if (sr_encode_u8(dst,0x80|chid)<0) return -1;
            if (sr_encode_u8(dst,noteid)<0) return -1;
            if (sr_encode_u8(dst,0x40)<0) return -1;
          }
        } break;
        
      case 0xe0: { // 111cccww wwwwwwww Wheel
          if (srcp>srcc-1) return -1;
          int chid=(lead>>2)&7;
          if (chid>=chanc) {
            fprintf(stderr,"%s: Unexpected event on channel %d, header declared only %d channels.\n",path,chid,chanc);
            return -2;
          }
          int w=((lead&0x03)<<8)|src[srcp++];
          w<<=4;
          w|=w>>10;
          SYNCTIME
          if (sr_encode_u8(dst,0xe0|chid)<0) return -1;
          if (sr_encode_u8(dst,w&0x7f)<0) return -1;
          if (sr_encode_u8(dst,w>>7)<0) return -1;
        } break;
    }
  }
  
  /* Any notes remaining on, emit the appropriate delays and Note Off events.
   */
  while (onnotec>0) {
    delay+=onnotev[0].time-time;
    if (delay<0) delay=0;
    if (sr_encode_vlq(dst,delay)<0) return -1;
    time=onnotev[0].time;
    delay=0;
    if (sr_encode_u8(dst,0x80|onnotev[0].chid)<0) return -1;
    if (sr_encode_u8(dst,onnotev[0].noteid)<0) return -1;
    if (sr_encode_u8(dst,0x40)<0) return -1; // Velocity. Egg songs have no concept of Off Velocity.
    onnotec--;
    memmove(onnotev,onnotev+1,sizeof(struct onnote)*(onnotec));
  }
  
  /* Track must end with a terminator.
   */
  SYNCTIME
  if (sr_encode_u8(dst,0xff)<0) return -1; // Meta
  if (sr_encode_u8(dst,0x2f)<0) return -1; // EOT
  if (sr_encode_u8(dst,0)<0) return -1; // payload length
  #undef SYNCTIME
  
  /* And finally, insert the MTrk length.
   */
  int len=dst->c-mtrkp;
  if (len<1) return -1;
  uint8_t *b=((uint8_t*)(dst->v))+mtrkp-4;
  b[0]=len>>24;
  b[1]=len>>16;
  b[2]=len>>8;
  b[3]=len;
  
  return 0;
}

/* Main entry points.
 */
 
int eggdev_compile_song(struct eggdev_res *res) {

  if ((res->serialc>=4)&&!memcmp(res->serial,"\0\xbe\xeeP",4)) return 0;

  if ((res->serialc>=4)&&!memcmp(res->serial,"MThd",4)) {
    struct eggdev_song_egg_from_midi ctx;
    eggdev_song_egg_from_midi_init(&ctx,res);
    int err=eggdev_song_egg_from_midi(&ctx);
    if (err>=0) {
      //fprintf(stderr,"%s: Compiled song %d => %d\n",res->path,res->serialc,ctx.dst.c);
      eggdev_res_handoff_serial(res,ctx.dst.v,ctx.dst.c);
      ctx.dst.v=0;
    } else if (err!=-2) {
      fprintf(stderr,"%s: Unspecified error compiling song from MIDI\n",res->path);
      err=-2;
    }
    eggdev_song_egg_from_midi_cleanup(&ctx);
    return err;
  }

  fprintf(stderr,"%s: Unknown format for song. Expected MIDI or Egg Song\n",res->path);
  return -2;
}

int eggdev_uncompile_song(struct eggdev_res *res) {

  if ((res->serialc>=4)&&!memcmp(res->serial,"\0\xbe\xeeP",4)) {
    struct sr_encoder dst={0};
    int err=eggdev_song_midi_from_egg(&dst,res->serial,res->serialc,res->path);
    if (err<0) {
      if (err!=-2) fprintf(stderr,"%s: Unspecified error converting song\n",res->path);
      sr_encoder_cleanup(&dst);
      return -2;
    }
    eggdev_res_handoff_serial(res,dst.v,dst.c);
    eggdev_res_set_format(res,"mid",3);
    return 0;
  }

  if ((res->serialc>=4)&&!memcmp(res->serial,"MThd",4)) return 0;

  fprintf(stderr,"%s: Unknown format for song. Expected MIDI or Egg Song\n",res->path);
  return -2;
}
