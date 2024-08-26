/* Implementation of selections from libm.
 * All copied from newlib.
 */
 
/* Not sure if this is the correct license for every bit copied here, but they're all similar.
 * (importantly: They all grant permission to copy).
 *
(C) Copyright IBM Corp. 2009

All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

* Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.
* Redistributions in binary form must reproduce the above copyright
notice, this list of conditions and the following disclaimer in the
documentation and/or other materials provided with the distribution.
* Neither the name of IBM nor the names of its contributors may be
used to endorse or promote products derived from this software without
specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.
*/

#include "egg-stdlib.h"
#include <stdint.h>

typedef union
{
  float value;
  uint32_t word;
} ieee_float_shape_type;

typedef union 
{
  double value;
  struct 
  {
    uint32_t lsw;
    uint32_t msw;
  } parts;
} ieee_double_shape_type;

#define GET_FLOAT_WORD(i,d)					\
do {								\
  ieee_float_shape_type gf_u;					\
  gf_u.value = (d);						\
  (i) = gf_u.word;						\
} while (0)

#define SET_FLOAT_WORD(d,i)					\
do {								\
  ieee_float_shape_type sf_u;					\
  sf_u.word = (i);						\
  (d) = sf_u.value;						\
} while (0)

/* Get two 32 bit ints from a double.  */

#define EXTRACT_WORDS(ix0,ix1,d)				\
do {								\
  ieee_double_shape_type ew_u;					\
  ew_u.value = (d);						\
  (ix0) = ew_u.parts.msw;					\
  (ix1) = ew_u.parts.lsw;					\
} while (0)

/* Get the more significant 32 bit int from a double.  */

#define GET_HIGH_WORD(i,d)					\
do {								\
  ieee_double_shape_type gh_u;					\
  gh_u.value = (d);						\
  (i) = gh_u.parts.msw;						\
} while (0)

/* Get the less significant 32 bit int from a double.  */

#define GET_LOW_WORD(i,d)					\
do {								\
  ieee_double_shape_type gl_u;					\
  gl_u.value = (d);						\
  (i) = gl_u.parts.lsw;						\
} while (0)

/* Set a double from two 32 bit ints.  */

#define INSERT_WORDS(d,ix0,ix1)					\
do {								\
  ieee_double_shape_type iw_u;					\
  iw_u.parts.msw = (ix0);					\
  iw_u.parts.lsw = (ix1);					\
  (d) = iw_u.value;						\
} while (0)

/* Set the more significant 32 bits of a double from an int.  */

#define SET_HIGH_WORD(d,v)					\
do {								\
  ieee_double_shape_type sh_u;					\
  sh_u.value = (d);						\
  sh_u.parts.msw = (v);						\
  (d) = sh_u.value;						\
} while (0)

/* Set the less significant 32 bits of a double from an int.  */

#define SET_LOW_WORD(d,v)					\
do {								\
  ieee_double_shape_type sl_u;					\
  sl_u.value = (d);						\
  sl_u.parts.lsw = (v);						\
  (d) = sl_u.value;						\
} while (0)

static double BIGX = 7.09782712893383973096e+02;
static double SMALLX = -7.45133219101941108420e+02;

// Making things up.
#define DBL_MAX_EXP 99
#define DBL_MIN_EXP -99
#define z_rooteps 0.000001

double fabs(double a) {
  if (a<0.0) return -a;
  return a;
}

/* sf_sine.c
 *********************************************************************************/

static const double r[] = { -0.1666665668,
                            0.8333025139e-02,
                           -0.1980741872e-03,
                            0.2601903036e-5 };

