#include "synth_formats.h"
#include "opt/serial/serial.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <stdlib.h>

/* In theory, there's no limit to how long a delay event can go, we can chain them forever.
 * But 20 seconds between events? Sounds fishy.
 */
#define SYNTH_UNREASONABLE_DELAY_MS 20000

/* Channel Header field metadata.
 */
 
#define SYNTH_CHHDR_ARG_LIMIT 8
#define A_NONE 0
#define A_U08 1
#define A_S08 2
#define A_U16 3
#define A_S8 4
#define A_U8 5
#define A_U016 6
#define A_U88 7
#define A_ENV 8
#define A_S016V 9
#define A_U016V 10
 
static const struct synth_chhdr_meta {
  uint8_t opcode;
  const char *name;
  uint8_t argfmtv[SYNTH_CHHDR_ARG_LIMIT];
  const char *argnamev[SYNTH_CHHDR_ARG_LIMIT];
} synth_chhdr_metav[]={
  {0x00,"noop"      ,{0},{0}},
  {0x01,"master"    ,{A_U08},{"trim"}},
  {0x02,"pan"       ,{A_S08},{"pan"}},
  {0x03,"drums"     ,{A_U16,A_S8,A_U08,A_U08},{"rid","bias","trimlo","trimhi"}},
  {0x04,"wheel"     ,{A_U16},{"cents"}},
  {0x05,"sub"       ,{A_U16},{"width(hz)"}},
  {0x06,"shape"     ,{A_U8},{"shape"}},
  {0x07,"harmonics" ,{A_U016V},{"coefficients"}},
  {0x08,"fm"        ,{A_U88,A_U88},{"rate","range"}},
  {0x09,"fmenv"     ,{A_ENV},{"env"}},
  {0x0a,"fmlfo"     ,{A_U16,A_U016,A_U08},{"ms","depth","phase"}},
  {0x0b,"pitchenv"  ,{A_ENV},{"env"}},
  {0x0c,"pitchlfo"  ,{A_U16,A_U16,A_U08},{"ms","cents","phase"}},
  {0x0d,"level"     ,{A_ENV},{"env"}},
  {0x80,"gain"      ,{A_U88,A_U08,A_U08},{"gain","clip","gate"}},
  {0x81,"waveshaper",{A_S016V},{"levels"}},
  {0x82,"delay"     ,{A_U16,A_U08,A_U08,A_U08,A_U08},{"ms","dry","wet","store","feedback"}},
//  {0x83,"detune"    ,{A_U16,A_U16,A_U08},{"ms","cents","phase"}}, // Removed. 0x83 is available.
  {0x84,"tremolo"   ,{A_U16,A_U016,A_U08},{"ms","depth","phase"}},
  {0x85,"lopass"    ,{A_U16},{"hz"}},
  {0x86,"hipass"    ,{A_U16},{"hz"}},
  {0x87,"bpass"     ,{A_U16,A_U16},{"hz","width"}},
  {0x88,"notch"     ,{A_U16,A_U16},{"hz","width"}},
};

static int synth_chhdr_opcode_eval(const char *src,int srcc) {
  if (!src) return -1;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  const struct synth_chhdr_meta *meta=synth_chhdr_metav;
  int i=sizeof(synth_chhdr_metav)/sizeof(synth_chhdr_metav[0]);
  for (;i-->0;meta++) {
    if (memcmp(meta->name,src,srcc)) continue;
    if (meta->name[srcc]) continue;
    return meta->opcode;
  }
  int n;
  if ((sr_int_eval(&n,src,srcc)>=2)&&(n>=0)&&(n<=0xff)) return n;
  return -1;
}

static const char *synth_chhdr_opcode_repr(int opcode) {
  const struct synth_chhdr_meta *meta=synth_chhdr_metav;
  int i=sizeof(synth_chhdr_metav)/sizeof(synth_chhdr_metav[0]);
  for (;i-->0;meta++) {
    if (opcode==meta->opcode) return meta->name;
  }
  return 0;
}

/* Note id.
 */
 
