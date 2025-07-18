/* synth.h
 * Egg native synthesizer.
 */
 
#ifndef SYNTH_H
#define SYNTH_H

#include <stdint.h>

struct synth;

void synth_del(struct synth *synth);
struct synth *synth_new(int rate,int chanc);

/* Call once immediately after construction to run at nominal volume, eg for printers.
 * Normal synths reduce all channel levels by a constant.
 */
void synth_emit_full_volume(struct synth *synth);

/* Call once immediately after construction if you know the signal will always be discarded.
 * Do not call after resources are installed or update has been called.
 * We do still operate, we generate silence, and we keep accurate time against the song.
 * This is for cases where the user has selected dummy output. No sense doing all the work of generating a proper signal then.
 */
void synth_neuter(struct synth *synth);

/* Add some artificial delay to the song, wherever it currently is.
 * We might use this at startup because some hosts have a nasty habit of dropping the first quarter-second or so of output.
 */
void synth_delay_song(struct synth *synth,double s);

/* (c) in samples -- not frames, not bytes.
 * Float is the more natural format for us. You can change formats any time.
 */
void synth_updatef(float *v,int c,struct synth *synth);
void synth_updatei(int16_t *v,int c,struct synth *synth);

/* Serial data is borrowed and must remain constant throughout synth's life.
 * Each ID can only be assigned to once; further attempts will be rejected.
 */
int synth_install_sound(struct synth *synth,int rid,const void *src,int srcc);
int synth_install_song(struct synth *synth,int rid,const void *src,int srcc);

void synth_play_song(struct synth *synth,int rid,int force,int repeat);
void synth_play_sound(struct synth *synth,int rid);
void synth_event(struct synth *synth,uint8_t chid,uint8_t opcode,uint8_t a,uint8_t b,int durms);

void synth_play_song_handoff(struct synth *synth,void *src,int srcc,int repeat);
void synth_play_song_borrow(struct synth *synth,const void *src,int srcc,int repeat);

int synth_get_song(const struct synth *synth);
double synth_get_playhead(struct synth *synth);
void synth_set_playhead(struct synth *synth,double s);

#endif
