#include "eggdev_internal.h"

struct eggdev eggdev={0};

//XXX
static int rdsample16(const uint8_t *src) { return (int16_t)(src[0]|(src[1]<<8)); }
static int rdsample8(const uint8_t *src) { int sample=*src; sample|=sample<<8; return sample; }
static void wavmix(int16_t *dst,const uint8_t *src,int framec,int samplesize,int chanc,int stride) {
  if ((samplesize==16)&&(chanc==1)&&(stride==2)) {
    memcpy(dst,src,framec<<1);
    return;
  }
  int (*rdsample)(const uint8_t *src);
  if (samplesize>=16) {
    src+=(samplesize-16)>>3;
    rdsample=rdsample16;
  } else if (samplesize==8) {
    rdsample=rdsample8;
  } else {
    memset(dst,0,framec<<1);
    return;
  }
  int chstride=samplesize>>3;
  for (;framec-->0;dst++) {
    const uint8_t *next=src+stride;
    int sample=0;
    int i=chanc; while (i-->0) {
      sample+=rdsample(src);
      src+=chstride;
    }
    src=next;
    #if 1 /* Trim level a little */
      sample=(sample*2)/5;
    #endif
    if (sample<-32768) *dst=-32768;
    else if (sample>32767) *dst=32767;
    else *dst=sample;
  }
}
static int eggdev_main_wavrepair() {
  fprintf(stderr,"%s...\n",__func__);
  const char *path="src/demo/data/song/15-nothing_lasts_forever.wav";
  const char *dstpath="src/demo/data/song/16-revised.wav";
  uint8_t *src=0;
  int srcc=file_read(&src,path);
  if (srcc<0) {
    fprintf(stderr,"%s: Failed to read file\n",path);
    return -2;
  }
  if ((srcc<12)||memcmp(src,"RIFF",4)||memcmp(src+8,"WAVE",4)) {
    fprintf(stderr,"%s: Not a WAV file\n",path);
    return -2;
  }
  
  /* First pass, read the header and mix to 16-bit mono.
   */
  int16_t *samplev=0;
  int samplec=0,samplea=0;
  int srcp=12,chanc=0,rate=0,stride,samplesize;
  while (srcp<srcc) {
    if (srcp>srcc-8) return -1;
    const uint8_t *chunkid=src+srcp;
    srcp+=4;
    int chunklen=src[srcp]|(src[srcp+1]<<8)|(src[srcp+2]<<16)|(src[srcp+3]<<24);
    srcp+=4;
    if ((chunklen<0)||(srcp>srcc-chunklen)) return -1;
    const uint8_t *v=src+srcp;
    srcp+=chunklen;
    if (srcp&1) srcp++;
    
    if (!memcmp(chunkid,"fmt ",4)) {
      if (chunklen<16) {
        fprintf(stderr,"%s: Invalid fmt length %d\n",path,chunklen);
        return -2;
      }
      int format=v[0]|(v[1]<<8);
      chanc=v[2]|(v[3]<<8);
      rate=v[4]|(v[5]<<8)|(v[6]<<16)|(v[7]<<24);
      stride=v[12]|(v[13]<<8);
      samplesize=v[14]|(v[15]<<8);
      if ((format!=1)||(chanc<1)||(chanc>8)||(samplesize&7)||!samplesize) {
        fprintf(stderr,"%s: Invalid format, format=%d chanc=%d samplesize=%d\n",path,format,chanc,samplesize);
        return -2;
      }
      fprintf(stderr,"%s: chanc=%d rate=%d samplesize=%d\n",path,chanc,rate,samplesize);
      
    } else if (!memcmp(chunkid,"data",4)) {
      if (!chanc) {
        fprintf(stderr,"%s: 'data' before 'fmt '\n",path);
        return -2;
      }
      fprintf(stderr,"%s: data, %d bytes\n",path,chunklen);
      int framec=chunklen/stride;
      samplea+=framec;
      if (!(samplev=realloc(samplev,sizeof(int16_t)*samplea))) return -1;
      wavmix(samplev+samplec,v,framec,samplesize,chanc,stride);
      samplec+=framec;
    }
  }
  free(src);
  
  fprintf(stderr,"...ok got %d frames of 16-bit mono at %d hz\n",samplec,rate);
  
  /* I suspect there is a tail at the end.
   * Confirm, then chop it off and mix into the front.
   * Song is 32 beats long, I think 106 bpm, 865798 frames at 44100 hz.
   * 32 @ 106 yield 798792 frames, like 1.5 seconds short. I don't think there was *that* much tail.
   * But chop it there and see.
   * ...confirmed, that's a bit early.
   * 97 bpm => 872907 frames, still some delay
   * 98 bpm => 864000 frames, good, could be perfect.
   * ...and will the tail-pasting, it really does sound seamless (except for a new leading bump, and nothing for it)
   */
  const int chop=864000;
  if (samplec>chop) {
    fprintf(stderr,"...truncating to %d\n",chop);
    int extrac=samplec-chop;
    samplec=chop;
    if (extrac>samplec) extrac=samplec;
    int16_t *dst=samplev;
    const int16_t *src=samplev+samplec;
    int i=extrac;
    for (;i-->0;dst++,src++) (*dst)+=(*src);
  }
  
  /* Compose a new WAV file.
   */
  struct sr_encoder encoder={0};
  if (sr_encode_raw(&encoder,
    "RIFF\0\0\0\0WAVE" // [4..7] file length
    "fmt \x10\0\0\0"
    "\1\0\1\0" // format, chanc (constant)
  ,24)<0) return -1;
  if (sr_encode_intle(&encoder,rate,4)<0) return -1;
  if (sr_encode_intle(&encoder,rate*2,4)<0) return -1;
  if (sr_encode_raw(&encoder,
    "\2\0\x10\0" // framesize, samplesize (constant)
    "data"
  ,8)<0) return -1;
  if (sr_encode_intle(&encoder,samplec<<1,4)<0) return -1;
  if (sr_encode_raw(&encoder,samplev,samplec<<1)<0) return -1;
  if (file_write(dstpath,encoder.v,encoder.c)<0) {
    fprintf(stderr,"%s: Failed to write file, %d bytes\n",dstpath,encoder.c);
    return -2;
  }
  sr_encoder_cleanup(&encoder);
  free(samplev);
  return 0;
}

/* Main.
 */
 
int main(int argc,char **argv) {
  int err;
  if ((err=eggdev_configure(argc,argv))<0) {
    if (err!=-2) fprintf(stderr,"%s: Unspecified error configuring.\n",eggdev.exename);
    return 1;
  }
  if (!eggdev.command||!strcmp(eggdev.command,"help")) eggdev_print_help(eggdev.helptopic);
  #define _(tag) else if (!strcmp(eggdev.command,#tag)) err=eggdev_main_##tag();
  _(pack)
  _(unpack)
  _(bundle)
  _(list)
  _(validate)
  _(serve)
  _(config)
  _(dump)
  _(project)
  _(metadata)
  _(sound)
  _(macicon)
  _(wavrepair)//XXX
  #undef _
  else {
    fprintf(stderr,"%s: Unknown command '%s'\n",eggdev.exename,eggdev.command);
    eggdev_print_help(0);
    err=-2;
  }
  if (err<0) {
    if (err!=-2) fprintf(stderr,"%s: Unspecified error running command '%s'\n",eggdev.exename,eggdev.command);
    return 1;
  }
  return 0;
}