static int synth_noteid_eval(const char *src,int srcc) {
  if (!src) return -1;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  if (!srcc) return -1;
  
  // Friendly form: [a-gA-G][#sb]?(-1|[0-9])
  // MIDI notes run from 0=c-1 thru 127=g9
  // Beware! Octaves run C,D,E,F,G,A,B, not A,B,C,D,E,F,G.
  // That's what happens when you put musicians in charge of defining a spec.
  int tone=-1;
  switch (src[0]) {
    case 'c': case 'C': tone=0; break;
    case 'd': case 'D': tone=2; break;
    case 'e': case 'E': tone=4; break;
    case 'f': case 'F': tone=5; break;
    case 'g': case 'G': tone=7; break;
    case 'a': case 'A': tone=9; break;
    case 'b': case 'B': tone=11; break;
  }
  if (tone>=0) {
    int srcp=1;
    if (srcp<srcc) {
      if ((src[srcp]=='#')||(src[srcp]=='s')) { tone++; srcp++; }
      else if (src[srcp]=='b') { tone--; srcp++; }
    }
    int octave=-2;
    if ((srcp==srcc-2)&&(src[srcp]=='-')&&(src[srcp+1]=='1')) {
      octave=-1;
      srcp=srcc;
    } else if ((srcp==srcc-1)&&(src[srcp]>='0')&&(src[srcp]<='9')) {
      octave=src[srcp]-'0';
      srcp=srcc;
    }
    if ((srcp==srcc)&&(octave>=-1)) {
      int noteid=(octave+1)*12+tone;
      if ((noteid>=0)&&(noteid<0x80)) return noteid;
    }
  }
  
  // Also allow plain integers in 0..127.
  int noteid=-1;
  if (sr_int_eval(&noteid,src,srcc)<2) return -1;
  if ((noteid>=0)&&(noteid<0x80)) return noteid;
  return -1;
}

static int synth_noteid_repr(char *dst,int dsta,int noteid) {
  if (!dst||(dsta<0)) dsta=0;
  if ((noteid<0)||(noteid>0x7f)) return -1;
  int octave=noteid/12-1;
  int tone=noteid%12;
  char name;
  int dstc=0;
  #define OUT(ch) { if (dstc<dsta) dst[dstc]=(ch); dstc++; }
  // I'm phrasing all the accidentals as flat, because "b" looks nicer than "#" or "s".
  switch (tone) {
    case 0: OUT('c') break;
    case 1: OUT('d') OUT('b') break;
    case 2: OUT('d') break;
    case 3: OUT('e') OUT('b') break;
    case 4: OUT('e') break;
    case 5: OUT('f') break;
    case 6: OUT('g') OUT('b') break;
    case 7: OUT('g') break;
    case 8: OUT('a') OUT('b') break;
    case 9: OUT('a') break;
    case 10: OUT('b') OUT('b') break;
    case 11: OUT('b') break;
  }
  if (octave<0) {
    OUT('-')
    OUT('1')
  } else if (octave<10) {
    OUT('0'+octave)
  } else return -1;
  #undef OUT
  if (dstc<dsta) dst[dstc]=0;
  return dstc;
}

/* Fully-Qualified Program ID.
 */
 
static int synth_fqpid_eval(const char *src,int srcc) {
  int i=0;
  for (;i<srcc;i++) {
    if (src[i]==':') {
      int bank,pid;
      if ((sr_int_eval(&bank,src,i)<2)||(bank<0)||(bank>0x3fff)) return -1;
      if ((sr_int_eval(&pid,src+i+1,srcc-i-1)<2)||(pid<0)||(pid>0x7f)) return -1;
      return (bank<<7)|pid;
    }
  }
  if (sr_int_eval(&i,src,srcc)<2) return -1;
  if ((i<0)||(i>0x1fffff)) return -1;
  return i;
}

/* Compile EGS events.
 */
 
static int synth_egs_compile_event_delay(struct sr_encoder *dst,const char *src,int srcc,const char *path,int lineno) {
  int ms;
  // We allow zero, and don't emit anything.
  // Also a high sanity limit, just to help detect mistakes.
  if ((sr_int_eval(&ms,src,srcc)<2)||(ms<0)||(ms>SYNTH_UNREASONABLE_DELAY_MS)) {
    if (!path) return -1;
    fprintf(stderr,"%s:%d: Expected delay in milliseconds, found '%.*s'\n",path,lineno,srcc,src);
    return -2;
  }
  if (ms<1) return 0;
  // Coarse delay (01xxxxxx) is 128..4096 inclusive, in increments of 64.
  while (ms>4096) {
    if (sr_encode_u8(dst,0x7f)<0) return -1;
    ms-=4096;
  }
  if (ms>=64) {
    if (sr_encode_u8(dst,0x40|((ms>>6)-1))<0) return -1;
    ms&=0x3f;
  }
  // Fine delay (00xxxxxx) is 1..63.
  if (ms>0) {
    if (sr_encode_u8(dst,ms)<0) return -1;
  }
  return 0;
}
 
