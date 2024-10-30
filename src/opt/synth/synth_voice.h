/* synth_voice.h
 * Abstraction of all signal-generating components.
 */
 
#ifndef SYNTH_VOICE_H
#define SYNTH_VOICE_H

/* Generic voice.
 *****************************************************************************/

struct synth_voice {
  struct synth *synth; // WEAK
  int finished; // Update must set nonzero when no more signal will be generated.
  int song; // Nonzero if we belong to the song -- will be released on a song change.
  
  /* All hooks are required.
   * They'll be populated with dummies at allocation.
   * Update is always mono, and you must add to (v), don't overwrite it.
   */
  void (*del)(struct synth_voice *voice);
  void (*update)(float *v,int c,struct synth_voice *voice);
  void (*release)(struct synth_voice *voice);
};

void synth_voice_del(struct synth_voice *voice);

struct synth_voice *synth_voice_new(struct synth *synth,int objlen);

/* Types.
 ****************************************************************************/
 
struct synth_voice *synth_voice_pcm_new(
  struct synth *synth,
  struct synth_pcm *pcm,
  float trim
);

struct synth_voice *synth_voice_wave_new(
  struct synth *synth,
  struct synth_wave *wave,
  float rate_norm,
  float velocity,
  int durframes,
  const struct synth_env_config *levelenv,
  const struct synth_env_config *pitchenv
);

struct synth_voice *synth_voice_fm_new(
  struct synth *synth,
  float modrate,
  float rate_norm,
  float velocity,
  int durframes,
  const struct synth_env_config *levelenv,
  const struct synth_env_config *pitchenv,
  const struct synth_env_config *rangeenv
);

struct synth_voice *synth_voice_sub_new(
  struct synth *synth,
  float mid_norm,
  float width_norm,
  float velocity,
  int durframes,
  const struct synth_env_config *levelenv
);

#endif