static double
sinternal (double x,
        int cosine)
{
  int sgn, N;
  double y, XN, g, R, res;
  double YMAX = 210828714.0;

  switch (fpclassify (x))
    {
      case FP_NAN: return x;
      case FP_INFINITE: return NAN;
    }

  /* Use sin and cos properties to ease computations. */
  if (cosine)
    {
      sgn = 1;
      y = fabs (x) + M_PI_2;
    }
  else
    {
      if (x < 0.0)
        {
          sgn = -1;
          y = -x;
        }
      else
        {
          sgn = 1;
          y = x;
        }
    }

  /* Check for values of y that will overflow here. */
  if (y > YMAX)
    {
      return (x);
    }

  /* Calculate the exponent. */
  if (y < 0.0)
    N = (int) (y * M_1_PI - 0.5);
  else
    N = (int) (y * M_1_PI + 0.5);
  XN = (double) N;

  if (N & 1)
    sgn = -sgn;

  if (cosine)
    XN -= 0.5;

  y = fabs (x) - XN * M_PI;

  if (-z_rooteps < y && y < z_rooteps)
    res = y;

  else
    {
      g = y * y;

      /* Calculate the Taylor series. */
      R = (((r[3] * g + r[2]) * g + r[1]) * g + r[0]) * g;

      /* Finally, compute the result. */
      res = y + y * R;
    }
 
  res *= sgn;

  return (res);
}

double sin(double a) { return sinternal(a,0); }
double cos(double a) { return sinternal(a,1); }

/* s_atangent.c
 **************************************************************************************/

static const double ROOT3 = 1.73205080756887729353;
static const double a[] = { 0.0, 0.52359877559829887308, 1.57079632679489661923,
                     1.04719755119659774615 };
static const double q[] = { 0.41066306682575781263e+2,
                     0.86157349597130242515e+2,
                     0.59578436142597344465e+2,
                     0.15024001160028576121e+2 };
static const double p[] = { -0.13688768894191926929e+2,
                     -0.20505855195861651981e+2,
                     -0.84946240351320683534e+1,
                     -0.83758299368150059274 };

double
atan2 (double v,
        double u)
{
  double x = 0.0;
  int arctan2 = 1;
  double f, g, R, P, Q, A, res;
  int N;
  int branch = 0;
  int expv, expu;

  /* Preparation for calculating arctan2. */
      if (u == 0.0) {
        if (v == 0.0)
          {
            return NAN;
          } else {
            branch = 1;
            res = M_PI_2;
          }
      }

      if (!branch)
        {
          int e;
          /* Get the exponent values of the inputs. */
          g = frexp (v, &expv);
          g = frexp (u, &expu);

          /* See if a divide will overflow. */
          e = expv - expu;
          if (e > DBL_MAX_EXP)
            {
               branch = 1;
               res = M_PI_2;
            }

          /* Also check for underflow. */
          else if (e < DBL_MIN_EXP)
            {
               branch = 2;
               res = 0.0;
            }
         }

  if (!branch)
    {
      f = fabs (v / u);

      if (f > 1.0)
        {
          f = 1.0 / f;
          N = 2;
        }
      else {
        N = 0;
      }

      if (f > (2.0 - ROOT3))
        {
          A = ROOT3 - 1.0;
          f = (((A * f - 0.5) - 0.5) + f) / (ROOT3 + f);
          N++;
        }

      /* Check for values that are too small. */
      if (-z_rooteps < f && f < z_rooteps)
        res = f;

      /* Calculate the Taylor series. */
      else
        {
          g = f * f;
          P = (((p[3] * g + p[2]) * g + p[1]) * g + p[0]) * g;
          Q = (((g + q[3]) * g + q[2]) * g + q[1]) * g + q[0];
          R = P / Q;

          res = f + f * R;
        }

      if (N > 1)
        res = -res;

      res += a[N];
    }

      if (u < 0.0)
        res = M_PI - res;
      if (v < 0.0)
        res = -res;

  return (res);
}

/* sf_pow.c
 ***************************************************************************/