static int synth_egs_compile_event_note(struct sr_encoder *dst,const char *src,int srcc,const char *path,int lineno) {
  int srcp=0,tokenc;
  const char *token;
  
  token=src+srcp;
  tokenc=0;
  while ((srcp<srcc)&&((unsigned char)src[srcp++]>0x20)) tokenc++;
  while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
  int chid;
  if ((sr_int_eval(&chid,token,tokenc)<2)||(chid<0)||(chid>=0x10)) {
    if (!path) return -1;
    fprintf(stderr,"%s:%d: Expected channel id in 0..15, found '%.*s'\n",path,lineno,tokenc,token);
    return -2;
  }
  
  token=src+srcp;
  tokenc=0;
  while ((srcp<srcc)&&((unsigned char)src[srcp++]>0x20)) tokenc++;
  while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
  int noteid=synth_noteid_eval(token,tokenc);
  if (noteid<0) {
    if (!path) return -1;
    fprintf(stderr,"%s:%d: Expected noteid (0..127 or eg 'a#4'), found '%.*s'\n",path,lineno,tokenc,token);
    return -2;
  }
  
  token=src+srcp;
  tokenc=0;
  while ((srcp<srcc)&&((unsigned char)src[srcp++]>0x20)) tokenc++;
  while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
  int velocity;
  if (!tokenc) {
    velocity=0x40;
  } else if ((sr_int_eval(&velocity,token,tokenc)<2)||(velocity<0)||(velocity>0x7f)) {
    if (!path) return -1;
    fprintf(stderr,"%s:%d: Expected velocity in 0..127, found '%.*s'\n",path,lineno,tokenc,token);
    return -2;
  }
  
  token=src+srcp;
  tokenc=0;
  while ((srcp<srcc)&&((unsigned char)src[srcp++]>0x20)) tokenc++;
  while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
  int duration;
  if (!tokenc) {
    duration=0;
  } else if ((sr_int_eval(&duration,token,tokenc)<2)||(duration<0)) {
    if (!path) return -1;
    fprintf(stderr,"%s:%d: Expected duration in ms, found '%.*s'\n",path,lineno,tokenc,token);
    return -2;
  }
  
  if (srcp<srcc) {
    if (!path) return -1;
    fprintf(stderr,"%s:%d: Unexpected tokens after 'note' event: '%.*s'\n",path,lineno,srcc-srcp,src+srcp);
    return -2;
  }
  
  /* We accept any duration >=0. But what we're able to emit is actually pretty limited:
   * 0..15     : 1000cccc nnnnnnnv vvvvvvxx : Fire and Forget. (c) chid, (n) noteid, (v) velocity.
   * 16..512   : 1001cccc nnnnnnnv vvvttttt : Short Note. (c) chid, (n) noteid, (v) velocity 4-bit, (t) hold time (t+1)*16ms
   * 128..4096 : 1010cccc nnnnnnnv vvvttttt : Long Note. (c) chid, (n) noteid, (v) velocity 4-bit, (t) hold time (t+1)*128ms
   */
  if (duration<16) { // Zero duration and high-resolution velocity.
    if (sr_encode_u8(dst,0x80|chid)<0) return -1;
    if (sr_encode_u8(dst,(noteid<<1)|(velocity>>6))<0) return -1;
    if (sr_encode_u8(dst,(velocity<<2))<0) return -1;
  } else if (duration<=512) { // Short Note.
    if (sr_encode_u8(dst,0x90|chid)<0) return -1;
    if (sr_encode_u8(dst,(noteid<<1)|(velocity>>6))<0) return -1;
    if (sr_encode_u8(dst,((velocity<<2)&0xe0)|((duration>>4)-1))<0) return -1;
  } else { // Long Note.
    if (duration>4096) {
      if (path) fprintf(stderr,"%s:%d:WARNING: Clamping note duration from %d to 4096 ms.\n",path,lineno,duration);
      duration=4096;
    }
    if (sr_encode_u8(dst,0xa0|chid)<0) return -1;
    if (sr_encode_u8(dst,(noteid<<1)|(velocity>>6))<0) return -1;
    if (sr_encode_u8(dst,((velocity<<2)&0xe0)|((duration>>7)-1))<0) return -1;
  }
  
  return 0;
}
 
static int synth_egs_compile_event_wheel(struct sr_encoder *dst,const char *src,int srcc,uint8_t *state,const char *path,int lineno) {
  int srcp=0,tokenc;
  const char *token;
  
  token=src+srcp;
  tokenc=0;
  while ((srcp<srcc)&&((unsigned char)src[srcp++]>0x20)) tokenc++;
  while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
  int chid;
  if ((sr_int_eval(&chid,token,tokenc)<2)||(chid<0)||(chid>=0x10)) {
    if (!path) return -1;
    fprintf(stderr,"%s:%d: Expected channel id in 0..15, found '%.*s'\n",path,lineno,tokenc,token);
    return -2;
  }
  
  token=src+srcp;
  tokenc=0;
  while ((srcp<srcc)&&((unsigned char)src[srcp++]>0x20)) tokenc++;
  while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
  double norm;
  if ((sr_double_eval(&norm,token,tokenc)<0)||(norm<-1.0)||(norm>1.0)) {
    if (!path) return -1;
    fprintf(stderr,"%s:%d: Expected wheel position in -1..1, found '%.*s'\n",path,lineno,tokenc,token);
    return -2;
  }
  
  if (srcp<srcc) {
    if (!path) return -1;
    fprintf(stderr,"%s:%d: Unexpected tokens after 'wheel' event: '%.*s'\n",path,lineno,srcc-srcp,src+srcp);
    return -2;
  }
  
  int iv=(int)((norm+1.0)*128.0);
  if (iv<0) iv=0; else if (iv>0xff) iv=0xff;
  if (iv==state[chid]) {
    //TODO Does this even warrant a warning?
    if (path) fprintf(stderr,"%s:%d:WARNING: Dropping redundant 'wheel' event.\n",path,lineno);
    return 0;
  }
  state[chid]=iv;
  
  if (sr_encode_u8(dst,0xb0|chid)<0) return -1;
  if (sr_encode_u8(dst,iv)<0) return -1;
  
  return 0;
}

