#include "graf.h"
#include "egg/egg.h"

/* Get image.
 */
 
int texcache_get_image(struct texcache *tc,int imageid) {
  struct texcache_entry *entry=tc->entryv;
  struct texcache_entry *oldest=entry;
  int i=tc->entryc;
  for (;i-->0;entry++) {
    if (entry->imageid==imageid) {
      entry->seq++;
      return entry->texid;
    }
    if (entry->seq<oldest->seq) oldest=entry;
  }
  if (tc->entryc<TEXCACHE_LIMIT) {
    entry=tc->entryv+tc->entryc++;
    if ((entry->texid=egg_texture_new())<1) {
      tc->entryc--;
      return 0;
    }
    egg_texture_load_image(entry->texid,imageid);
    entry->imageid=imageid;
    entry->seq=0;
    return entry->texid;
  }
  if (tc->graf) graf_flush(tc->graf);
  tc->evictc++;
  egg_texture_load_image(oldest->texid,imageid);
  oldest->imageid=imageid;
  oldest->seq=0;
  return oldest->texid;
}
