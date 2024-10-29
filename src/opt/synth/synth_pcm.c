#include "synth_internal.h"

// Arbitrary limit in samples, to prevent excessive erroneous allocations.
#define SYNTH_PCM_SIZE_LIMIT 10000000

/* Wave lifecycle.
 */

void synth_wave_del(struct synth_wave *wave) {
  if (!wave) return;
  if (wave->refc-->1) return;
  free(wave);
}

int synth_wave_ref(struct synth_wave *wave) {
  if (!wave) return -1;
  if (wave->refc<1) return -1;
  if (wave->refc==INT_MAX) return -1;
  wave->refc++;
  return 0;
}

struct synth_wave *synth_wave_new() {
  struct synth_wave *wave=calloc(1,sizeof(struct synth_wave));
  if (!wave) return 0;
  wave->refc=1;
  return wave;
}

/* Require shared sine wave.
 */

int synth_require_sine(struct synth *synth) {
  if (synth->sine) return 0;
  if (!(synth->sine=synth_wave_new())) return -1;
  float *dst=synth->sine->v;
  int i=SYNTH_WAVE_SIZE_SAMPLES;
  float p=0.0f,dp=(M_PI*2.0f)/SYNTH_WAVE_SIZE_SAMPLES;
  for (;i-->0;dst++,p+=dp) *dst=sinf(p);
  return 0;
}

/* Add harmonic.
 */
 
static void synth_wave_add_harmonic(float *dst,const float *src,int step,float level) {
  if (step>=SYNTH_WAVE_SIZE_SAMPLES>>1) return;
  int srcp=0,i=SYNTH_WAVE_SIZE_SAMPLES;
  for (;i-->0;dst++) {
    (*dst)+=src[srcp]*level;
    if ((srcp+=step)>=SYNTH_WAVE_SIZE_SAMPLES) srcp-=SYNTH_WAVE_SIZE_SAMPLES;
  }
}

/* Decode wave.
 */

int synth_wave_decode(struct synth_wave *dst,struct synth *synth,const void *src,int srcc) {
  if (!dst||!synth) return -1;
  if (srcc<1) { // Empty is legal, produce a sine wave.
    if (synth_require_sine(synth)<0) return -1;
    memcpy(dst->v,synth->sine->v,sizeof(float)*SYNTH_WAVE_SIZE_SAMPLES);
    return 0;
  }
  if (!src) return -1;
  const uint8_t *SRC=src;
  switch (SRC[0]) {
    case 0: { // harmonics
        int srcp=1;
        if (srcp>=srcc) return -1;
        int coefc=SRC[srcp++];
        if (srcp>srcc-coefc*2) return -1;
        if (synth_require_sine(synth)<0) return -1;
        memset(dst->v,0,sizeof(float)*SYNTH_WAVE_SIZE_SAMPLES);
        int step=1;
        for (;step<=coefc;step++,srcp+=2) {
          if (!SRC[srcp]&&!SRC[srcp+1]) continue;
          float level=((SRC[srcp]<<8)|SRC[srcp=1])/65535.0f;
          synth_wave_add_harmonic(dst->v,synth->sine->v,step,level);
        }
        return srcp;
      }
    case 1: { // sine
        if (synth_require_sine(synth)<0) return -1;
        memcpy(dst->v,synth->sine->v,sizeof(float)*SYNTH_WAVE_SIZE_SAMPLES);
      } return 1;
    case 2: { // square
        int headlen=SYNTH_WAVE_SIZE_SAMPLES>>1;
        int taillen=SYNTH_WAVE_SIZE_SAMPLES-headlen;
        float *v=dst->v;
        for (;headlen-->0;v++) *v=1.0f;
        for (;taillen-->0;v++) *v=-1.0f;
      } return 1;
    case 3: { // saw
        float p=1.0f;
        float dp=-2.0f/SYNTH_WAVE_SIZE_SAMPLES;
        int i=SYNTH_WAVE_SIZE_SAMPLES;
        float *v=dst->v;
        for (;i-->0;v++,p+=dp) *v=p;
      } return 1;
    case 4: { // triangle
        float p=-1.0f;
        float dp=2.0f/(SYNTH_WAVE_SIZE_SAMPLES>>1);
        int i=SYNTH_WAVE_SIZE_SAMPLES>>1;
        float *v=dst->v;
        for (;i-->0;v++,p+=dp) *v=p;
        for (i=SYNTH_WAVE_SIZE_SAMPLES>>1;i-->0;v++,p-=dp) *v=p;
      } return 1;
  }
  return -1;
}

/* PCM object lifecycle.
 */

void synth_pcm_del(struct synth_pcm *pcm) {
  if (!pcm) return;
  if (pcm->refc-->1) return;
  free(pcm);
}

int synth_pcm_ref(struct synth_pcm *pcm) {
  if (!pcm) return -1;
  if (pcm->refc<1) return -1;
  if (pcm->refc==INT_MAX) return -1;
  pcm->refc++;
  return 0;
}

struct synth_pcm *synth_pcm_new(int c) {
  if (c<1) return 0;
  if (c>SYNTH_PCM_SIZE_LIMIT) return 0;
  struct synth_pcm *pcm=calloc(1,sizeof(struct synth_pcm)+sizeof(float)*c);
  if (!pcm) return 0;
  pcm->refc=1;
  pcm->c=1;
  return pcm;
}

/* Resample PCM.
 */
 