/* EGS from text, usually from inside MSF text, and without the 'song' or 'sound' introducer.
 */
 
int synth_egs_from_text(struct sr_encoder *dst,const char *src,int srcc,int channel_header_only,const char *path,int lineno) {
  struct sr_decoder decoder={.v=src,.c=srcc};
  const char *line;
  int linec;
  int chid=-1; // -1=init, -2=events, >=0=channel currently configuring
  int chlenp=0;
  uint8_t wheelstate[16]={0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80};
  
  if (channel_header_only) {
    chid=0;
  } else {
    if (sr_encode_raw(dst,"\0EGS",4)<0) return -1;
  }
  
  #define TERMCH { \
    if (!channel_header_only&&(chid>=0)) { \
      int len=dst->c-chlenp-2; \
      if (len<1) { \
        if (sr_encode_raw(dst,"\0\0",2)<0) return -1; \
        len=2; \
      } \
      if (len>0xffff) { \
        if (!path) return -1; \
        fprintf(stderr,"%s:%d: Channel Header too long (%d, limit 65535)\n",path,lineno,len); \
        return -2; \
      } \
      ((uint8_t*)dst->v)[chlenp]=len>>8; \
      ((uint8_t*)dst->v)[chlenp+1]=len; \
    } \
  }
  
  for (;(linec=sr_decode_line(&line,&decoder))>0;lineno++) {
    while (linec&&((unsigned char)line[linec-1]<=0x20)) linec--;
    while (linec&&((unsigned char)line[0]<=0x20)) { linec--; line++; }
    if (!linec||(line[0]=='#')) continue;
    
    const char *kw=line;
    int kwc=0,linep=0;
    while ((linep<linec)&&((unsigned char)line[linep++]>0x20)) kwc++;
    while ((linep<linec)&&((unsigned char)line[linep]<=0x20)) linep++;
    
    // "channel" begins a Channel Header and ends current one if any.
    if (!channel_header_only&&(kwc==7)&&!memcmp(kw,"channel",7)) {
      if (chid==-2) return -1;
      TERMCH
      int nchid;
      if ((sr_int_eval(&nchid,line+linep,linec-linep)<2)||(nchid<=chid)||(nchid>=16)) {
        if (!path) return -1;
        fprintf(stderr,"%s:%d: Expected channel id in %d..15, found '%.*s'\n",path,lineno,chid+1,linec-linep,line+linep);
        return -2;
      }
      int delta=nchid-chid;
      while (delta>1) {
        if (sr_encode_raw(dst,"\0\2\0\0",4)<0) return -1;
        delta--;
      }
      chid=nchid;
      chlenp=dst->c;
      if (sr_encode_raw(dst,"\0\0",2)<0) return -1;
      continue;
    }
    
    // "events" begins the Event stream, and ends current Channel Header if any.
    if (!channel_header_only&&(kwc==6)&&!memcmp(kw,"events",6)) {
      if (chid==-2) return -1;
      TERMCH
      if (sr_encode_raw(dst,"\0\0",2)<0) return -1;
      chid=-2;
      continue;
    }
    
    // From initial state, next line must be "channel" or "events".
    if (chid==-1) {
      if (!path) return -1;
      fprintf(stderr,"%s:%d: Expected 'channel' or 'events', found '%.*s'\n",path,lineno,kwc,kw);
      return -2;
    }
    
    // Three commands are valid in Event stream.
    if (chid<0) {
      int err;
      if ((kwc==5)&&!memcmp(kw,"delay",5)) {
        if ((err=synth_egs_compile_event_delay(dst,line+linep,linec-linep,path,lineno))<0) return err;
      } else if ((kwc==4)&&!memcmp(kw,"note",4)) {
        if ((err=synth_egs_compile_event_note(dst,line+linep,linec-linep,path,lineno))<0) return err;
      } else if ((kwc==5)&&!memcmp(kw,"wheel",5)) {
        if ((err=synth_egs_compile_event_wheel(dst,line+linep,linec-linep,wheelstate,path,lineno))<0) return err;
      } else {
        if (!path) return -1;
        fprintf(stderr,"%s:%d: Expected 'delay', 'note', or 'wheel'. Found '%.*s'\n",path,lineno,kwc,kw);
        return -2;
      }
      continue;
    }
    
    // In the Channel Header, everything is processed generically.
    int opcode=synth_chhdr_opcode_eval(kw,kwc);
    if (opcode<0) {
      if (!path) return -1;
      fprintf(stderr,"%s:%d: Not a known Channel Header opcode: '%.*s'\n",path,lineno,kwc,kw);
      return -2;
    }
    if (sr_encode_u8(dst,opcode)<0) return -1;
    int lenp=dst->c;
    if (sr_encode_u8(dst,0)<0) return -1;
    int hi=-1;
    for (;linep<linec;linep++) {
      if ((unsigned char)line[linep]<=0x20) continue;
      int digit=sr_digit_eval(line[linep]);
      if ((digit<0)||(digit>=0x10)) {
        if (!path) return -1;
        fprintf(stderr,"%s:%d: Unexpected character '%c' in hex dump.\n",path,lineno,line[linep]);
        return -2;
      }
      if (hi<0) {
        hi=digit;
      } else {
        if (sr_encode_u8(dst,(hi<<4)|digit)<0) return -1;
        hi=-1;
      }
    }
    if (hi>=0) {
      if (!path) return -1;
      fprintf(stderr,"%s:%d: Uneven count of digits\n",path,lineno);
      return -2;
    }
    int len=dst->c-lenp-1;
    if (len>0xff) {
      if (!path) return -1;
      fprintf(stderr,"%s:%d: Channel Header field payload too long (%d, limit 255)\n",path,lineno,len);
      return -2;
    }
    ((uint8_t*)dst->v)[lenp]=len;
  }
  TERMCH
  #undef TERMCH
  return 0;
}

