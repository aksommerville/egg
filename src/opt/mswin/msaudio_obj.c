#include "msaudio_internal.h"

/* We depend on callbacks that I don't see how to supply a context pointer.
 * Whatever. There can't be more than one msaudio instance at a time.
 */
static struct hostio_audio *msaudio_global_driver=0;

/* Callback.
 */

static void CALLBACK msaudio_cb(
  HWAVEOUT hwo,UINT uMsg,DWORD_PTR dwInstance,DWORD dwParam1,DWORD dwParam2
) {
  struct hostio_audio *driver=msaudio_global_driver;
  switch (uMsg) {
    case MM_WOM_DONE: {
        SetEvent(DRIVER->buffer_ready);
      } break;
  }
}

/* Thread main.
 */

// Returns <0 to abort.
static int msaudio_check_buffer(WAVEHDR *hdr) {
  struct hostio_audio *driver=msaudio_global_driver;
  if (!driver) return 0;
  if (hdr->dwUser) return 0;

  if (WaitForSingleObject(DRIVER->mutex,0)!=WAIT_OBJECT_0) {
    return 0;
  }
  if (WaitForSingleObject(DRIVER->thread_terminate,0)==WAIT_OBJECT_0) {
    ReleaseMutex(DRIVER->mutex);
    return -1;
  }
  
  hdr->dwUser=1;
  if (driver->playing) {
    driver->delegate.cb_pcm_out((void*)hdr->lpData,hdr->dwBufferLength>>1,driver->delegate.userdata);
  } else {
    memset(hdr->lpData,0,hdr->dwBufferLength);
  }

  ReleaseMutex(DRIVER->mutex);
  
  hdr->dwBytesRecorded=hdr->dwBufferLength;
  hdr->dwFlags=WHDR_PREPARED;
  waveOutWrite(DRIVER->waveout,hdr,sizeof(WAVEHDR));

  return 0;
}

static DWORD WINAPI msaudio_thread(LPVOID arg) {
  struct hostio_audio *driver=msaudio_global_driver;
  while (1) {

    /* Check for termination. */
    if (WaitForSingleObject(DRIVER->thread_terminate,0)==WAIT_OBJECT_0) break;

    /* Populate any buffers with zero user data. */
    if (msaudio_check_buffer(DRIVER->bufv+DRIVER->bufp)<0) break;
    if (msaudio_check_buffer(DRIVER->bufv+(DRIVER->bufp^1))<0) break;

    /* Sleep for a little. Swap buffers if signalled. */
    if (WaitForSingleObject(DRIVER->buffer_ready,1)==WAIT_OBJECT_0) {
      DRIVER->bufv[DRIVER->bufp].dwUser=0;
      DRIVER->bufp^=1;
    }
    
  }
  SetEvent(DRIVER->thread_complete);
  return 0;
}

/* Delete.
 */
  
static void _msaudio_del(struct hostio_audio *driver) {
  if (DRIVER->thread) {
    WaitForSingleObject(DRIVER->mutex,INFINITE);
    SetEvent(DRIVER->thread_terminate);
    WaitForSingleObject(DRIVER->thread_complete,INFINITE);
  }
  if (DRIVER->thread_terminate) CloseHandle(DRIVER->thread_terminate);
  if (DRIVER->thread_complete) CloseHandle(DRIVER->thread_complete);
  if (DRIVER->buffer_ready) CloseHandle(DRIVER->buffer_ready);
  if (DRIVER->waveout) waveOutClose(DRIVER->waveout);
  if (DRIVER->mutex) CloseHandle(DRIVER->mutex);
  if (DRIVER->bufv[0].lpData) free(DRIVER->bufv[0].lpData);
  if (DRIVER->bufv[1].lpData) free(DRIVER->bufv[1].lpData);
  if (driver==msaudio_global_driver) msaudio_global_driver=0;
}

/* Init.
 */
 
