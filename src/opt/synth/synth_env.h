/* synth_env.h
 * Linear envelope.
 */
 
#ifndef SYNTH_ENV_H
#define SYNTH_ENV_H

struct synth_env {
  float initlo,inithi;
  int susp;
  int pointc;
  struct synth_env_point {
    float tlo,thi; // frames, but typed float because they will lirp every time
    float vlo,vhi;
  } pointv[];
};

struct synth_env_runner {
  float level;
  int hold;
  float d;
  int c;
  int pointp;
  float target;
  int released;
  int deferrelease;
  float loweight,hiweight; // sum is 1.0
  const struct synth_env *env; // WEAK
};

struct synth_env *synth_env_decode(const void *src,int srcc,int rate);
void synth_env_adjust_values(struct synth_env *env,float bias,float scale); // 0..1 by default

void synth_env_runner_init(struct synth_env_runner *runner,const struct synth_env *env,float velocity);

void synth_env_runner_release(struct synth_env_runner *runner);
void synth_env_runner_release_later(struct synth_env_runner *runner,int framec);

static inline int synth_env_runner_is_finished(const struct synth_env_runner *runner) {
  return (runner->hold&&runner->released);
}

#define synth_env_runner_next(runner) ({ \
  float level=(runner).level; \
  if ((runner).deferrelease) { \
    if (!--(runner).deferrelease) { \
      synth_env_runner_release(&(runner)); \
    } \
  } \
  if (!(runner).hold) { \
    if ((runner).c>0) { \
      (runner).c--; \
      (runner).level+=(runner).d; \
    } else { \
      synth_env_runner_advance(&(runner)); \
    } \
  } \
  level; \
})

void synth_env_runner_advance(struct synth_env_runner *runner);

#endif