/* Either egg binary, from text.
 */
 
int synth_egg_from_text(struct sr_encoder *dst,const void *src,int srcc,const char *path) {
  if (!dst||!src||(srcc<0)) return -1;
  struct synth_text_reader reader={.src=src,.srcc=srcc};
  const char *subsrc;
  int index,lineno,subsrcc;
  int pvindex=0; // Last MSF index we emitted. Haven't emitted anything if zero.
  while ((subsrcc=synth_text_reader_next(&index,&lineno,&subsrc,&reader))>0) {
    if (!index) {
      if (pvindex) { subsrcc=-1; break; }
      return synth_egs_from_text(dst,subsrc,subsrcc,0,path,lineno);
    } else if (index<=pvindex) {
      if (!path) return -1;
      fprintf(stderr,"%s:%d: Sound index %d can't come after the previous, %d.\n",path,lineno,index,pvindex);
      return -2;
    } else {
      if (!pvindex) {
        if (sr_encode_raw(dst,"\0MSF",4)<0) return -1;
      }
      int delta=index-pvindex;
      pvindex=index;
      while (delta>256) {
        if (sr_encode_raw(dst,"\xff\0\0\0",4)<0) return -1;
        delta-=256;
      }
      if (sr_encode_u8(dst,delta-1)<0) return -1;
      int lenp=dst->c;
      if (sr_encode_raw(dst,"\0\0\0",3)<0) return -1;
      int err=synth_egs_from_text(dst,subsrc,subsrcc,0,path,lineno);
      if (err<0) return err;
      int len=dst->c-lenp-3;
      if (len<0) return -1;
      if (len>0xffffff) {
        if (!path) return -1;
        fprintf(stderr,"%s:%d: Sound length %d exceeds limit 16777215.\n",path,lineno,len);
        return -2;
      }
      ((uint8_t*)dst->v)[lenp]=len>>16;
      ((uint8_t*)dst->v)[lenp+1]=len>>8;
      ((uint8_t*)dst->v)[lenp+2]=len;
    }
  }
  if (subsrcc<0) {
    if (!path) return -1;
    fprintf(stderr,"%s:%d: Malformed synth text. Check your 'song' and 'sound' framing.\n",path,reader.lineno);
    return -2;
  }
  if (!pvindex) {
    // Empty input => Empty MSF.
    if (sr_encode_raw(dst,"\0MSF",4)<0) return -1;
  }
  return 0;
}

/* Text from EGS Channel Header with no introducer line.
 */
 
static int synth_text_from_egs_header(struct sr_encoder *dst,const uint8_t *src,int srcc) {
  struct synth_song_channel_reader reader={.src=src,.srcc=srcc};
  int opcode,bodyc;
  const uint8_t *body;
  while ((bodyc=synth_song_channel_reader_next(&opcode,&body,&reader))>=0) {
    if (sr_encode_raw(dst,"  ",2)<0) return -1;
    const char *opname=synth_chhdr_opcode_repr(opcode);
    if (opname&&opname[0]) {
      if (sr_encode_raw(dst,opname,-1)<0) return -1;
    } else {
      if (sr_encode_fmt(dst,"%d",opcode)<0) return -1;
    }
    for (;bodyc-->0;body++) {
      if (sr_encode_fmt(dst," %02x",*body)<0) return -1;
    }
    if (sr_encode_u8(dst,0x0a)<0) return -1;
  }
  return 0;
}

