#include "eggdev/eggdev_internal.h"
#include "opt/image/image.h"

#define EGGDEV_FMT_PNG 1
#define EGGDEV_FMT_MIDI 5
#define EGGDEV_FMT_WAV 6
#define EGGDEV_FMT_EGS 7
#define EGGDEV_FMT_STRINGS_BIN 9
#define EGGDEV_FMT_METADATA_BIN 10
#define EGGDEV_FMT_ROM 11
#define EGGDEV_FMT_TEXT 12

/* Guess format.
 */
 
static int eggdev_cvta2a_guess_format(const char *name,int namec,const void *src,int srcc,int fromfmt) {

  /* (name) trumps all if we recognize it, and if we don't recognize it that's an error.
   */
  if (namec>0) {
    if ((namec==3)&&!memcmp(name,"png",3)) return EGGDEV_FMT_PNG;
    if ((namec==4)&&!memcmp(name,"midi",4)) return EGGDEV_FMT_MIDI;
    if ((namec==3)&&!memcmp(name,"wav",3)) return EGGDEV_FMT_WAV;
    if ((namec==3)&&!memcmp(name,"egs",3)) return EGGDEV_FMT_EGS;
    if ((namec==11)&&!memcmp(name,"strings_bin",11)) return EGGDEV_FMT_STRINGS_BIN;
    if ((namec==12)&&!memcmp(name,"metadata_bin",12)) return EGGDEV_FMT_METADATA_BIN;
    if ((namec==3)&&!memcmp(name,"rom",3)) return EGGDEV_FMT_ROM;
    if ((namec==4)&&!memcmp(name,"text",4)) return EGGDEV_FMT_TEXT;
    return 0;
  }
  
  /* If serial data was provided, look for signatures.
   */
  if (srcc>0) {
    // image...
    if ((srcc>=8)&&!memcmp(src,"\x89PNG\r\n\x1a\n",8)) return EGGDEV_FMT_PNG;
    // audio...
    if ((srcc>=4)&&!memcmp(src,"MThd",4)) return EGGDEV_FMT_MIDI;
    if ((srcc>=4)&&!memcmp(src,"RIFF",4)) return EGGDEV_FMT_WAV;
    if ((srcc>=4)&&!memcmp(src,"\0EGS",4)) return EGGDEV_FMT_EGS;
    // egg resources...
    if ((srcc>=4)&&!memcmp(src,"\0ES\xff",4)) return EGGDEV_FMT_STRINGS_BIN;
    if ((srcc>=4)&&!memcmp(src,"\0EM\xff",4)) return EGGDEV_FMT_METADATA_BIN;
    // other...
    if ((srcc>=4)&&!memcmp(src,"\0EGG",4)) return EGGDEV_FMT_ROM;
    // If the first 256 bytes are G0 and a few selected C0s, call it TEXT.
    int ckc=srcc,text=1;
    if (ckc>256) ckc=256;
    const uint8_t *ck=src;
    for (;ckc-->0;ck++) {
      if (*ck>=0x7f) { text=0; break; }
      if (*ck>=0x20) continue;
      if (*ck==0x09) continue;
      if (*ck==0x0a) continue;
      if (*ck==0x0d) continue;
      text=0;
      break;
    }
    if (text) return EGGDEV_FMT_TEXT;
  }
  
  /* Anything we can infer from the source format?
   */
  switch (fromfmt) {
    // MIDI=>EGS and EGS=>WAV. It is possible to make MIDI from EGS but you have to ask for it specifically.
    case EGGDEV_FMT_MIDI: return EGGDEV_FMT_EGS;
    case EGGDEV_FMT_EGS: return EGGDEV_FMT_WAV;
    // WAV "converts" to itself; it's a sanitization process.
    case EGGDEV_FMT_WAV: return EGGDEV_FMT_WAV;
    // Egg resources go to text. From text, we really can't guess.
    case EGGDEV_FMT_STRINGS_BIN: return EGGDEV_FMT_TEXT;
    case EGGDEV_FMT_METADATA_BIN: return EGGDEV_FMT_TEXT;
    // And some other things, we don't know what to do.
    case EGGDEV_FMT_ROM:
    case EGGDEV_FMT_TEXT:
      break;
  }

  return 0;
}

