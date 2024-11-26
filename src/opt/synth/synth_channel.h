/* synth_channel.h
 */
 
#ifndef SYNTH_CHANNEL_H
#define SYNTH_CHANNEL_H

#define SYNTH_CHANNEL_MODE_NOOP 0
#define SYNTH_CHANNEL_MODE_DRUM 1
#define SYNTH_CHANNEL_MODE_WAVE 2
#define SYNTH_CHANNEL_MODE_FM   3
#define SYNTH_CHANNEL_MODE_SUB  4

struct synth_channel {
  uint8_t chid;
  uint8_t mode;
  float trim;
  
  struct synth_drum {
    float trimlo,trimhi;
    struct synth_pcm *pcm; // STRONG
    const void *src; // WEAK
    int srcc;
    int warned;
  } *drumv; // null or 128 entries.
  
  struct synth_env_config levelenv; // WAVE,FM,SUB
  struct synth_env_config pitchenv; // WAVE,FM
  struct synth_env_config rangeenv; // FM; peak is baked in.
  
  struct synth_wave *wave; // WAVE
  
  float modrate; // FM
  
  float subwidth; // SUB
};

/* Zeroes (channel).
 */
void synth_channel_cleanup(struct synth_channel *channel);

/* (channel) must be cleaned up and zeroed first.
 * Initializes channel and returns consumed length.
 * (src) starts at the top of the Channel Header (u8 chid...).
 * Must be at least 6 bytes, and we validate aggressively.
 * We may borrow (src) indefinitely until cleanup; it must remain constant.
 */
int synth_channel_decode(struct synth_channel *channel,struct synth *synth,const void *src,int srcc,float global_trim);

/* Returns a STRONG voice; updating and deleting it is the caller's responsibility.
 */
struct synth_voice *synth_channel_play_note(
  struct synth_channel *channel,
  struct synth *synth,
  uint8_t noteid, // 0..127
  uint8_t velocity, // 0..127
  int durms
);

#endif