/* Text from EGS Events with no introducer line.
 */
 
static int synth_text_from_egs_events(struct sr_encoder *dst,const uint8_t *src,int srcc) {
  struct synth_song_event event;
  int err;
  while ((err=synth_song_event_next(&event,src,srcc))>0) {
    src+=err;
    srcc-=err;
    switch (event.opcode) {
      case 0: { // Delay.
          if (sr_encode_fmt(dst,"  delay %d\n",event.duration)<0) return -1;
        } break;
      case 0x90: { // Note.
          char notename[16];
          int notenamec=synth_noteid_repr(notename,sizeof(notename),event.noteid);
          if ((notenamec<1)||(notenamec>sizeof(notename))) return -1;
          if (sr_encode_fmt(dst,"  note %d %.*s %d %d\n",event.chid,notenamec,notename,event.velocity,event.duration)<0) return -1;
        } break;
      case 0xe0: { // Wheel.
          double norm=(event.wheel-0x2000)/8192.0;
          if (sr_encode_fmt(dst,"  wheel %d %f\n",event.chid,norm)<0) return -1;
        } break;
      default: return -1;
    }
  }
  if (err<0) return -1;
  return 0;
}

/* Text from EGS with no introducer line.
 */
 
static int synth_text_from_egs(struct sr_encoder *dst,const uint8_t *src,int srcc,const char *path) {
  struct synth_song_parts parts={0};
  if (synth_song_split(&parts,src,srcc)<0) return -1;
  int chid=0,err;
  for (;chid<16;chid++) {
    const uint8_t *chv=parts.channels[chid].v;
    int chc=parts.channels[chid].c;
    while ((chc>=2)&&(chv[0]==0x00)) {
      int len=chv[1];
      chv+=2+len;
      chc-=2+len;
    }
    if (chc<2) continue;
    if (sr_encode_fmt(dst,"channel %d\n",chid)<0) return -1;
    if ((err=synth_text_from_egs_header(dst,chv,chc))<0) return err;
  }
  if (parts.eventsc>0) {
    if (sr_encode_raw(dst,"events\n",7)<0) return -1;
    if ((err=synth_text_from_egs_events(dst,parts.events,parts.eventsc))<0) return err;
  }
  return 0;
}

/* Text from either binary format.
 */

int synth_text_from_egg(struct sr_encoder *dst,const void *src,int srcc,const char *path) {
  if (!dst||(srcc<0)||(srcc&&!src)) return -1;
  
  /* Empty => Empty, why not.
   */
  if (!srcc) return 0;
  
  /* Unpack MSF with 'sound ID' before each.
   * If we encounter a member that isn't EGS, fail.
   */
  if ((srcc>=4)&&!memcmp(src,"\0MSF",4)) {
    struct synth_sounds_reader reader;
    if (synth_sounds_reader_init(&reader,src,srcc)<0) return -1;
    int index,subsrcc;
    const void *subsrc;
    while ((subsrcc=synth_sounds_reader_next(&index,&subsrc,&reader))>0) {
      if (sr_encode_fmt(dst,"\nsound %d\n",index)<0) return -1;
      if (synth_text_from_egs(dst,subsrc,subsrcc,path)<0) return -1;
    }
    if (subsrcc<0) return -1;
    return 0;
  }
  
  /* EGS with 'song' preamble.
   */
  if ((srcc>=4)&&!memcmp(src,"\0EGS",4)) {
    if (sr_encode_raw(dst,"song\n",5)<0) return -1;
    return synth_text_from_egs(dst,src,srcc,path);
  }
  
  return 0;
}

/* Quick test for noop encoded sound.
 */
 
int synth_sound_is_empty(const void *src,int srcc) {

  // Empty input is definitely empty.
  if (!src) return 1;
  if (srcc<1) return 1;
  const uint8_t *SRC=src;
  
  if ((srcc>=4)&&!memcmp(src,"\0EGS",4)) {
    // An EGS file containing only zeroes after the signature, or just the signature alone, is definitely noop.
    // Anything nonzero, assume there's real sound production.
    int srcp=4;
    for (;srcp<srcc;srcp++) {
      if (SRC[srcp]) return 0;
    }
    return 1;
  }
  
  // I guess we could do the same check for WAV? No samples, or all samples zero.
  // Seems like too much work, getting at those samples.
  return 0;
}

/* Parse text file.
 */
 
