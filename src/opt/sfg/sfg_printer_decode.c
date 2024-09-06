#include "sfg_internal.h"

/* Extract one sound from resource.
 */
 
int sfg_get_sound_serial(void *dstpp,const void *src,int srcc,int index) {
  if (!src) return -1;
  if ((srcc<4)||memcmp(src,"\0SFG",4)) return -1;
  if (index<0) return -1;
  const uint8_t *SRC=src;
  int srcp=4,current=0;
  while (srcp<=srcc-3) {
    uint8_t d=SRC[srcp++];
    int len=(SRC[srcp]<<8)|SRC[srcp+1];
    srcp+=2;
    current+=d+1;
    if (current==index) {
      if (dstpp) *(const void**)dstpp=SRC+srcp;
      return len;
    }
    if (current>index) break;
    srcp+=len;
  }
  return -1;
}
  
/* List sounds in encoded resource.
 */
 
int sfg_serial_for_each(
  const void *src,int srcc,
  int (*cb)(int index,const void *src,int srcc,void *userdata),
  void *userdata
) {
  if (!src||(srcc<4)||memcmp(src,"\0SFG",4)) return 0;
  const uint8_t *SRC=src;
  int srcp=4,index=0;
  while (srcp<=srcc-3) {
    uint8_t d=SRC[srcp++];
    int len=(SRC[srcp]<<8)|SRC[srcp+1];
    srcp+=2;
    if (srcp>srcc-len) break;
    index+=d+1;
    int err=cb(index,SRC+srcp,len,userdata);
    if (err) return err;
    srcp+=len;
  }
  return 0;
}

/* Generate wave from harmonics.
 * Decodes from (src), 2 bytes per coefficient.
 */
 
static int sfg_voice_generate_harmonics(struct sfg_voice *voice,const uint8_t *src,int coefc) {
  //TODO
  return 0;
}

/* Measure envelope argument.
 */
 
static int sfg_env_measure(const uint8_t *src,int srcc) {
  if (srcc<3) return -1;
  int legc=src[2];
  int end=3+legc*4;
  if (end>srcc) return -1;
  return end;
}

/* Assert that a voice is ready.
 */
 
static int sfg_printer_finish_voice(struct sfg_printer *printer,struct sfg_voice *voice) {
  //TODO Must contain level envelope.
  //TODO Must contain either freq envelope or shape=noise.
  return 0;
}

/* Decode into fresh printer, main entry point.
 * Return duration in samples, and don't allocate pcm.
 */
 
