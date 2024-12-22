#include "macaudio_internal.h"

/* Cleanup.
 */
 
void macaudio_del(struct macaudio *macaudio) {
  if (!macaudio) return;
  if (macaudio->instance) {
    AudioOutputUnitStop(macaudio->instance);
    AudioComponentInstanceDispose(macaudio->instance);
  }
  pthread_mutex_destroy(&macaudio->cbmtx);
  free(macaudio);
}

/* PCM callback.
 */

static OSStatus macaudio_cb_pcm(
  void *userdata,
  AudioUnitRenderActionFlags *flags,
  const AudioTimeStamp *time,
  UInt32 bus,
  UInt32 framec,
  AudioBufferList *data
) {
  struct macaudio *macaudio=userdata;
  if (macaudio->play) {
    if (pthread_mutex_lock(&macaudio->cbmtx)) return 0;
    macaudio->delegate.pcm_out(data->mBuffers[0].mData,data->mBuffers[0].mDataByteSize>>1,macaudio->delegate.userdata);
    pthread_mutex_unlock(&macaudio->cbmtx);
  } else {
    memset(data->mBuffers[0].mData,0,data->mBuffers[0].mDataByteSize);
  }
  return 0;
}

/* Initialize AudioUnit stuff in a fresh context.
 */

static int macaudio_init_AudioUnit(struct macaudio *macaudio) {

  AudioComponentDescription desc={0};
  desc.componentType=kAudioUnitType_Output;
  desc.componentSubType=kAudioUnitSubType_DefaultOutput;

  if (!(macaudio->component=AudioComponentFindNext(0,&desc))) return -1;
  if (AudioComponentInstanceNew(macaudio->component,&macaudio->instance)) return -1;
  if (AudioUnitInitialize(macaudio->instance)) return -1;

  AudioStreamBasicDescription fmt={0};
  fmt.mSampleRate=macaudio->rate;
  fmt.mFormatID=kAudioFormatLinearPCM;
  fmt.mFormatFlags=kAudioFormatFlagIsSignedInteger;
  fmt.mFramesPerPacket=1;
  fmt.mChannelsPerFrame=macaudio->chanc;
  fmt.mBitsPerChannel=16;
  fmt.mBytesPerPacket=macaudio->chanc*2;
  fmt.mBytesPerFrame=macaudio->chanc*2;
  if (AudioUnitSetProperty(macaudio->instance,kAudioUnitProperty_StreamFormat,kAudioUnitScope_Input,0,&fmt,sizeof(fmt))) {
    return -1;
  }

  AURenderCallbackStruct aucb={0};
  aucb.inputProc=macaudio_cb_pcm;
  aucb.inputProcRefCon=macaudio;
  if (AudioUnitSetProperty(macaudio->instance,kAudioUnitProperty_SetRenderCallback,kAudioUnitScope_Input,0,&aucb,sizeof(aucb))) {
    return -1;
  }

  if (AudioOutputUnitStart(macaudio->instance)) return -1;

  return 0;
}

/* New.
 */

struct macaudio *macaudio_new(
  const struct macaudio_delegate *delegate,
  const struct macaudio_setup *setup
) {
  if (!delegate||!setup) return 0;
  struct macaudio *macaudio=calloc(1,sizeof(struct macaudio));
  if (!macaudio) return 0;

  macaudio->delegate=*delegate;
  macaudio->rate=setup->rate;
  macaudio->chanc=setup->chanc;

  if (!macaudio->rate) macaudio->rate=44100;
  if (!macaudio->chanc) macaudio->chanc=1;
  if ((macaudio->rate<200)||(macaudio->rate>200000)||(macaudio->chanc<1)||(macaudio->chanc>8)) {
    macaudio_del(macaudio);
    return 0;
  }

  pthread_mutex_init(&macaudio->cbmtx,0);

  if (macaudio->delegate.pcm_out) {
    if (macaudio_init_AudioUnit(macaudio)<0) {
      macaudio_del(macaudio);
      return 0;
    }
  }

  if (macaudio->delegate.midi_in) {
    //TODO MIDI In
  }

  return macaudio;
}

/* Trivial accessors.
 */

int macaudio_get_rate(const struct macaudio *macaudio) {
  if (!macaudio) return 0;
  return macaudio->rate;
}

int macaudio_get_chanc(const struct macaudio *macaudio) {
  if (!macaudio) return 0;
  return macaudio->chanc;
}

void *macaudio_get_userdata(const struct macaudio *macaudio) {
  if (!macaudio) return 0;
  return macaudio->delegate.userdata;
}

int macaudio_lock(struct macaudio *macaudio) {
  if (!macaudio) return -1;
  if (pthread_mutex_lock(&macaudio->cbmtx)) return -1;
  return 0;
}

int macaudio_unlock(struct macaudio *macaudio) {
  if (!macaudio) return -1;
  if (pthread_mutex_unlock(&macaudio->cbmtx)) return -1;
  return 0;
}

/* Pause/resume.
 */
 
void macaudio_play(struct macaudio *macaudio,int play) {
  if (!macaudio) return;
  if (play) {
    if (macaudio->play) return;
    macaudio->play=1;
  } else {
    if (!macaudio->play) return;
    macaudio->play=0;
  }
}