/* Convert images.
 */
 
static int eggdev_cvta2a_image(struct sr_encoder *dst,const void *src,int srcc,int dstfmt,const char *refname) {
  struct image *image=image_decode(src,srcc);
  if (!image) {
    if (!refname) return -1;
    fprintf(stderr,"%s: Failed to decode image from %d bytes\n",refname,srcc);
    return -2;
  }
  int err=image_encode(dst,image,dstfmt);
  image_del(image);
  if (err>=0) return 0;
  if (!refname) return -1;
  fprintf(stderr,"%s: Failed to reencode image\n",refname);
  return -2;
}

/* Stand a synthesizer, load this EGS file, and run it to completion, capturing raw s16le.
 */
 
static int eggdev_cvta2a_synthesize(struct sr_encoder *dst,const void *src,int srcc,int rate,int chanc,const char *refname) {
  struct synth *synth=synth_new(rate,chanc);
  if (!synth) return -1;
  synth_play_song_borrow(synth,src,srcc,0);
  int frames_per_cycle=rate; // Can be anything, really. One second per cycle sounds reasonable.
  int samples_per_cycle=frames_per_cycle*chanc;
  int bytes_per_cycle=samples_per_cycle<<1;
  while (synth_get_song(synth)) {
    if (dst->c>=100<<20) { // 100 MB sanity limit
      fprintf(stderr,"%s:ERROR: Exceeded output sanity limit.\n",__func__);
      synth_del(synth);
      return -2;
    }
    if (sr_encoder_require(dst,bytes_per_cycle)<0) {
      synth_del(synth);
      return -1;
    }
    synth_updatei((int16_t*)(dst->v+dst->c),samples_per_cycle,synth);
    dst->c+=bytes_per_cycle;
  }
  synth_del(synth);
  return 0;
}

/* Add WAV headers to a raw s16le pcm dump.
 */

static int eggdev_cvta2a_prepend_wav_header(struct sr_encoder *dst,int p,int rate,int chanc) {
  int dlen=dst->c-p;
  int flen=4+8+16+8+dlen;
  int brate=(rate*chanc)<<1;
  uint8_t pre[44]={ // 12 RIFF header: "RIFF" + (u32)filelen + "WAVE"; 8 fmt hdr; 16 fmt; 8 data hdr.
    'R','I','F','F',flen,flen>>8,flen>>16,flen>>24,'W','A','V','E',
    'f','m','t',' ',16,0,0,0,
      1,0, // format: 1=LPCM
      chanc,0,
      rate,rate>>8,rate>>16,rate>>24,
      brate,brate>>8,brate>>16,brate>>24,
      chanc<<1,0, // frame size in bytes
      16,0, // sample size in bits
    'd','a','t','a',dlen,dlen>>8,dlen>>16,dlen>>24,
  };
  if (sr_encoder_insert(dst,p,pre,sizeof(pre))<0) return -1;
  return 0;
}

/* Convert to WAV from a logical song format (EGS,MIDI).
 * This means creating a synthesizer and capturing its output.
 * We don't have the means of asking for rate or channel count, so we assume 44.1 kHz mono always.
 * (we're equipped to capture stereo, because an earlier version of Egg supported that).
 */
 
static int eggdev_cvta2a_print_song(struct sr_encoder *dst,const void *src,int srcc,int dfmt,int sfmt,const char *refname) {
  int err;

  /* If the input is MIDI, we must first convert it to EGS.
   */
  struct sr_encoder srccvt={0};
  if (sfmt==EGGDEV_FMT_MIDI) {
    if ((err=eggdev_song_egs_from_midi(&srccvt,src,srcc,refname))<0) {
      sr_encoder_cleanup(&srccvt);
      return err;
    }
    sfmt=EGGDEV_FMT_EGS;
    src=srccvt.v;
    srcc=srccvt.c;
  }
  
  /* Call out for the interesting bit.
   */
  int rate=44100;
  int chanc=1;
  int dstc0=dst->c;
  if ((err=eggdev_cvta2a_synthesize(dst,src,srcc,rate,chanc,refname))<0) {
    sr_encoder_cleanup(&srccvt);
    if (err!=-2) fprintf(stderr,"%s: Error running synthesizer.\n",refname);
    return -2;
  }
  
  /* Add the appropriate headers to (dst).
   */
  switch (dfmt) {
    case EGGDEV_FMT_WAV: err=eggdev_cvta2a_prepend_wav_header(dst,dstc0,rate,chanc); break;
    default: err=-1;
  }
  sr_encoder_cleanup(&srccvt);
  return err;
}

