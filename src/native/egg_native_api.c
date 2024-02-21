/* egg_native_api.c
 * Public API entry points, loose things that don't need a unit to themselves.
 */
 
#include "egg_native_internal.h"
#include <stdarg.h>
#include <time.h>
#include <sys/time.h>

/* Terminate.
 */

void egg_terminate(int status) {
  egg.terminate=status?-1:1;
}

/* Log.
 */

void egg_log(const char *fmt,...) {
  if (!fmt) fmt="";
  va_list vargs;
  va_start(vargs,fmt);
  char msg[256];
  int msgc=vsnprintf(msg,sizeof(msg),fmt,vargs);
  if ((msgc<0)||(msgc>=sizeof(msg))) msgc=0;
  if (msgc&&(msg[msgc-1]==0x0a)) msgc--; // Per spec, trim one newline if present.
  fprintf(stderr,"%s: %.*s\n",egg.rompath?egg.rompath:egg.exename,msgc,msg);
}

/* Real time.
 */

double egg_get_real_time() {
  struct timeval tv={0};
  gettimeofday(&tv,0);
  return (double)tv.tv_sec+(double)tv.tv_usec/1000000.0;
}

/* Local time.
 */

void egg_get_local_time(int *year,int *month,int *day,int *hour,int *minute,int *second,int *ms) {
  time_t now=time(0);
  struct tm *tm=localtime(&now);
  if (tm) {
    if (year) *year=1900+tm->tm_year;
    if (month) *month=1+tm->tm_mon;
    if (day) *day=tm->tm_mday;
    if (hour) *hour=tm->tm_hour;
    if (minute) *minute=tm->tm_min;
    if (second) *second=tm->tm_sec;
    if (ms) *ms=((int)now)%1000;
  }
}

/* Local time as 8601 string.
 * YYYY-MM-DDTHH:MM:SS.mmm
 *     4  7  10 13 16 19  23
 */

int egg_get_local_time_string(char *dst,int dsta) {
  int year=0,month=0,day=0,hour=0,minute=0,second=0,ms=0,dstc;
  egg_get_local_time(&year,&month,&day,&hour,&minute,&second,&ms);
       if (dsta>23) dstc=snprintf(dst,dsta,"%04d-%02d-%02dT%02d:%02d:%02d.%03d",year,month,day,hour,minute,second,ms);
  else if (dsta>19) dstc=snprintf(dst,dsta,"%04d-%02d-%02dT%02d:%02d:%02d",year,month,day,hour,minute,second);
  else if (dsta>16) dstc=snprintf(dst,dsta,"%04d-%02d-%02dT%02d:%02d",year,month,day,hour,minute);
  else if (dsta>13) dstc=snprintf(dst,dsta,"%04d-%02d-%02dT%02d",year,month,day,hour);
  else if (dsta>10) dstc=snprintf(dst,dsta,"%04d-%02d-%02d",year,month,day);
  else if (dsta> 7) dstc=snprintf(dst,dsta,"%04d-%02d",year,month);
  else if (dsta> 4) dstc=snprintf(dst,dsta,"%04d",year);
  else dstc=0;
  if (dstc<dsta) dst[dstc]=0;
  return dstc;
}
