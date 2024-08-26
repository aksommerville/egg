/* pulse.h
 * Required:
 * Optional: hostio
 * Link: -lpulse-simple
 * Wrapper around PulseAudio for PCM-Out.
 */
 
#ifndef PULSE_H
#define PULSE_H

#include <stdint.h>

struct pulse_delegate {
  void *userdata;
  
  /* You must write (c) samples to (v).
   * (c) is in samples as usual -- not frames, not bytes.
   */
  void (*pcm_out)(int16_t *v,int c,void *userdata);
};

struct pulse_setup {
  int rate; // hz
  int chanc; // usually 1 or 2
  int buffersize; // Hardware buffer size in frames. Usually best to leave it zero, let us decide.
  const char *appname;
  const char *servername;
};

struct pulse;

void pulse_del(struct pulse *pulse);

struct pulse *pulse_new(
  const struct pulse_delegate *delegate,
  const struct pulse_setup *setup
);

void *pulse_get_userdata(const struct pulse *pulse);
int pulse_get_rate(const struct pulse *pulse);
int pulse_get_chanc(const struct pulse *pulse);
int pulse_get_running(const struct pulse *pulse);

void pulse_set_running(struct pulse *pulse,int running);

int pulse_update(struct pulse *pulse);
int pulse_lock(struct pulse *pulse);
void pulse_unlock(struct pulse *pulse);

double pulse_estimate_remaining_buffer(const struct pulse *pulse);

#endif
