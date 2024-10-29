/* synth_env.h
 * Generic linear envelope.
 * Cleanup is not necessary, for either config or runner.
 */
 
#ifndef SYNTH_ENV_H
#define SYNTH_ENV_H

#define SYNTH_ENV_POINT_LIMIT 16

#define SYNTH_ENV_FLAG_INIT     0x01
#define SYNTH_ENV_FLAG_VELOCITY 0x02
#define SYNTH_ENV_FLAG_SUSTAIN  0x04
#define SYNTH_ENV_FLAG_RESERVED 0xf8

struct synth_env_config {
  uint8_t flags;
  float inivlo,inivhi;
  int susp;
  int pointc;
  struct synth_env_config_point {
    int tlo,thi; // frames
    float vlo,vhi;
  } pointv[SYNTH_ENV_POINT_LIMIT];
};

struct synth_env_runner {
  float v; // Current value.
  float dv; // Step per frame.
  int c; // Steps remaining in point.
  int pointp;
  int pointc;
  int susp; // >=0 if a sustain is in play; <0 if already released. There's no special sustain handling during the run.
  int finished;
  struct synth_env_runner_point {
    int c;
    float v;
  } pointv[SYNTH_ENV_POINT_LIMIT];
};

/* We overwrite (config) blindly, and return the amount consumed on success. Never zero.
 */
int synth_env_config_decode(struct synth_env_config *config,const void *src,int srcc,int rate);

// Values are 0..1 unless you apply bias or scale.
void synth_env_config_bias(struct synth_env_config *config,float d);
void synth_env_config_scale(struct synth_env_config *config,float d);

/* Initialize a runner from finished config and velocity.
 * You provide the delay until release as (durframes).
 * It's OK to provide something unreasonably long here, and manually release later.
 */
void synth_env_init(struct synth_env_runner *runner,const struct synth_env_config *config,float velocity,int durframes);

// Normally you provide the release time at init. But you can force it early at any time.
void synth_env_release(struct synth_env_runner *runner);

#define synth_env_next(env) ({ \
  float _v=(env).v; \
  if ((env).c>0) { \
    (env).c--; \
    (env).v+=(env).dv; \
  } else { \
    synth_env_advance(&(env)); \
  } \
  _v; \
})

// Don't call directly. synth_env_next() does it for you.
void synth_env_advance(struct synth_env_runner *runner);

#endif
