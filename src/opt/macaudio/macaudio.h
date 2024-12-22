/* macaudio.h
 * PCM output and MIDI input for MacOS.
 * Link: -framework Audiounit
 */

#ifndef MACAUDIO_H
#define MACAUDIO_H

#include <stdint.h>

struct macaudio;

/* We decide whether to initialize PCM and MIDI based on whether the delegate hook is set.
 */
struct macaudio_delegate {
  void *userdata;
  void (*pcm_out)(int16_t *v,int c,void *userdata);
  void (*midi_in)(const void *src,int srcc,void *userdata);
};

struct macaudio_setup {
  int rate; // hz
  int chanc; // 1,2
};

void macaudio_del(struct macaudio *macaudio);

struct macaudio *macaudio_new(
  const struct macaudio_delegate *delegate,
  const struct macaudio_setup *setup
);

int macaudio_get_rate(const struct macaudio *macaudio);
int macaudio_get_chanc(const struct macaudio *macaudio);
void *macaudio_get_userdata(const struct macaudio *macaudio);

/* A new context is initially stopped.
 * You must call this with (play) nonzero to begin emitting PCM.
 */
void macaudio_play(struct macaudio *macaudio,int play);

int macaudio_lock(struct macaudio *macaudio);
int macaudio_unlock(struct macaudio *macaudio);

#endif
