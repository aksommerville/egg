/* synth_env.h
 * EGS has two envelope encodings:
 *  - ADSR with 8-bit values in 12 bytes with implicit zeroes fore and aft.
 *  - 16-bit values corresponding to the ADSR level envelope, for auxiliary values.
 * The second requires the first for timing.
 * All envelopes are sustainable, and we expect a release time at instantiation.
 */
 
#ifndef SYNTH_ENV_H
#define SYNTH_ENV_H

/* Configuration for either flavor of envelope.
 * Times are in milliseconds. (float because we'll be doing continuous math on them).
 * Values are in a range specified at decode.
 */
struct synth_env_config {
  float inivlo,inivhi;
  float atktlo,atkthi;
  float atkvlo,atkvhi;
  float dectlo,decthi;
  float susvlo,susvhi;
  float rlstlo,rlsthi;
  float rlsvlo,rlsvhi;
  int noop; // Decode set nonzero if every value is exactly the provided "noop" value.
};

struct synth_env_runner {
  float v;
  float dv;
  int c; // Frames remaining in leg.
  int p; // Leg: (0,1,2,3,4)=(attack,decay,sustain,release,done)
  float iniv,atkv,susv,rlsv;
  int atkt,dect,sust,rlst; // All always nonzero if configured.
};

/* Returns consumed length (always 12), or <0 on errors.
 */
int synth_env_decode_level(struct synth_env_config *dst,const void *src,int srcc);

/* Returns consumed length (always 16), or <0 on errors.
 * (ref) env is required, for timing.
 * Values are normalized to 0..1.
 * If (noopv) in 0..0xffff, we check whether every value is equal to that, and set (dst->noop) if so.
 */
int synth_env_decode_aux(
  struct synth_env_config *dst,
  const struct synth_env_config *ref,
  const void *src,int srcc,
  int noopv
);

/* Same as decode_aux, but don't normalize.
 * For pitch envelopes, where the value is cents.
 */
int synth_env_decode_aux_u16(
  struct synth_env_config *dst,
  const struct synth_env_config *ref,
  const void *src,int srcc,
  int noopv
);

/* Multiply or add a constant to each value.
 * All values are 0..1 immediately after decode.
 */
void synth_env_config_scale(struct synth_env_config *dst,float scale);
void synth_env_config_bias(struct synth_env_config *dst,float bias);

static inline int synth_env_is_ready(const struct synth_env_runner *runner) {
  if (!runner->atkt||!runner->dect||!runner->sust||!runner->rlst) return 0;
  return 1;
}

/* Begin playback of an envelope, at a specified velocity and hold time.
 * (rate) is the main playback rate. We convert times from milliseconds to frames during this call.
 * The attack, decay, and release legs always play out as encoded.
 * The sustain leg grows such that its end occurs (durms) into playback.
 * If (durms) is short, sustain length clamps to zero.
 */
void synth_env_run(struct synth_env_runner *runner,const struct synth_env_config *config,float velocity,int durms,int rate);

/* If the sustain leg is in progress, terminate it now.
 * If it hasn't started yet, drop sustain time to zero.
 * You can fake live envelopes by supplying a long (durms) initially, then releasing when you like.
 */
void synth_env_release(struct synth_env_runner *runner);

/* Begin the next leg. You shouldn't call this directly.
 */
void synth_env_advance(struct synth_env_runner *runner);

/* Advance time by one frame and return the next value.
 */
static inline float synth_env_update(struct synth_env_runner *runner) {
  float v=runner->v;
  if (runner->c>0) {
    runner->c--;
    runner->v+=runner->dv;
  } else {
    synth_env_advance(runner);
  }
  return v;
}

static inline int synth_env_is_finished(const struct synth_env_runner *runner) {
  return (runner->p>=4);
}

#endif