int synth_text_reader_next(int *index,int *lineno,void *dstpp,struct synth_text_reader *reader) {
  int startp=0; // Nonzero if we're reading a sound block.
  for (;;) {
    if (reader->srcp>=reader->srcc) {
      if (startp) return reader->srcc-startp;
      return 0;
    }
    reader->lineno++;
    int srcp0=reader->srcp;
    const char *line=reader->src+reader->srcp;
    int linec=0;
    while ((reader->srcp<reader->srcc)&&(reader->src[reader->srcp++]!=0x0a)) linec++;
    while (linec&&((unsigned char)line[linec-1]<=0x20)) linec--;
    while (linec&&((unsigned char)line[0]<=0x20)) { linec--; line++; }
    if (!linec||(line[0]=='#')) continue;
    
    const char *kw=line;
    int kwc=0,linep=0;
    while ((linep<linec)&&((unsigned char)line[linep++]>0x20)) kwc++;
    while ((linep<linec)&&((unsigned char)line[linep]<=0x20)) linep++;
    
    if (startp) {
      if (
        ((kwc==5)&&!memcmp(kw,"sound",5))||
        ((kwc==10)&&!memcmp(kw,"instrument",10))
      ) {
        // Unread this line, then return what we've skipped.
        reader->srcp=srcp0;
        reader->lineno--;
        // Don't let it be zero! eg if you have an empty block followed immediately by the next block.
        if (startp==srcp0) {
          if (dstpp) *(const void**)dstpp=" ";
          return 1;
        }
        return srcp0-startp;
      } else {
        continue;
      }
    }
    
    if ((kwc==5)&&!memcmp(kw,"sound",5)) {
      const char *token=line+linep;
      int tokenc=0;
      while ((linep<linec)&&((unsigned char)line[linep++]>0x20)) tokenc++;
      if (!index) return -1;
      if ((sr_int_eval(index,token,tokenc)<2)||(*index<1)||(*index>0xfff)) return -1;
      if (lineno) *lineno=reader->lineno+1;
      if (dstpp) *(const void**)dstpp=reader->src+reader->srcp;
      startp=reader->srcp;
      continue;
    }
    
    if ((kwc==10)&&!memcmp(kw,"instrument",10)) {
      const char *token=line+linep;
      int tokenc=0;
      while ((linep<linec)&&((unsigned char)line[linep++]>0x20)) tokenc++;
      int fqpid=synth_fqpid_eval(token,tokenc);
      if (fqpid<0) return -1;
      if (index) *index=fqpid;
      if (lineno) *lineno=reader->lineno+1;
      if (dstpp) *(const void**)dstpp=reader->src+reader->srcp;
      startp=reader->srcp;
      continue;
    }
    
    if ((kwc==4)&&!memcmp(kw,"song",4)) {
      if (index) *index=0;
      if (lineno) *lineno=reader->lineno+1;
      if (dstpp) *(const void**)dstpp=reader->src+reader->srcp;
      int len=reader->srcc-reader->srcp;
      reader->srcp=reader->srcc;
      return len;
    }
    
    return -1;
  }
}

/* MSF writer.
 */
 
void synth_sounds_writer_cleanup(struct synth_sounds_writer *writer) {
  sr_encoder_cleanup(&writer->dst);
}

int synth_sounds_writer_init(struct synth_sounds_writer *writer) {
  memset(writer,0,sizeof(struct synth_sounds_writer));
  if (sr_encode_raw(&writer->dst,"\0MSF",4)<0) return -1;
  return 0;
}

int synth_sounds_writer_add(struct synth_sounds_writer *writer,const void *src,int srcc,int index,const char *path) {
  if ((srcc<0)||(srcc&&!src)) return -1;
  if (srcc>0xffffff) return -1;
  if (index<=writer->pvindex) return -1;
  if (index>0xfff) return -1;
  int delta=index-writer->pvindex;
  while (delta>256) {
    if (sr_encode_raw(&writer->dst,"\xff\0\0\0",4)<0) return -1;
    delta-=256;
  }
  if (sr_encode_u8(&writer->dst,delta-1)<0) return -1;
  if (sr_encode_intbe(&writer->dst,srcc,3)<0) return -1;
  if (sr_encode_raw(&writer->dst,src,srcc)<0) return -1;
  writer->pvindex=index;
  return 0;
}

/* MSF reader.
 */

int synth_sounds_reader_init(struct synth_sounds_reader *reader,const void *src,int srcc) {
  if (!reader||(srcc<0)||(srcc&&!src)) return -1;
  reader->src=src;
  reader->srcc=srcc;
  reader->srcp=0;
  reader->index=0;
  if ((srcc>=4)&&!memcmp(src,"\0MSF",4)) {
    // Typical case, it's an MSF file. Point ourselves beyond the signature.
    // (srcp==0) will be the signal that we got a non-MSF file.
    reader->srcp=4;
  }
  return 0;
}

