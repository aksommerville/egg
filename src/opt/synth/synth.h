/* synth.h
 * Egg's native synthesizer.
 */
 
#ifndef SYNTH_H
#define SYNTH_H

#include <stdint.h>

struct synth;

/* Rates 200..200k are permitted.
 * Channel count may go up to 8, but we only emit a signal on the first 2.
 */
void synth_del(struct synth *synth);
struct synth *synth_new(int rate,int chanc);

/* In order to use synth_play_sound and synth_play_song, you must install resources ahead of time.
 * You must keep (src) unchanged for as long as the synth lives.
 * Installing resources is cheap, we only decode them when they're played.
 */
int synth_install_sound(struct synth *synth,int rid,const void *src,int srcc);
int synth_install_song(struct synth *synth,int rid,const void *src,int srcc);

/* Advance time and produce output.
 * It's safe (but weird) to change output formats on the fly.
 * (c) is always in samples -- not bytes, not frames.
 */
void synth_updatei(int16_t *v,int c,struct synth *synth);
void synth_updatef(float *v,int c,struct synth *synth);

/* Simple operations for exposure to client games.
 * Playing a song ends any prior song, and that doesn't necessarily happen immediately.
 * Synth has some latitude to apply fade-out or similar effects in the transition.
 * Any subsequent calls to synth_get_song(), synth_get_playhead(), synth_set_playhead() do refer to the new song immediately.
 */
void synth_play_sound(struct synth *synth,int rid);
void synth_play_song(struct synth *synth,int rid,int force,int repeat);
void synth_event(struct synth *synth,uint8_t chid,uint8_t opcode,uint8_t a,uint8_t b,int durms);

/* End current song and play an EGS song or PCM sound.
 * If 'handoff' reports success, we will eventually free (src).
 * For 'borrow', you must keep (src) unchanged until completion.
 * synth_get_song() will return 0x10000 while playing, and 0 once it completes.
 */
int synth_play_song_handoff(struct synth *synth,void *src,int srcc,int repeat);
int synth_play_song_borrow(struct synth *synth,const void *src,int srcc,int repeat);

/* Current song id and playhead.
 * Playhead is in seconds from start of song.
 * It wraps around if repeat enabled.
 * synth_get_song() is always zero when no song is playing -- if you ask for an invalid rid, we never report that rid as playing.
 * It reports 0x10000 (OOB for rids) when we're playing an explicitly-provided resource.
 * Playhead is always 0.0 when no song playing, and never exactly 0.0 when one is.
 * When reporting playhead to the game, caller should try to adjust by the driver's buffer. That's out of scope for us.
 */
int synth_get_song(const struct synth *synth);
double synth_get_playhead(const struct synth *synth);
void synth_set_playhead(struct synth *synth,double s);
int synth_count_busses(const struct synth *synth);

#endif
