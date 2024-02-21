/* egg_system.h
 * Global platform stuff that doesn't have any better home.
 */
 
#ifndef EGG_SYSTEM_H
#define EGG_SYSTEM_H

/* Request termination at the next convenient moment.
 * Beware that this might return or might not, varies between platforms.
 * Nonzero (status) means abnormal.
 */
void egg_terminate(int status);

/* Print to stderr or similar.
 * Might do nothing. Don't use this to communicate with the user.
 * A newline is added implicitly, and we trim one newline from (fmt) if present, in case you forget.
 */
void egg_log(const char *fmt,...);

/* The really real time, in seconds since 1 Jan 1970 UTC.
 * Games are not expected to need this. But who knows.
 */
double egg_get_real_time();

/* Local time in sensible units.
 * (year,month,day) are one-based, ie exactly as you should display them.
 * (hour) is 0..23.
 * OK to provide null vectors, we'll skip them.
 * The string version does not use locale; it's always ISO 8601: "YYYY-MM-DDTHH:MM:SS.MMM".
 * Provide a short string buffer, and we stop at the end of a complete field.
 */
void egg_get_local_time(int *year,int *month,int *day,int *hour,int *minute,int *second,int *ms);
int egg_get_local_time_string(char *dst,int dsta);

/* Return all languages the host appears to support, in order of preference.
 * Each is 2 lowercase letters: An ISO 631 language code ("en"=English, "fr"=French, ...).
 * We don't distinguish variations like "en-US" or "fr-CA", cmon, one English is as good as another.
 * (dsta) and return value are in bytes.
 */
int egg_get_languages(char *dst,int dsta);

#endif
