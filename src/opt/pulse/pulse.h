/* pulse.h
 * Interface to PulseAudio.
 */
 
#ifndef PULSE_H
#define PULSE_H

#include <stdint.h>

struct pulse;

void pulse_del(struct pulse *pulse);

struct pulse *pulse_new(
  int rate,int chanc,
  void (*cb)(int16_t *v,int c,void *userdata),
  void *userdata,
  const char *appname
);

void *pulse_get_userdata(const struct pulse *pulse);
int pulse_get_rate(const struct pulse *pulse);
int pulse_get_chanc(const struct pulse *pulse);
void pulse_play(struct pulse *pulse,int play);
int pulse_lock(struct pulse *pulse);
void pulse_unlock(struct pulse *pulse);

#endif
