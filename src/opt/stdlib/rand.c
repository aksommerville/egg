/* rand.c
 * Using George Marsaglia's Xorshift algorithm, as described here: https://en.wikipedia.org/wiki/Xorshift
 */

#include "egg-stdlib.h"

double egg_time_real();

static struct {
  unsigned int state;
} grand={0};

int rand() {
  grand.state^=grand.state<<13;
  grand.state^=grand.state>>17;
  grand.state^=grand.state<<5;
  return grand.state&0x7fffffff;
}

void srand(int seed) {
  grand.state=seed;
}

void srand_auto() {
  double now=egg_time_real();
  int seed=(int)now;
  int mixer=65521;
  for (;;) {
    int onec=0;
    unsigned int q=seed;
    for (;q;q>>=1) if (q&1) onec++;
    if ((onec>=10)&&(onec<=20)) {
      srand(seed);
      return;
    }
    seed^=mixer;
    mixer+=65521;
  }
}
