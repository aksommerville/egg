#include "eggdev_internal.h"
#include <math.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>

static struct sr_encoder capture={0};
static int samplec=0;
static int16_t lo=0,hi=0;
static double sqsum=0.0;

/* Signal handler.
 */
 
static void eggdev_sound_rcvsig(int sigid) {
  switch (sigid) {
    case SIGINT: if (++(eggdev.terminate)>=3) {
        fprintf(stderr,"%s: Too many unprocessed signals\n",eggdev.exename);
        exit(1);
      } break;
  }
}

/* Current CPU time.
 */
 
static double now_cpu() {
  struct timespec tv={0};
  clock_gettime(CLOCK_PROCESS_CPUTIME_ID,&tv);
  return (double)tv.tv_sec+(double)tv.tv_nsec/1000000000.0;
}

/* Callback from audio driver. Also called manually with (driver) null, if we're running headless.
 */
 
static void eggdev_sound_cb_pcm_out(int16_t *v,int c,struct hostio_audio *maybe) {
  synth_updatei(v,c,eggdev.synth);
  
  if (eggdev.dstpath) {
    // Assume that the host byte order is little-endian, and that we can emit the natural channel count.
    // If we find either of those assumptions unacceptable, no big deal, just need to capture framewise instead.
    sr_encode_raw(&capture,v,c<<1);
  }
  
  samplec+=c;
  for (;c-->0;v++) {
    if (*v<lo) lo=*v; else if (*v>hi) hi=*v;
    double norm=(*v)/32767.0;
    sqsum+=norm*norm;
  }
}

/* sound, given final serial data.
 * (rom) might be empty.
 */
 
static int eggdev_sound_inner(struct eggdev_rom *rom,const void *src,int srcc,const char *path) {
  capture.c=0;
  samplec=0;
  lo=0;
  hi=0;
  sqsum=0.0;
  
  /* Stand audio driver if requested, and if so, write (rate,chanc) back into our globals.
   */
  if (eggdev.audio_drivers&&strcmp(eggdev.audio_drivers,"none")) {
    struct hostio_audio_delegate delegate={
      .cb_pcm_out=eggdev_sound_cb_pcm_out,
    };
    if (!(eggdev.hostio=hostio_new(0,&delegate,0))) return -1;
    struct hostio_audio_setup setup={
      .rate=eggdev.audio_rate,
      .chanc=eggdev.audio_chanc,
      .device=eggdev.audio_device,
      .buffer_size=eggdev.audio_buffer,
    };
    if (hostio_init_audio(eggdev.hostio,eggdev.audio_drivers,&setup)<0) {
      fprintf(stderr,"%s: Failed to initialize audio driver.\n",eggdev.exename);
      return -2;
    }
    eggdev.audio_rate=eggdev.hostio->audio->rate;
    eggdev.audio_chanc=eggdev.hostio->audio->chanc;
    fprintf(stderr,
      "%s: Using driver '%s', rate=%d, chanc=%d\n",
      eggdev.exename,eggdev.hostio->audio->type->name,eggdev.audio_rate,eggdev.audio_chanc
    );
    
  /* If we're not creating a driver, apply default rate and chanc.
   */
  } else {
    if (!eggdev.audio_rate) eggdev.audio_rate=44100;
    if (!eggdev.audio_chanc) eggdev.audio_chanc=2;
  }
  
  /* Stand synthesizer.
   */
  if (!(eggdev.synth=synth_new(eggdev.audio_rate,eggdev.audio_chanc))) {
    fprintf(stderr,"%s: Failed to initialize synthesizer. rate=%d chanc=%d\n",eggdev.exename,eggdev.audio_rate,eggdev.audio_chanc);
    return -2;
  }
  
  /* Begin playing the song.
   */
  int repeat=0;
  if (eggdev.repeat&&eggdev.hostio&&!eggdev.dstpath) repeat=1;
  else if (eggdev.repeat) fprintf(stderr,"%s:WARNING: Ignoring '--repeat' due to output path or no driver.\n",eggdev.exename);
  synth_play_song_borrow(eggdev.synth,src,srcc,repeat);
  
  /* If we have a driver, start it up and sleep until all the busses (just 1) get dropped.
   */
  double starttime=now_cpu();
  if (eggdev.hostio) {
    if (hostio_audio_play(eggdev.hostio,1)<0) return -1;
    while (!eggdev.terminate) {
      if (hostio_update(eggdev.hostio)<0) return -1;
      if (!synth_get_song(eggdev.synth)) break;
      usleep(20000);
    }
    hostio_audio_play(eggdev.hostio,0);
    
  /* No driver, pump the synthesizer manually.
   */
  } else {
    int16_t tmp[1024];
    while (!eggdev.terminate&&synth_get_song(eggdev.synth)) {
      eggdev_sound_cb_pcm_out(tmp,sizeof(tmp)>>1,0);
    }
  }
  double endtime=now_cpu();
  
  /* If we're capturing, write that out.
   */
  if (capture.c&&eggdev.dstpath) {
    if (file_write(eggdev.dstpath,capture.v,capture.c)<0) {
      fprintf(stderr,"%s: Failed to write %d-byte PCM dump.\n",eggdev.dstpath,capture.c);
    } else {
      fprintf(stderr,"%s: Wrote PCM dump, %d bytes of s16le in %d channels, rate %d hz\n",eggdev.dstpath,capture.c,eggdev.audio_chanc,eggdev.audio_rate);
    }
  }
  
  /* Report stats.
   */
  if (samplec>0) {
    lo=-lo;
    if (lo>hi) hi=lo;
    double peak=hi/32768.0;
    double rms=sqrt(sqsum/(double)samplec);
    int framec=samplec/eggdev.audio_chanc;
    double dur=(double)framec/(double)eggdev.audio_rate;
    double load=0.0;
    int invload=0;
    if (starttime<endtime) {
      load=(endtime-starttime)/dur;
      invload=lround(1.0/load);
    }
    fprintf(stdout,"%s: %d frames, %.03f s, peak=%.06f, rms=%.06f, cpu=%06f(%dx)\n",path,framec,dur,peak,rms,load,invload);
  } else {
    fprintf(stdout,"%s: No output.\n",path);
  }
  
  return 0;
}

