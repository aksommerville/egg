#include "eggrt_internal.h"
#include <time.h>
#include <sys/time.h>
#include <unistd.h>

// We will only report intervals between these two fences.
#define EGGRT_FRAME_RATE 1.0/60.0
#define EGGRT_TOO_LONG_DELAY 0.050

/* Primitives.
 */

#if USE_mswin

#include <Windows.h>

double eggrt_now_real() {
  struct timeval tv={0};
  gettimeofday(&tv,0);
  return (double)tv.tv_sec+(double)tv.tv_usec/1000000.0;
}

double eggrt_now_cpu() {
  return eggrt_now_real(); // We're going to report 100% CPU usage, oh well.
}

void eggrt_sleep(double s) {
  Sleep((int)(s*1000.0));
}

#else
 
double eggrt_now_real() {
  struct timespec tv={0};
  clock_gettime(CLOCK_REALTIME,&tv);
  return (double)tv.tv_sec+(double)tv.tv_nsec/1000000000.0;
}

double eggrt_now_cpu() {
  struct timespec tv={0};
  clock_gettime(CLOCK_PROCESS_CPUTIME_ID,&tv);
  return (double)tv.tv_sec+(double)tv.tv_nsec/1000000000.0;
}

void eggrt_sleep(double s) {
  usleep((int)(s*1000000.0));
}

#endif

/* Init.
 */
 
void eggrt_clock_init() {
  eggrt.framelen=EGGRT_FRAME_RATE;
  eggrt.pvtime=eggrt_now_real()-eggrt.framelen;
  eggrt.framec=0;
  eggrt.starttime_real=eggrt.pvtime;
  eggrt.starttime_cpu=eggrt_now_cpu();
  eggrt.clock_faultc=0;
}

/* Update.
 */
 
double eggrt_clock_update() {
  double now=eggrt_now_real();
  double elapsed=now-eggrt.pvtime;
  if (elapsed<=0.0) {
    eggrt.clock_faultc++;
    elapsed=eggrt.framelen;
  } else if (elapsed>EGGRT_TOO_LONG_DELAY) {
    eggrt.clock_faultc++;
    elapsed=eggrt.framelen;
  } else {
    while (elapsed<eggrt.framelen) {
      eggrt_sleep(eggrt.framelen-elapsed);
      now=eggrt_now_real();
      elapsed=now-eggrt.pvtime;
    }
  }
  eggrt.pvtime=now;
  eggrt.framec++;
  return elapsed;
}

/* Report.
 */
 
void eggrt_clock_report() {
  if (eggrt.framec<1) return;
  double end_real=eggrt_now_real();
  double end_cpu=eggrt_now_cpu();
  double elapsed=end_real-eggrt.starttime_real;
  double avgrate=(double)eggrt.framec/elapsed;
  double cpuload=(end_cpu-eggrt.starttime_cpu)/elapsed;
  fprintf(stderr,"%s: %d frames in %.03f s, average %.03f Hz, CPU load %.06f.\n",eggrt.exename,eggrt.framec,elapsed,avgrate,cpuload);
}
