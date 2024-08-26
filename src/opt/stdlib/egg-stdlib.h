/* egg-stdlib.h
 * Selections from libc and libm for use in Egg.
 * Compile with -Wno-incompatible-library-redeclaration.
 * The libm bits were borrowed from newlib: https://sourceware.org/newlib/
 */
 
#ifndef EGG_STDLIB_H
#define EGG_STDLIB_H

/* Define USE_REAL_STDLIB nonzero if you actually have real libc and libm.
 * eg compiling your Egg game for the true-native case.
 * No need to compile any of this unit then.
 */
#if USE_REAL_STDLIB
  #include <string.h>
  #include <stdlib.h>
  #include <limits.h>
  #include <math.h>
  #include <time.h>
  #include <stdio.h>
  static inline void srand_auto() { srand(time(0)); }
#else

#define INT_MIN (int)(0x80000000)
#define INT_MAX (int)(0x7fffffff)

/* Malloc will take so many bytes statically and that's all you ever get.
 * We do not support dynamic resizing of the heap. (but WebAssembly does; you can implement on your own).
 * It's safe to change this value, just be sure malloc.c recompiles after.
 */
#define HEAP_SIZE (16<<20)

/* Our malloc et al work about like the standard ones.
 * We do allow allocated zero-length chunks.
 * Invalid free() will quietly noop, if we can tell it's invalid -- free(0) should always be safe.
 * Chunks will always align to 4-byte boundaries.
 */
void *malloc(int c);
void free(void *p);
void *realloc(void *p,int c);
void *calloc(int c,int size);

void *memcpy(void *dst,const void *src,unsigned long c);
void *memmove(void *dst,const void *src,int c);
int memcmp(const void *a,const void *b,int c);
int strncmp(const char *a,const char *b,int limit);
char *strdup(const char *src);
void *memset(void *dst,unsigned char src,int c);

/* Beware: A seed of zero causes rand() to only return zeroes, and that's where it is by default.
 * rand() returns values 0 thru 0x7fffffff, never negative.
 * srand_auto() is my addition, it pulls current time as a source and ensures that the state is nonzero.
 */
int rand();
void srand(int seed);
void srand_auto();

/* Our fprintf and snprintf do not work exactly like standard ones.
 * Some key inconsistencies:
 *  - No length modifiers are supported (eg "%lld"). We never read 64-bit integers.
 *  - No "*m$" for reading arguments at some index.
 *  - "%e" "%f" "%g" "%a", all print the same way.
 *  - "%ls" not supported, we only do 'char' and never 'wchar_t'.
 *  - "%n" to write emitted length is not supported.
 *  - "%m" for strerror(errno) not supported, since we aren't providing errno.
 *  - We're not doing field width, alignment, or padding yet. (TODO)
 * Also, fprintf() under Egg will always write immediately and will get a newline whether you include it or not.
 */
extern void *stderr;
extern void *stdout;
int fprintf(void *unused,const char *fmt,...);
int snprintf(char *dst,int dsta,const char *fmt,...);

/* Yoinked from newlib.
 */
#define M_E		2.7182818284590452354
#define M_LOG2E		1.4426950408889634074
#define M_LOG10E	0.43429448190325182765
#define M_LN2		0.693147180559945309417
#define M_LN10		2.30258509299404568402
#define M_PI		3.14159265358979323846
#define M_PI_2		1.57079632679489661923
#define M_PI_4		0.78539816339744830962
#define M_1_PI		0.31830988618379067154
#define M_2_PI		0.63661977236758134308
#define M_2_SQRTPI	1.12837916709551257390
#define M_SQRT2		1.41421356237309504880
#define M_SQRT1_2	0.70710678118654752440
#define M_TWOPI         (M_PI * 2.0)
#define M_3PI_4		2.3561944901923448370E0
#define M_SQRTPI        1.77245385090551602792981
#define M_LN2LO         1.9082149292705877000E-10
#define M_LN2HI         6.9314718036912381649E-1
#define M_SQRT3	1.73205080756887719000
#define M_IVLN10        0.43429448190325182765 /* 1 / log(10) */
#define M_LOG2_E        0.693147180559945309417
#define M_INVLN2        1.4426950408889633870E0  /* 1 / log(2) */
#define FP_NAN         0
#define FP_INFINITE    1
#define FP_ZERO        2
#define FP_SUBNORMAL   3
#define FP_NORMAL      4
#define NAN (__builtin_nanf(""))
#define INFINITY (__builtin_inff())

/* Math functions will not set errno, because we're not implementing errno.
 */
double sin(double a);
double cos(double a);
double atan2(double a,double b);
double pow(double a,double b);
double log(double a);
double log10(double a);
double modf(double a,double *i);
double fmod(double a,double b);
double sqrt(double a);
double frexp(double a,int *exp);
double ldexp(double x,int exp);
double exp(double x);
double fabs(double a);
int fpclassify(double a);
static inline int isfinite(double a) { switch (fpclassify(a)) { case FP_NAN: case FP_INFINITE: return 0; } return 1; }
static inline int isnormal(double a) { return (fpclassify(a)==FP_NORMAL); }
static inline int isnan(double a) { return (fpclassify(a)==FP_NAN); }
static inline int isinf(double a) { return (fpclassify(a)==FP_INFINITE); }

#endif
#endif
