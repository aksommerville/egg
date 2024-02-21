/* egg_store.h
 */
 
#ifndef EGG_STORE_H
#define EGG_STORE_H

/* Get or set an entry in the persistent store.
 * Keys must be C identifiers no longer than 32 bytes.
 * Values must be UTF-8, with no universal length limit.
 * Empty values remove the field.
 * User may impose limits beyond your control.
 */
int egg_store_get(char *v,int va,const char *k,int kc);
int egg_store_set(const char *k,int kc,const char *v,int vc);
int egg_store_key_by_index(char *k,int ka,int p);

/* Get a copy of a resource identified by (path).
 * (path) are 1..255 bytes of 0x21..0x7e.
 * Conventionally, we form paths as "TYPE/ID", where TYPE is a short C identifier and ID a decimal integer.
 * "TYPE/QUALIFIER/ID" is also a common form, eg for strings with a language.
 * *** Index is not guaranteed to report consistently across runtimes. Make no assumptions about res order. ***
 * Every runtime must of course report the same resources for one archive, just they might be in different orders.
 */
int egg_res_copy(void *dst,int dsta,const char *path);
int egg_res_path_by_index(char *path,int patha,int p);

#endif