double powf (double x, double y)
{
  double d, k, t, r = 1.0;
  int n, sign, exponent_is_even_int = 0;
  int32_t px;

  GET_FLOAT_WORD (px, x);

  k = modf (y, &d);

  if (k == 0.0) 
    {
      /* Exponent y is an integer. */
      if (modf (ldexp (y, -1), &t))
        {
          /* y is odd. */
          exponent_is_even_int = 0;
        }
      else
        {
          /* y is even. */
          exponent_is_even_int = 1;
        }
    }

  if (x == 0.0)
    {
      //if (y <= 0.0)
      //  errno = EDOM;
    }
  else if ((t = y * log (fabs (x))) >= BIGX) 
    {
      if (px & 0x80000000) 
        {
          /* x is negative. */
          if (k) 
            {
              /* y is not an integer. */
              x = 0.0;
            }
          else if (exponent_is_even_int)
            x = INFINITY;
          else
            x = -INFINITY;
        }
    else
      {
        x = INFINITY;
      }
    }
  else if (t < SMALLX)
    {
      x = 0.0;
    }
  else 
    {
      if ( !k && fabs (d) <= 32767 ) 
        {
          n = (int) d;

          if ((sign = (n < 0)))
            n = -n;

          while ( n > 0 ) 
            {
              if ((unsigned int) n % 2) 
                r *= x;
              x *= x;
              n = (unsigned int) n / 2;
            }

          if (sign)
            r = 1.0 / r;

          return r;
        }
      else 
        {
          if ( px & 0x80000000 ) 
            {
              /* x is negative. */
              if (k) 
                {
                  /* y is not an integer. */
                  return 0.0;
                }
            }

          x = exp (t);

          if (!exponent_is_even_int) 
            { 
              if (px & 0x80000000)
                {
                  /* y is an odd integer, and x is negative,
                     so the result is negative. */
                  GET_FLOAT_WORD (px, x);
                  px |= 0x80000000;
                  SET_FLOAT_WORD (x, px);
                }
            }
        }
    }

  return x;
}

/* s_logarithm.c
 ****************************************************************/

static const double log_a[] = { -0.64124943423745581147e+02,
                             0.16383943563021534222e+02,
                            -0.78956112887481257267 };
static const double log_b[] = { -0.76949932108494879777e+03,
                             0.31203222091924532844e+03,
                            -0.35667977739034646171e+02 };
static const double C1 =  22713.0 / 32768.0;
static const double C2 =  1.428606820309417232e-06;
static const double C3 =  0.43429448190325182765;

static double
logarithm (double x,
        int ten)
{
  int N;
  double f, w, z;

  /* Check for range and domain errors here. */
  if (x == 0.0)
    {
      return (-INFINITY);
    }
  else if (x < 0.0)
    {
      return (NAN);
    }
  else if (!isfinite(x))
    {
      if (isnan(x))
        return (NAN);
      else
        return (INFINITY);
    }

  /* Get the exponent and mantissa where x = f * 2^N. */
  f = frexp (x, &N);

  z = f - 0.5;

  if (f > M_SQRT1_2)
    z = (z - 0.5) / (f * 0.5 + 0.5);
  else
    {
      N--;
      z /= (z * 0.5 + 0.5);
    }
  w = z * z;

  /* Use Newton's method with 4 terms. */
  z += z * w * ((log_a[2] * w + log_a[1]) * w + log_a[0]) / (((w + log_b[2]) * w + log_b[1]) * w + log_b[0]);

  if (N != 0)
    z = (N * C2 + z) + N * C1;

  if (ten)
    z *= C3;

  return (z);
}

double log(double a) {
  return logarithm(a,0);
}

double log10(double a) {
  return logarithm(a,1);
}

/* s_fmod.c
 *******************************************************************/
 
static const double one = 1.0, Zero[] = {0.0, -0.0,};