static int _msaudio_init(struct hostio_audio *driver,const struct hostio_audio_setup *config) {
  if (msaudio_global_driver) return -1;
  
  if (config) {
    if (config->rate<200) driver->rate=200;
    else if (config->rate>200000) driver->rate=200000;
    else driver->rate=config->rate;
    if (config->chanc<1) driver->chanc=1;
    else if (config->chanc>2) driver->chanc=2;
    else driver->chanc=config->chanc;
  } else {
    driver->rate=44100;
    driver->chanc=1;
  }

  DRIVER->mutex=CreateMutex(0,0,0);
  if (!DRIVER->mutex) {
    return -1;
  }

  WAVEFORMATEX format={
    .wFormatTag=WAVE_FORMAT_PCM,
    .nChannels=driver->chanc,
    .nSamplesPerSec=driver->rate,
    .nAvgBytesPerSec=driver->rate*driver->chanc*2,
    .nBlockAlign=driver->chanc*2, /* frame size in bytes */
    .wBitsPerSample=16,
    .cbSize=0,
  };
  
  msaudio_global_driver=driver;

  MMRESULT result=waveOutOpen(
    &DRIVER->waveout,
    WAVE_MAPPER,
    &format,
    (DWORD_PTR)msaudio_cb,
    0,
    CALLBACK_FUNCTION
  );
  if (result!=MMSYSERR_NOERROR) return -1;

  int buffer_size=8192; // bytes. I get underruns at 2048, and noticeable quantization at 8192. 4096 seems ok.
  WAVEHDR *a=DRIVER->bufv+0;
  WAVEHDR *b=DRIVER->bufv+1;
  a->dwBufferLength=b->dwBufferLength=buffer_size;
  a->dwBytesRecorded=b->dwBytesRecorded=0;
  a->dwUser=b->dwUser=0;
  a->dwFlags=b->dwFlags=0;
  a->dwLoops=b->dwLoops=0;
  a->lpNext=b->lpNext=0;
  a->reserved=b->reserved=0;
  if (!(a->lpData=calloc(1,buffer_size))) return -1;
  if (!(b->lpData=calloc(1,buffer_size))) return -1;

  if (!(DRIVER->thread_terminate=CreateEvent(0,0,0,0))) return -1;
  if (!(DRIVER->thread_complete=CreateEvent(0,0,0,0))) return -1;
  if (!(DRIVER->buffer_ready=CreateEvent(0,0,0,0))) return -1;

  DRIVER->thread=CreateThread(
    0, /* LPSECURITY_ATTRIBUTES */
    0, /* dwStackSize */
    (LPTHREAD_START_ROUTINE)msaudio_thread,
    0, /* lpParameter */
    0, /* dwCreationFlags */
    0 /* lpThreadId */
  );
  if (!DRIVER->thread) return -1;
  
  return 0;
}

/* Play/pause.
 */
 
static void _msaudio_play(struct hostio_audio *driver,int play) {
  driver->playing=play;
}

/* Update.
 */
 
static int _msaudio_update(struct hostio_audio *driver) {
  //TODO report errors?
  return 0;
}

/* Lock.
 */
 
static int _msaudio_lock(struct hostio_audio *driver) {
  if (WaitForSingleObject(DRIVER->mutex,INFINITE)) {
    return -1;
  }
  return 0;
}

static void _msaudio_unlock(struct hostio_audio *driver) {
  ReleaseMutex(DRIVER->mutex);
}

/* Type definition.
 */
 
const struct hostio_audio_type hostio_audio_type_msaudio={
  .name="msaudio",
  .desc="Audio for Windows",
  .objlen=sizeof(struct hostio_audio_msaudio),
  .del=_msaudio_del,
  .init=_msaudio_init,
  .play=_msaudio_play,
  .update=_msaudio_update,
  .lock=_msaudio_lock,
  .unlock=_msaudio_unlock,
};
