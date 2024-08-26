#include "egg-stdlib.h"

/* Globals.
 * Trying to keep it as simple as possible.
 * Our heap is typed int, so all allocated blocks are rounded up to 4 bytes and align on 4 bytes.
 * It's organized into blocks, each with a one-word header:
 *   <0: Available. (-n) distance to next block including this header.
 *    0: End of used heap. From here to (HEAP_SIZE>>2) can be initialized as needed.
 *   >0: In use. (n) distance to next block including this header.
 */
 
static struct {
  int heapc; // Extent to first untouched block in words. Either (heapc==HEAP_SIZE>>2) or (heap[heapc]==0).
  int deadp; // Position of header of lowest possibly available block.
  int heap[HEAP_SIZE>>2];
} gmalloc={0};

/*--------------------- Public entry points. ---------------------------------*/

void *malloc(int c) {
  if (c<0) return 0;
  if (c>=HEAP_SIZE) return 0;
  c=(c+3)>>2;
  
  /* Look for a dead block we can reuse.
   * If desired, we could skip this and only look for dead blocks when we run out of head space.
   * (in other words, move this block to below the "Append" block).
   * I think filling in first will yield better defense against fragmentation, at some performance cost.
   */
  if (1) {
    int p=gmalloc.deadp;
    while (p<gmalloc.heapc) {
      if (!gmalloc.heap[p]) break; // Error! There shouldn't be a zero header below gmalloc.heapc.
      if (gmalloc.heap[p]>0) { // In use, skip it.
        p+=gmalloc.heap[p];
        continue;
      }
      int available=-gmalloc.heap[p];
      for (;;) {
        if (available>c) { // sic > not >=
          gmalloc.heap[p]=available;
          gmalloc.deadp=p+available;
          return gmalloc.heap+p+1;
        }
        if (gmalloc.heap[p+available]<0) { // Combine adjacent unused blocks and try again.
          available-=gmalloc.heap[p+available];
        } else break;
      }
      p+=available;
    }
  }
  
  // Append.
  if (gmalloc.heapc>=HEAP_SIZE>>2) return 0; // Memory full.
  int available=(HEAP_SIZE>>2)-gmalloc.heapc; // Including the header word.
  if (available<=c) return 0; // sic <= not <
  gmalloc.heap[gmalloc.heapc]=1+c;
  void *result=gmalloc.heap+gmalloc.heapc+1;
  gmalloc.heapc+=1+c;
  if (gmalloc.heapc<HEAP_SIZE>>2) gmalloc.heap[gmalloc.heapc]=0;
  return result;
}

void free(void *p) {
  int ix=(int*)p-gmalloc.heap;
  if ((ix<1)||(ix>=HEAP_SIZE>>2)) return; // OOB pointer (eg null)
  int hdrp=ix-1;
  if (gmalloc.heap[hdrp]<=0) return; // Error! Not an allocated block.
  gmalloc.heap[hdrp]=-gmalloc.heap[hdrp];
  if (hdrp<gmalloc.deadp) gmalloc.deadp=hdrp;
}

void *realloc(void *p,int c) {
  if (!p) return malloc(c);
  if (c<0) return 0;
  if (c>=HEAP_SIZE) return 0;
  c=(c+3)>>2;
  int ix=(int*)p-gmalloc.heap;
  if ((ix<1)||(ix>=HEAP_SIZE>>2)) return 0; // OOB pointer (eg null)
  int hdrp=ix-1;
  if (gmalloc.heap[hdrp]<=0) return 0; // Error! Not an allocated block.
  
  // If it's already big enough, declare victory without changing anything.
  if (c<gmalloc.heap[hdrp]) return p;
  
  // Try to combine subsequent available blocks.
  for (;;) {
    int nextp=hdrp+gmalloc.heap[hdrp];
    if (nextp>=HEAP_SIZE>>2) break;
    if (gmalloc.heap[nextp]>0) break;
    int available_next=-gmalloc.heap[nextp];
    if (available_next) {
      gmalloc.heap[hdrp]+=available_next;
      if (c<gmalloc.heap[hdrp]) {
        return p;
      }
    } else { // Reached end of heap, can we grow right here?
      available_next=(HEAP_SIZE>>2)-hdrp;
      if (available_next>c) {
        gmalloc.heap[hdrp]=1+c;
        gmalloc.heapc=hdrp+1+c;
        if (gmalloc.heapc<HEAP_SIZE>>2) gmalloc.heap[gmalloc.heapc]=0;
        return p;
      }
      break;
    }
  }
  
  // Allocate a new block, copy content, and free the old one.
  void *nv=malloc(c<<2);
  if (!nv) return 0;
  memcpy(nv,p,(gmalloc.heap[hdrp]-1)<<2);
  gmalloc.heap[hdrp]=-gmalloc.heap[hdrp];
  if (hdrp<gmalloc.deadp) gmalloc.deadp=hdrp;
  return nv;
}

void *calloc(int c,int size) {
  if ((c<0)||(size<0)) return 0;
  if (!c||!size) return malloc(0); // Unlike libc, we always do allow malloc(0).
  if (c>HEAP_SIZE/size) return 0;
  void *v=malloc(c*size);
  if (!v) return 0;
  memset(v,0,c*size);
  return v;
}
