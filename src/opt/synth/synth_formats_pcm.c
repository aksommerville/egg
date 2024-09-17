#include "synth_internal.h"

/* Generic header for all PCM file formats.
 */
 
#define SYNTH_SAMPLEFORMAT_SLPCM 1 /* Signed linear PCM. */
#define SYNTH_SAMPLEFORMAT_ULPCM 2 /* Unsigned linear PCM. */
 
struct synth_pcmfile_header {
  int rate; // hz
  int chanc;
  int samplesize; // bits
  int sampleformat;
  int framec;
};

/* WAV.
 */
 
static int synth_pcmfile_is_wav(const uint8_t *src,int srcc) {
  if (srcc<12) return 0;
  if (memcmp(src,"RIFF",4)) return 0;
  if (memcmp(src+8,"WAVE",4)) return 0;
  return 1;
}

static int synth_pcmfile_header_decode_wav(struct synth_pcmfile_header *header,const uint8_t *src,int srcc) {
  int srcp=12,stopp=srcc-8;
  header->framec=0; // Gather first in bytes.
  header->samplesize=0; // Use as a flag for "fmt " present.
  while (srcp<=stopp) {
    const uint8_t *blocktype=src+srcp;
    int blocklen=src[srcp+4]|(src[srcp+5]<<8)|(src[srcp+6]<<16)|(src[srcp+7]<<24);
    srcp+=8;
    if ((blocklen<0)||(srcp>srcc-blocklen)) break;
    if (!memcmp(blocktype,"fmt ",4)) {
      if (blocklen<16) return -1;
      const uint8_t *block=src+srcp;
      int sfmt=block[0]|(block[1]<<8);
      int chanc=block[2]|(block[3]<<8);
      int rate=block[4]|(block[5]<<8)|(block[6]<<16)|(block[7]<<24);
      int ssize=block[14]|(block[15]<<8);
      if (sfmt!=1) return -1; // LPCM only.
      if (chanc<1) return -1;
      if ((rate<200)||(rate>200000)) return -1; // Arbitrary sanity limits.
      if (ssize!=16) return -1; // Only 16-bit samples are permitted, to keep things simple here.
      header->rate=rate;
      header->chanc=chanc;
      header->samplesize=ssize;
      header->sampleformat=SYNTH_SAMPLEFORMAT_SLPCM; // If we ever allow 8-bit samples, those are ULPCM.
    } else if (!memcmp(blocktype,"data",4)) {
      header->framec+=blocklen;
    }
    srcp+=blocklen;
  }
  if (header->framec&&header->samplesize) {
    int framesize=(header->samplesize*header->chanc)>>3;
    if (framesize<1) return -1;
    if (header->framec%framesize) return -1;
    header->framec/=header->samplesize>>3;
    return 0;
  }
  return -1;
}

// No resampling. Returns emitted frame count.
static int synth_pcmfile_decode_wav_straight(
  float *dst,int dsta,
  const struct synth_pcmfile_header *header,
  const uint8_t *src,int srcc
) {
  int stride=header->chanc<<1;
  int framec=srcc/stride;
  int i=framec;
  for (;i-->0;dst++,src+=stride) {
    int16_t isample=src[0]|(src[1]<<8);
    *dst=(float)isample/32768.0f;
  }
  return framec;
}

// Read in (src) at rate (step). Round-off gets dropped between chunks. Returns emitted frame count.
static int synth_pcmfile_decode_wav_resample(
  float *dst,int dsta,
  const struct synth_pcmfile_header *header,
  double step,
  const uint8_t *src,int srcc
) {
  int stride=header->chanc<<1;
  int stopp=srcc-stride;
  double srcpf=0.0;
  int dstc=0;
  while (dstc<dsta) {
    int srcpi=lround(srcpf)*stride;
    if (srcpi>stopp) break;
    int16_t isample=src[srcpi]|(src[srcpi+1]<<8);
    dst[dstc]=(float)isample/32768.0f;
    dstc++;
    srcpf+=step;
  }
  return dstc;
}

static int synth_pcmfile_decode_wav(struct synth_pcm *dst,const struct synth_pcmfile_header *header,const uint8_t *src,int srcc) {
  int srcp=12,stopp=srcc-8,dstc=0,err;
  double step=0.0;
  if (dst->c!=header->framec) step=(double)header->framec/(double)dst->c;
  while (srcp<=stopp) {
    const uint8_t *blocktype=src+srcp;
    int blocklen=src[srcp+4]|(src[srcp+5]<<8)|(src[srcp+6]<<16)|(src[srcp+7]<<24);
    srcp+=8;
    if ((blocklen<0)||(srcp>srcc-blocklen)) break;
    if (!memcmp(blocktype,"data",4)) {
      if (dst->c==header->framec) {
        err=synth_pcmfile_decode_wav_straight(dst->v+dstc,dst->c-dstc,header,src+srcp,blocklen);
      } else {
        err=synth_pcmfile_decode_wav_resample(dst->v+dstc,dst->c-dstc,header,step,src+srcp,blocklen);
      }
      dstc+=err;
    }
    srcp+=blocklen;
  }
  return 0;
}

/* Decode PCM file, dispatching after type detection.
 * You must decode header first, then create the synth_pcm object.
 * Provide both PCM and Header to the final decode.
 * Final decoders are expected to resample naively based on the size of the synth_pcm.
 */
 
static int synth_pcmfile_header_decode(struct synth_pcmfile_header *header,const uint8_t *src,int srcc) {
  if (synth_pcmfile_is_wav(src,srcc)) return synth_pcmfile_header_decode_wav(header,src,srcc);
  return -1;
}

static int synth_pcmfile_decode(struct synth_pcm *dst,const struct synth_pcmfile_header *header,const uint8_t *src,int srcc) {
  if (synth_pcmfile_is_wav(src,srcc)) return synth_pcmfile_decode_wav(dst,header,src,srcc);
  return -1;
}

/* Decode raw PCM formats, main entry point.
 */
 
struct synth_pcm *synth_pcm_decode(int rate,const void *src,int srcc) {
  struct synth_pcmfile_header header={0};
  if (synth_pcmfile_header_decode(&header,src,srcc)<0) return 0;
  if (header.rate<1) return 0;
  int dstframec=header.framec;
  if (rate>0) {
    double scale=(double)rate/(double)header.rate;
    dstframec=(int)(header.framec*scale);
    if (dstframec<1) dstframec=1;
  }
  struct synth_pcm *pcm=synth_pcm_new(dstframec);
  if (!pcm) return 0;
  if (synth_pcmfile_decode(pcm,&header,src,srcc)<0) {
    synth_pcm_del(pcm);
    return 0;
  }
  return pcm;
}