/* sound, main entry point.
 */
 
int eggdev_main_sound() {
  int err;
  
  signal(SIGINT,eggdev_sound_rcvsig);
  
  /* Positional arguments must be one of: [ROM,RES], [FILE]
   */
  const char *rompath=0,*resname=0,*path=0;
  switch (eggdev.srcpathc) {
    case 1: path=eggdev.srcpathv[0]; break;
    case 2: rompath=eggdev.srcpathv[0]; resname=eggdev.srcpathv[1]; break;
    default: {
        fprintf(stderr,"%s: Unexpected count %d of positional arguments. Must be 1 or 2.\n",eggdev.exename,eggdev.srcpathc);
        return -2;
      }
  }
  
  /* Acquire serial from rom.
   */
  void *src=0;
  int srcc=0;
  struct eggdev_rom rom={0};
  if (rompath) {
    if ((err=eggdev_rom_add_path(&rom,rompath))<0) {
      if (err!=-2) fprintf(stderr,"%s: Unspecified error loading ROM.\n",rompath);
      eggdev_rom_cleanup(&rom);
      return -2;
    }
    struct eggdev_res *res=eggdev_rom_res_by_string(&rom,resname,-1);
    if (!res) {
      fprintf(stderr,"%s: Resource '%s' not found.\n",rompath,resname);
      eggdev_rom_cleanup(&rom);
      return -2;
    }
    src=res->serial;
    srcc=res->serialc;
    res->serial=0;
    res->serialc=0;
    
  /* Acquire serial from loose file.
   */
  } else if (path) {
    if ((srcc=file_read(&src,path))<0) {
      fprintf(stderr,"%s: Failed to read file.\n",path);
      return -2;
    }
  }
  
  /* Convert resource to an internal format.
   */
  struct eggdev_res res={.serial=src,.serialc=srcc};
  if ((err=eggdev_compile_song(&res,&rom))<0) {
    if (err!=-2) fprintf(stderr,"%s: Failed to compile sound from %d bytes input.\n",rompath?rompath:path,srcc);
    free(src);
    eggdev_rom_cleanup(&rom);
    return -2;
  }
  src=res.serial;
  srcc=res.serialc;
  
  /* Call out for further operations.
   */
  if (rompath) {
    char refname[1024];
    int refnamec=snprintf(refname,sizeof(refname),"%s:%s",rompath,resname);
    if ((refnamec<0)||(refnamec>=sizeof(refname))) refname[0]=0;
    err=eggdev_sound_inner(&rom,src,srcc,refname);
  } else {
    err=eggdev_sound_inner(&rom,src,srcc,path);
  }
  if (eggdev.hostio) {
    hostio_del(eggdev.hostio);
    eggdev.hostio=0;
  }
  if (eggdev.synth) {
    synth_del(eggdev.synth);
    eggdev.synth=0;
  }
  free(src);
  eggdev_rom_cleanup(&rom);
  return err;
}