int sfg_printer_decode(struct sfg_printer *printer,const uint8_t *src,int srcc) {
  fprintf(stderr,"%s srcc=%d\n",__func__,srcc);
  int samplec=0; // Final answer, ie longest of voices
  struct sfg_voice *voice=0;
  
  #define CREATE_VOICE { \
    if (!voice) { \
      if (!(voice=sfg_printer_spawn_voice(printer))) return -1; \
    } \
  }
  #define REQUIRE_VOICE { \
    if (!voice) return -1; \
  }
  
  int srcp=0; while (srcp<srcc) {
    uint8_t opcode=src[srcp++];
    switch (opcode) {

      case 0x00: {
          if (voice) {
            if (sfg_printer_finish_voice(printer,voice)<0) return -1;
            voice=0;
          }
        } break;
        
      /* The oscillator commands 1..6 (shape..freqlfo) will create the voice, if it's not created yet.
       */
        
      case 0x01: { // shape
          CREATE_VOICE
          if (srcp>srcc-1) return -1;
          voice->shape=src[srcp++];
        } break;
        
      case 0x02: { // harm
          CREATE_VOICE
          if (srcp>srcc-1) return -1;
          uint8_t coefc=src[srcp++];
          if (srcp>srcc-coefc*2) return -1;
          if (sfg_voice_generate_harmonics(voice,src+srcp,coefc)<0) return -1;
          srcp+=coefc*2;
        } break;
        
      case 0x03: { // fm
          CREATE_VOICE
          if (srcp>srcc-2) return -1;
          voice->fmrate=((src[srcp]<<8)|src[srcp+1])/256.0f;
          srcp+=2;
          int err=sfg_env_measure(src+srcp,srcc-srcp);//TODO
          if (err<1) return -1;
          srcp+=err;
        } break;
        
      case 0x04: { // fmlfo
          CREATE_VOICE
          if (srcp>srcc-3) return -1;
          voice->fmlforate=((src[srcp]<<8)|src[srcp+1])/256.0f;
          srcp+=2;
          voice->fmlfodepth=src[srcp]/16.0f;
          srcp++;
        } break;
        
      case 0x05: { // freq
          CREATE_VOICE
          int err=sfg_env_measure(src+srcp,srcc-srcp);//TODO
          if (err<1) return -1;
          srcp+=err;
        } break;
        
      case 0x06: { // freqlfo
          CREATE_VOICE
          if (srcp>srcc-4) return -1;
          voice->freqlforate=((src[srcp]<<8)|src[srcp+1])/256.0f;
          srcp+=2;
          voice->freqlfodepth=(src[srcp]<<8)|src[srcp+1];
          srcp+=2;
        } break;
        
      /* Commands 7 and up require the oscillator to be finished already.
       * Do not create the voice.
       */
        
      case 0x07: { // level
          REQUIRE_VOICE
          int err=sfg_env_measure(src+srcp,srcc-srcp);
          if (err<1) return -1;
          srcp+=err;
          fprintf(stderr,"  LEVEL\n");
        } break;
        
      case 0x08: { // waveshaper
          REQUIRE_VOICE
          if (srcp>srcc-1) return -1;
          int c=src[srcp++]+2;
          if (srcp>srcc-c) return -1;
          srcp+=c;
          fprintf(stderr,"  WAVESHAPER %d\n",c);
        } break;
        
      case 0x09: { // gain
          REQUIRE_VOICE
          if (srcp>srcc-4) return -1;
          float gain=((src[srcp]<<8)|src[srcp+1])/256.0f;
          srcp+=2;
          float clip=src[srcp++]/256.0f;
          float gate=src[srcp++]/256.0f;
          fprintf(stderr,"  GAIN *%f x%f _%f\n",gain,clip,gate);
        } break;
        
      case 0x0a: { // delay
          REQUIRE_VOICE
          if (srcp>srcc-5) return -1;
          int ms=src[srcp++]<<2;
          float dry=src[srcp++]/256.0f;
          float wet=src[srcp++]/256.0f;
          float sto=src[srcp++]/256.0f;
          float fbk=src[srcp++]/256.0f;
          fprintf(stderr,"  DELAY %dms (%f,%f,%f,%f)\n",ms,dry,wet,sto,fbk);
        } break;
        
      case 0x0b: { // detune
          REQUIRE_VOICE
          if (srcp>srcc-2) return -1;
          int ms=src[srcp++]<<2;
          float depth=src[srcp++]/256.0f;
          fprintf(stderr,"  DETUNE %dms %f\n",ms,depth);
        } break;
        
      case 0x0c: { // lopass
          REQUIRE_VOICE
          if (srcp>srcc-2) return -1;
          int hz=(src[srcp]<<8)|src[srcp+1];
          srcp+=2;
          fprintf(stderr,"  LOPASS %dhz\n",hz);
        } break;
        
      case 0x0d: { // hipass
          REQUIRE_VOICE
          if (srcp>srcc-2) return -1;
          int hz=(src[srcp]<<8)|src[srcp+1];
          srcp+=2;
          fprintf(stderr,"  HIPASS %dhz\n",hz);
        } break;
        
      case 0x0e: { // bpass
          REQUIRE_VOICE
          if (srcp>srcc-4) return -1;
          int hz=(src[srcp]<<8)|src[srcp+1];
          srcp+=2;
          int whz=(src[srcp]<<8)|src[srcp+1];
          srcp+=2;
          fprintf(stderr,"  BPASS %dhz w%d\n",hz,whz);
        } break;
        
      case 0x0f: { // notch
          REQUIRE_VOICE
          if (srcp>srcc-4) return -1;
          int hz=(src[srcp]<<8)|src[srcp+1];
          srcp+=2;
          int whz=(src[srcp]<<8)|src[srcp+1];
          srcp+=2;
          fprintf(stderr,"  NOTCH %dhz w%d\n",hz,whz);
        } break;
          
      default: fprintf(stderr,"  UNKNOWN OPCODE 0x%02x!\n",opcode); return -1;
    }
  }
  #undef CREATE_VOICE
  #undef REQUIRE_VOICE
  if (voice) {
    if (sfg_printer_finish_voice(printer,voice)<0) return -1;
  }
  return samplec;
}
