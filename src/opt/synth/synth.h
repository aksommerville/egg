/* synth.h
 * The default synthesizer for Egg, and hopefully the only one.
 */
 
#ifndef SYNTH_H
#define SYNTH_H

#include <stdint.h>

struct synth;

void synth_del(struct synth *synth);

/* We accept (rate,chanc) exactly or fail.
 * We're very tolerant of both, pretty much anything goes.
 */
struct synth *synth_new(int rate,int chanc);

/* Any time after construction, you may replace encoded sounds and song resources.
 * Doing this is dirt cheap. We don't actually decode anything until it's needed.
 * We borrow the buffers weakly: Caller must keep them in scope and unchanged throughout synth's lifetime.
 * It's expected that these are inside the ROM file, which is never allowed to change.
 * You can replace resources even if they are currently playing; anything playing right now will not be affected.
 * sounds:1 should contain the GM drum kit in indices 35..81.
 */
int synth_install_sounds(struct synth *synth,int rid,const void *src,int srcc);
int synth_install_song(struct synth *synth,int rid,const void *src,int srcc);

/* Advance internal state and produce PCM.
 * Floating-point output is more natural for us, but ask for what makes sense to you.
 * It's weird but perfectly safe to change formats at any time.
 */
void synth_updatef(float *v,int c,struct synth *synth);
void synth_updatei(int16_t *v,int c,struct synth *synth);

/* Begin a fire-and-forget sound effect.
 */
void synth_play_sound(struct synth *synth,int rid,int index);

/* Stop the current song, and if (rid) exists, begin playing it.
 * (force) to restart even if the requested song is already playing.
 * (repeat) to loop at the end, otherwise we finish and play silence.
 */
void synth_play_song(struct synth *synth,int rid,int force,int repeat);

/* Rid of current song, or zero if silent.
 * It's zero if an invalid song was requested (not that invalid rid), and also after a non-repeating song finishes.
 */
int synth_get_song(const struct synth *synth);

/* Push an arbitrary event onto the bus.
 * Most MIDI commands are understood, but don't necessarily do anything.
 * Songs occupy the first 8 channels, and the next 8 are available exclusively for programmatic use.
 * Sound effects do not occupy channels.
 * Opcode 0x98 is equivalent to Note On followed by Note Off after (durms) milliseconds.
 */
void synth_event(struct synth *synth,uint8_t chid,uint8_t opcode,uint8_t a,uint8_t b,int durms);

/* Current position in song, in seconds.
 * Exactly zero if no song is playing, and never exactly zero if one is.
 * You should try to adjust for driver buffering before reporting this to the client -- that's out of scope for synth.
 * The time we report should be some distance in the future of what's actually playing right now.
 */
double synth_get_playhead(const struct synth *synth);
void synth_set_playhead(struct synth *synth,double s);

#endif
