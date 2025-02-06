#include "test/egg_test.h"

/* String in space-delimited list.
 */
 
static int string_in_list(const char *str,int strc,const char *list,int listc) {
  if (!str) return 0;
  if (strc<0) { strc=0; while (str[strc]) strc++; }
  if (!strc) return 0;
  if (!list) return 0;
  if (listc<0) { listc=0; while (list[listc]) listc++; }
  int stopp=listc-strc;
  int listp=0;
  while (listp<=stopp) {
    if ((unsigned char)list[listp]<=0x20) { listp++; continue; }
    const char *token=list+listp;
    int tokenc=0;
    while ((listp<listc)&&((unsigned char)list[listp++]>0x20)) tokenc++;
    if ((tokenc==strc)&&!memcmp(token,str,strc)) return 1;
  }
  return 0;
}

/* Filter.
 */
 
int egg_test_filter(const char *name,const char *tags,int enable) {

  // Was a filter provided via environment?
  const char *arg=getenv("EGG_TEST_FILTER");
  if (!arg) arg="";
  int argc=0; while (arg[argc]) argc++;
  while (argc&&((unsigned char)arg[argc-1]<=0x20)) argc--;
  while (argc&&((unsigned char)arg[0]<=0x20)) { arg++; argc--; }
  
  // No manual filter, use the test case's default.
  if (!argc) return enable;
  
  // If (name) or any token in (tags) is present in (arg), run it.
  if (string_in_list(name,-1,arg,argc)) return 1;
  if (tags) {
    while (*tags) {
      if ((unsigned char)(*tags)<=0x20) { tags++; continue; }
      const char *token=tags;
      int tokenc=0;
      while ((unsigned char)(*tags)>0x20) { tags++; tokenc++; }
      if (string_in_list(token,tokenc,arg,argc)) return 1;
    }
  }
  
  return 0;
}

/* OK to print string?
 */
 
int egg_string_kosher(const char *v,int c) {
  if (!v) return 0;
  if (c<0) return 0;
  if (c>64) return 0;
  for (;c-->0;v++) {
    if ((*v<0x20)||(*v>0x7e)) return 0;
  }
  return 1;
}