int synth_sounds_reader_next(
  int *index,void *dstpp,
  struct synth_sounds_reader *reader
) {
  if (reader->srcp>=reader->srcc) return 0;
  if (!reader->srcp) { // Not MSF, return the whole thing in one go.
    if (index) *index=0;
    *(const void**)dstpp=reader->src;
    reader->srcp=reader->srcc;
    return reader->srcc;
  }
  for (;;) {
    if (reader->srcp>reader->srcc-4) {
      reader->srcp=reader->srcc;
      return 0;
    }
    int delta=reader->src[reader->srcp++]+1;
    int len=reader->src[reader->srcp++]<<16;
    len|=reader->src[reader->srcp++]<<8;
    len|=reader->src[reader->srcp++];
    if (reader->srcp>reader->srcc-len) {
      reader->srcp=reader->srcc;
      return 0;
    }
    reader->index+=delta;
    if (!len) continue;
    if (index) *index=reader->index;
    *(const void**)dstpp=reader->src+reader->srcp;
    reader->srcp+=len;
    return len;
  }
}

/* Split up an EGGSND's Channel Headers and Events.
 */

int synth_song_split(struct synth_song_parts *parts,const void *src,int srcc) {
  if (!src||(srcc<4)||memcmp(src,"\0EGS",4)) return -1;
  memset(parts,0,sizeof(struct synth_song_parts));
  
  // Channel Headers in order, and the first zero length ends the set.
  const uint8_t *SRC=src;
  int srcp=4,chid=0;
  for (;;chid++) {
    if (srcp>srcc-2) { srcp=srcc; break; }
    int len=(SRC[srcp]<<8)|SRC[srcp+1];
    srcp+=2;
    if (!len) break;
    if (srcp>srcc-len) return -1;
    if (chid<16) {
      parts->channels[chid].v=SRC+srcp;
      parts->channels[chid].c=len;
    }
    srcp+=len;
  }
  
  // Everything else is Events.
  parts->events=SRC+srcp;
  parts->eventsc=srcc-srcp;
  return 0;
}

/* Iterate Channel Header.
 */
 
int synth_song_channel_reader_next(
  int *opcode,void *dstpp,
  struct synth_song_channel_reader *reader
) {
  if (reader->srcp>reader->srcc-2) return -1;
  if (opcode) *opcode=reader->src[reader->srcp];
  reader->srcp++;
  int len=reader->src[reader->srcp++];
  if (reader->srcp>reader->srcc-len) {
    reader->srcp=reader->srcc;
    return -1;
  }
  if (dstpp) *(const void**)dstpp=reader->src+reader->srcp;
  reader->srcp+=len;
  return len;
}

/* Next song event.
 */
 
int synth_song_event_next(struct synth_song_event *event,const void *src,int srcc) {
  if (!src) return 0;
  if (srcc<1) return 0;
  const uint8_t *SRC=src;
  uint8_t lead=SRC[0];
  
  // Explicit EOS?
  if (!lead) return 0;
  
  // Delays. There are two event types, and both are single bytes.
  if (!(lead&0x80)) {
    int srcp=0,delay=0;
    while ((srcp<srcc)&&!(SRC[srcp]&0x80)) {
      if (SRC[srcp]&0x40) { // Coarse
        delay+=((SRC[srcp]&0x3f)+1)*64;
      } else if (!SRC[srcp]) { // EOS, stop reading.
        break;
      } else { // Fine
        delay+=SRC[srcp];
      }
      srcp++;
    }
    event->opcode=0;
    event->duration=delay;
    return srcp;
  }
  
  // The three Note forms, and Wheel, are distinguished by their 4 high bits.
  switch (lead&0xf0) {
    case 0x80:
    case 0x90:
    case 0xa0: { // FF, Short, and Long. Shapes are mostly similar.
        if (srcc<3) return -1;
        event->opcode=0x90;
        event->chid=lead&0x0f;
        event->noteid=SRC[1]>>1;
        if ((lead&0xf0)==0x80) { // Fire and Forget: High resolution velocity and fixed zero duration.
          event->velocity=((SRC[1]&1)<<6)|(SRC[2]>>2);
          event->duration=0;
        } else { // Short and Long: 4-bit velocity and 5-bit duration.
          event->velocity=((SRC[1]&1)<<6)|((SRC[2]&0xe0)>>2);
          event->velocity|=event->velocity>>4;
          event->duration=(SRC[2]&0x1f);
          if ((lead&0xf0)==0x90) { // Short
            event->duration=(event->duration+1)*16;
          } else { // Long
            event->duration=(event->duration+1)*128;
          }
        }
      } return 3;
      
    case 0xb0: { // Wheel.
        if (srcc<2) return -1;
        event->opcode=0xe0;
        event->chid=lead&0x0f;
        event->wheel=SRC[1]<<6;
        event->wheel|=((event->wheel&0x1fe0)>>7); // Ignore the MSB, so 0 becomes 0, 0x80 becomes 0x2000, and 0xff becomes 0x3fff.
      } return 2;
  }
  return -1; // eg Reserved event.
}