static struct synth_pcm *synth_pcm_resample(const struct synth_pcm *src,int dstrate,int srcrate) {
  int dstsamplec=lround(((double)dstrate*(double)src->c)/(double)srcrate);
  struct synth_pcm *dst=synth_pcm_new(dstsamplec);
  if (!dst) return 0;
  double srcp=0.0f;
  double srcpd=(double)srcrate/(double)dstrate;
  int i=dstsamplec;
  float *dstp=dst->v;
  for (;i-->0;dstp++,srcp+=srcpd) {
    double whole,fract;
    fract=modf(srcp,&whole);
    int srcpi=(int)whole;
    if (srcpi>=src->c-1) {
      *dstp=src->v[src->c-1];
    } else {
      float a=src->v[srcpi];
      float b=src->v[srcpi+1];
      float bweight=(float)fract;
      float aweight=1.0f-bweight;
      *dstp=a*aweight+b*bweight;
    }
  }
  return dst;
}

/* Decode PCM from WAV.
 */
 
static struct synth_pcm *synth_pcm_decode_wav(const uint8_t *src,int srcc,int dstrate) {
  if (srcc<44) return 0;
  int samplec=(srcc-44)>>1;
  int datalen=src[40]|(src[41]<<8)|(src[42]<<16)|(src[43]<<24);
  if (datalen!=samplec<<1) return 0;
  struct synth_pcm *pcm=synth_pcm_new(samplec);
  if (!pcm) return 0;
  float *dst=pcm->v;
  int srcp=44;
  int i=samplec;
  for (;i-->0;dst++,srcp+=2) {
    *dst=((int16_t)(src[srcp]|(src[srcp+1]<<8)))/32768.0f;
  }
  int srcrate=src[24]|(src[25]<<8)|(src[26]<<16)|(src[27]<<24);
  if (srcrate!=dstrate) {
    struct synth_pcm *npcm=synth_pcm_resample(pcm,dstrate,srcrate);
    if (npcm) {
      synth_pcm_del(pcm);
      pcm=npcm;
    } // If resample fails, whatever, return the wrong-rate pcm.
  }
  return pcm;
}

/* "Decode PCM" by employing a printer.
 */
 
static struct synth_pcm *synth_pcm_decode_print(const void *src,int srcc,int dstrate) {
  struct synth_printer *printer=synth_printer_new(src,srcc,dstrate);
  if (!printer) return 0;
  synth_printer_update(printer,printer->pcm->c);
  struct synth_pcm *pcm=printer->pcm;
  printer->pcm=0;
  synth_printer_del(printer);
  
  // There can be a substantial amount of silence at the end. Trim it.
  double pe=0.000001;
  double ne=-pe;
  while (pcm->c&&(pcm->v[pcm->c-1]>ne)&&(pcm->v[pcm->c-1]<pe)) pcm->c--;
  
  return pcm;
}

/* Decode PCM, dispatch on format.
 */

struct synth_pcm *synth_pcm_decode(const void *src,int srcc,int dstrate) {
  if (!src) return 0;
  if ((srcc>=4)&&!memcmp(src,"RIFF",4)) return synth_pcm_decode_wav(src,srcc,dstrate);
  if ((srcc>=4)&&!memcmp(src,"\0EGS",4)) return synth_pcm_decode_print(src,srcc,dstrate);
  return 0;
}

/* Delete printer.
 */

void synth_printer_del(struct synth_printer *printer) {
  if (!printer) return;
  synth_pcm_del(printer->pcm);
  synth_del(printer->synth);
  free(printer);
}

/* Initialize new printer.
 */
 
static int synth_printer_init(struct synth_printer *printer,const uint8_t *src,int srcc,int rate) {
  if (!(printer->synth=synth_new(rate,1))) return -1;
  synth_play_song_borrow(printer->synth,src,srcc,0);
  if (!printer->synth->songid) return -1;
  
  /* Add up delay events from the embedded synthesizer, now that it's loaded.
   */
  int durms=0;
  int srcp=0;
  src=printer->synth->song;
  srcc=printer->synth->songc;
  while (srcp<srcc) {
    uint8_t lead=src[srcp++];
    if (!lead) break;
    
    if (!(lead&0x80)) {
      if (lead&0x40) durms+=((lead&0x3f)+1)<<6;
      else durms+=lead;
      continue;
    }
    
    switch (lead&0xf0) {
      case 0x80: srcp+=2; break;
      case 0x90: srcp+=2; break;
      case 0xa0: srcp+=2; break;
      default: return -1;
    }
  }
  
  /* There's no telling how much tail time they'll need.
   * Add half a second to be safe.
   */
  durms+=500;
  
  int framec=(int)((double)durms*(double)rate)/1000.0;
  if (framec<1) framec=1;
  if (!(printer->pcm=synth_pcm_new(framec))) return -1;
  
  return 0;
}

/* New printer.
 */
 
struct synth_printer *synth_printer_new(const void *src,int srcc,int rate) {
  struct synth_printer *printer=calloc(1,sizeof(struct synth_printer));
  if (!printer) return 0;
  if (synth_printer_init(printer,src,srcc,rate)<0) {
    synth_printer_del(printer);
    return 0;
  }
  return printer;
}

/* Update printer.
 */
 
int synth_printer_update(struct synth_printer *printer,int c) {
  if (!printer) return -1;
  if (c>0) {
    int updc=printer->pcm->c-printer->p;
    if (updc>c) updc=c;
    if (updc>0) {
      synth_updatef(printer->pcm->v+printer->p,updc,printer->synth);
      printer->p+=updc;
    }
  }
  return (printer->p>=printer->pcm->c)?0:1;
}
