#include "graf.h"
#include "egg/egg.h"

/* Get image.
 */
 
int texcache_get_image(struct texcache *tc,int imageid) {
  struct texcache_entry *entry=tc->entryv;
  struct texcache_entry *oldest=entry;
  int i=tc->entryc,hiseq=0;
  for (;i-->0;entry++) {
    if (entry->imageid==imageid) {
      entry->seq++;
      return entry->texid;
    }
    if (entry->seq>hiseq) hiseq=entry->seq;
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
    entry->seq=hiseq;
    return entry->texid;
  }
  if (tc->graf) graf_flush(tc->graf);
  tc->evictc++;
  int oldimageid=oldest->imageid;
  egg_texture_load_image(oldest->texid,imageid);
  oldest->imageid=imageid;
  oldest->seq=hiseq;
  return oldest->texid;
}