double fmod(double x, double y) {
	int32_t n,hx,hy,hz,ix,iy,sx,i;
	uint32_t lx,ly,lz;

	EXTRACT_WORDS(hx,lx,x);
	EXTRACT_WORDS(hy,ly,y);
	sx = hx&0x80000000;		/* sign of x */
	hx ^=sx;		/* |x| */
	hy &= 0x7fffffff;	/* |y| */

    /* purge off exception values */
	if((hy|ly)==0||(hx>=0x7ff00000)||	/* y=0,or x not finite */
	  ((hy|((ly|-ly)>>31))>0x7ff00000))	/* or y is NaN */
	    return (x*y)/(x*y);
	if(hx<=hy) {
	    if((hx<hy)||(lx<ly)) return x;	/* |x|<|y| return x */
	    if(lx==ly) 
		return Zero[(uint32_t)sx>>31];	/* |x|=|y| return x*0*/
	}

    /* determine ix = ilogb(x) */
	if(hx<0x00100000) {	/* subnormal x */
	    if(hx==0) {
		for (ix = -1043, i=lx; i>0; i<<=1) ix -=1;
	    } else {
		for (ix = -1022,i=(hx<<11); i>0; i<<=1) ix -=1;
	    }
	} else ix = (hx>>20)-1023;

    /* determine iy = ilogb(y) */
	if(hy<0x00100000) {	/* subnormal y */
	    if(hy==0) {
		for (iy = -1043, i=ly; i>0; i<<=1) iy -=1;
	    } else {
		for (iy = -1022,i=(hy<<11); i>0; i<<=1) iy -=1;
	    }
	} else iy = (hy>>20)-1023;

    /* set up {hx,lx}, {hy,ly} and align y to x */
	if(ix >= -1022) 
	    hx = 0x00100000|(0x000fffff&hx);
	else {		/* subnormal x, shift x to normal */
	    n = -1022-ix;
	    if(n<=31) {
	        hx = (hx<<n)|(lx>>(32-n));
	        lx <<= n;
	    } else {
		hx = lx<<(n-32);
		lx = 0;
	    }
	}
	if(iy >= -1022) 
	    hy = 0x00100000|(0x000fffff&hy);
	else {		/* subnormal y, shift y to normal */
	    n = -1022-iy;
	    if(n<=31) {
	        hy = (hy<<n)|(ly>>(32-n));
	        ly <<= n;
	    } else {
		hy = ly<<(n-32);
		ly = 0;
	    }
	}

    /* fix point fmod */
	n = ix - iy;
	while(n--) {
	    hz=hx-hy;lz=lx-ly; if(lx<ly) hz -= 1;
	    if(hz<0){hx = hx+hx+(lx>>31); lx = lx+lx;}
	    else {
	    	if((hz|lz)==0) 		/* return sign(x)*0 */
		    return Zero[(uint32_t)sx>>31];
	    	hx = hz+hz+(lz>>31); lx = lz+lz;
	    }
	}
	hz=hx-hy;lz=lx-ly; if(lx<ly) hz -= 1;
	if(hz>=0) {hx=hz;lx=lz;}

    /* convert back to floating value and restore the sign */
	if((hx|lx)==0) 			/* return sign(x)*0 */
	    return Zero[(uint32_t)sx>>31];	
	while(hx<0x00100000) {		/* normalize x */
	    hx = hx+hx+(lx>>31); lx = lx+lx;
	    iy -= 1;
	}
	if(iy>= -1022) {	/* normalize output */
	    hx = ((hx-0x00100000)|((iy+1023)<<20));
	    INSERT_WORDS(x,hx|sx,lx);
	} else {		/* subnormal output */
	    n = -1022 - iy;
	    if(n<=20) {
		lx = (lx>>n)|((uint32_t)hx<<(32-n));
		hx >>= n;
	    } else if (n<=31) {
		lx = (hx<<(32-n))|(lx>>n); hx = sx;
	    } else {
		lx = hx>>(n-32); hx = sx;
	    }
	    INSERT_WORDS(x,hx|sx,lx);
	    x *= one;		/* create necessary signal */
	}
	return x;		/* exact output */
}

double modf(double a,double *i) {
  double f=fmod(a,1.0);
  *i=a-f;
  return f;
}

/* s_sqrt.c
 ***********************************************************************/

double sqrt (double x) {
  double f, y;
  int exp, i, odd;

  /* Check for special values. */
  switch (fpclassify(x))
    {
      case FP_NAN:
        return (x);
      case FP_INFINITE:
        if (x>0.0)
          {
            return NAN;
          }
        else
          {
            return INFINITY;
          }
    }

  /* Initial checks are performed here. */
  if (x == 0.0)
    return (0.0);
  if (x < 0)
    {
      return NAN;
    }

  /* Find the exponent and mantissa for the form x = f * 2^exp. */
  f = frexp (x, &exp);

  odd = exp & 1;

  /* Get the initial approximation. */
  y = 0.41731 + 0.59016 * f;

  f /= 2.0;
  /* Calculate the remaining iterations. */
  for (i = 0; i < 3; ++i)
    y = y / 2.0 + f / y;

  /* Calculate the final value. */
  if (odd)
    {
      y *= M_SQRT1_2;
      exp++;
    }
  exp >>= 1;
  y = ldexp (y, exp);

  return (y);
}