/* Convert anything-to-anything, main entry point.
 */
 
int eggdev_cvta2a(
  struct sr_encoder *dst,
  const void *src,int srcc,
  const char *refname,
  const char *dstfmt,int dstfmtc,
  const char *srcfmt,int srcfmtc
) {
  if (!src||(srcc<0)) srcc=0;
  if (!dstfmt) dstfmtc=0; else if (dstfmtc<0) { dstfmtc=0; while (dstfmt[dstfmtc]) dstfmtc++; }
  if (!srcfmt) srcfmtc=0; else if (srcfmtc<0) { srcfmtc=0; while (srcfmt[srcfmtc]) srcfmtc++; }
  int sfmt=eggdev_cvta2a_guess_format(srcfmt,srcfmtc,src,srcc,0);
  int dfmt=eggdev_cvta2a_guess_format(dstfmt,dstfmtc,0,0,sfmt);
  //fprintf(stderr,"%s srcc=%d refname=%s dstfmt='%.*s'(%d) srcfmt='%.*s'(%d)\n",__func__,srcc,refname,dstfmtc,dstfmt,dfmt,srcfmtc,srcfmt,sfmt);
  
  /* Same to same, there may be a sanitization process we can run.
   * But in most cases, just echo the input.
   */
  if (sfmt==dfmt) switch (sfmt) {
    case EGGDEV_FMT_WAV: return eggdev_song_sanitize_wav(dst,src,srcc,refname);
    //case EGGDEV_FMT_MIDI: return eggdev_song_sanitize_midi(dst,src,srcc,refname);//TODO Do we want this? It could add default EGS headers like compile, but stay in MIDI format.
    default: return sr_encode_raw(dst,src,srcc);
  }
  
  switch (dfmt) {
    case EGGDEV_FMT_PNG: return eggdev_cvta2a_image(dst,src,srcc,IMAGE_FORMAT_png,refname);
    case EGGDEV_FMT_MIDI: switch (sfmt) {
        case EGGDEV_FMT_EGS: return eggdev_song_midi_from_egs(dst,src,srcc,refname);
      } break;
    case EGGDEV_FMT_WAV: switch (sfmt) {
        case EGGDEV_FMT_MIDI: return eggdev_cvta2a_print_song(dst,src,srcc,dfmt,sfmt,refname);
        case EGGDEV_FMT_EGS: return eggdev_cvta2a_print_song(dst,src,srcc,dfmt,sfmt,refname);
      } break;
    case EGGDEV_FMT_EGS: switch (sfmt) {
        case EGGDEV_FMT_MIDI: return eggdev_song_egs_from_midi(dst,src,srcc,refname);
      } break;
    case EGGDEV_FMT_STRINGS_BIN: switch (sfmt) {
        case EGGDEV_FMT_TEXT: return eggdev_strings_bin_from_text(dst,src,srcc,refname);
      } break;
    case EGGDEV_FMT_METADATA_BIN: switch (sfmt) {
        case EGGDEV_FMT_TEXT: return eggdev_metadata_bin_from_text(dst,src,srcc,refname);
      } break;
    case EGGDEV_FMT_ROM: break; // cvta2a is not the right tool for manipulating roms.
    case EGGDEV_FMT_TEXT: switch (sfmt) {
        case EGGDEV_FMT_STRINGS_BIN: eggdev_strings_text_from_bin(dst,src,srcc,refname);
        case EGGDEV_FMT_METADATA_BIN: eggdev_metadata_bin_from_text(dst,src,srcc,refname);
      } break;
  }
  if (refname) {
    fprintf(stderr,"%s: No handler for conversion. srcfmt='%.*s',%d. dstfmt='%.*s',%d. srcc=%d\n",refname,srcfmtc,srcfmt,sfmt,dstfmtc,dstfmt,dfmt,srcc);
    return -2;
  }
  return -1;
}