/* s_frexp.c
 ********************************************************************/
 
double frexp (double d, int *exp)
{
  double f;
  uint32_t hd, ld, hf, lf;

  /* Check for special values. */
  switch (fpclassify(d))
    {
      case FP_NAN:
      case FP_INFINITE:
      case FP_ZERO:
        *exp = 0;
        return (d);
    }

  EXTRACT_WORDS (hd, ld, d);

  /* Get the exponent. */
  *exp = ((hd & 0x7ff00000) >> 20) - 1022;

  /* Get the mantissa. */ 
  lf = ld;
  hf = hd & 0x800fffff;  
  hf |= 0x3fe00000;

  INSERT_WORDS (f, hf, lf);

  return (f);
}

/* s_ldexp.c
 ***************************************************************/
#define DOUBLE_EXP_OFFS 1023
 
double
ldexp (double d,
        int e)
{
  int exp;
  uint32_t hd;

  GET_HIGH_WORD (hd, d);

  /* Check for special values and then scale d by e. */
  switch (fpclassify(d))
    {
      case FP_NAN: break;
      case FP_INFINITE: break;
      case FP_ZERO: break;

      default:
        exp = (hd & 0x7ff00000) >> 20;
        exp += e;

        if (exp > DBL_MAX_EXP + DOUBLE_EXP_OFFS)
         {
           d = INFINITY;
         }
        else if (exp < DBL_MIN_EXP + DOUBLE_EXP_OFFS)
         {
           d = -INFINITY;
         }
        else
         {
           hd &= 0x800fffff;
           hd |= exp << 20;
           SET_HIGH_WORD (d, hd);
         }
    }

    return (d);
}

/* s_exp.c
 ********************************************************************/
 
static const double INV_LN2 = 1.4426950408889634074;
static const double LN2 = 0.6931471805599453094172321;
static const double expp[] = { 0.25, 0.75753180159422776666e-2,
                     0.31555192765684646356e-4 };
static const double expq[] = { 0.5, 0.56817302698551221787e-1,
                     0.63121894374398504557e-3,
                     0.75104028399870046114e-6 };

double
exp (double x)
{
  int N;
  double g, z, R, P, Q;

  switch (fpclassify(x))
    {
      case FP_NAN:
        return (x);
      case FP_INFINITE:
        if (x>0.0)
          return INFINITY;
        else
          return (0.0);
      case FP_ZERO:
        return (1.0);
    }

  /* Check for out of bounds. */
  if (x > BIGX || x < SMALLX)
    {
      return (x);
    }

  /* Check for a value too small to calculate. */
  if (-z_rooteps < x && x < z_rooteps)
    {
      return (1.0);
    }

  /* Calculate the exponent. */
  if (x < 0.0)
    N = (int) (x * INV_LN2 - 0.5);
  else
    N = (int) (x * INV_LN2 + 0.5);

  /* Construct the mantissa. */
  g = x - N * LN2;
  z = g * g;
  P = g * ((expp[2] * z + expp[1]) * z + expp[0]);
  Q = ((expq[3] * z + expq[2]) * z + expq[1]) * z + expq[0];
  R = 0.5 + P / (Q - P);

  /* Return the floating point value. */
  N++;
  return (ldexp (R, N));
}

/* s_numtest.c
 *******************************************************************/
 
int 
fpclassify (double x)
{
  uint32_t hx, lx;
  int exp;

  EXTRACT_WORDS (hx, lx, x);

  exp = (hx & 0x7ff00000) >> 20;

  /* Check for a zero input. */
  if (x == 0.0) return FP_ZERO;

  /* Check for not a number or infinity. */
  if (exp == 0x7ff)
    {
      if(hx & 0xf0000 || lx) return FP_NAN;
      return FP_INFINITE;
    }
     
  /* Otherwise it's a finite value. */ 
  return FP_NORMAL;
  //TODO aks: Should we check for SUBNORMAL too? What does that mean?
}
