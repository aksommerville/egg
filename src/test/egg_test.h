/* egg_test.h
 * Support for tests written in C (both unit and integration).
 */
 
#ifndef EGG_TEST_H
#define EGG_TEST_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>

/* Nonzero if it's ok to log this string, ie all of:
 *  - (c) >=0
 *  - (c) under some sanity limit, 100-ish.
 *  - (v) contains only G0.
 */
int egg_string_kosher(const char *v,int c);

/* Declaring tests.
 ***************************************************************************************/

/* Integration tests must use these macros to declare the test cases.
 * Varargs are a list of tags for grouping.
 * The X macro marks as disabled by default.
 */
#define EGG_ITEST(name,...) int name()
#define XXX_EGG_ITEST(name,...) int name()

/* Unit tests should declare their cases as plain functions: int my_test_case() {...}
 * Then in their main(), name each of those cases with these macros.
 */
#define EGG_UTEST(name,...) EGG_UTEST_INTERNAL(name,#__VA_ARGS__,1)
#define XXX_EGG_UTEST(name,...) EGG_UTEST_INTERNAL(name,#__VA_ARGS__,0)

// ignore me
#define EGG_UTEST_INTERNAL(name,tags,enable) { \
  if (!egg_test_filter(#name,tags,enable)) fprintf(stderr,"EGG_TEST SKIP %s (%s) %s:%d\n",#name,tags,__FILE__,__LINE__); \
  else if (name()<0) fprintf(stderr,"EGG_TEST FAIL %s (%s) %s:%d\n",#name,tags,__FILE__,__LINE__); \
  else fprintf(stderr,"EGG_TEST PASS %s\n",#name); \
}

/* Nonzero if the test should run.
 * (enable) is its default preference.
 * Checks env EGG_TEST_FILTER.
 */
int egg_test_filter(const char *name,const char *tags,int enable);

/* Structured failure with logging.
 * Mostly you'll want "Organized assertions", below.
 *************************************************************************************/

// If you make assertions from a function that returns null on errors, undef and redef this.
// Don't forget to set it back to -1 when you're done!
#define EGG_TEST_FAILURE_VALUE (-1)

#define EGG_FAIL_MORE(k,fmt,...) { \
  fprintf(stderr,"EGG_TEST DETAIL | %15s: "fmt"\n",k,##__VA_ARGS__); \
}

#define EGG_FAIL_BEGIN(fmt,...) { \
  fprintf(stderr,"EGG_TEST DETAIL +-----------------------------------------------\n"); \
  fprintf(stderr,"EGG_TEST DETAIL | Assertion failed!\n"); \
  fprintf(stderr,"EGG_TEST DETAIL +-----------------------------------------------\n"); \
  EGG_FAIL_MORE("Location","%s:%d",__FILE__,__LINE__) \
  if (fmt[0]) EGG_FAIL_MORE("Message",fmt,##__VA_ARGS__) \
}

#define EGG_FAIL_END { \
  fprintf(stderr,"EGG_TEST DETAIL +-----------------------------------------------\n"); \
  return EGG_TEST_FAILURE_VALUE; \
}

/* Organized assertions.
 ******************************************************************************************/
 
#define EGG_FAIL(...) { \
  EGG_FAIL_BEGIN(""__VA_ARGS__) \
  EGG_FAIL_END \
}

#define EGG_ASSERT(condition,...) { \
  if (!(condition)) { \
    EGG_FAIL_BEGIN(""__VA_ARGS__) \
    EGG_FAIL_MORE("As written","%s",#condition) \
    EGG_FAIL_MORE("Expected","true") \
    EGG_FAIL_END \
  } \
}

#define EGG_ASSERT_NOT(condition,...) { \
  if (condition) { \
    EGG_FAIL_BEGIN(""__VA_ARGS__) \
    EGG_FAIL_MORE("As written","%s",#condition) \
    EGG_FAIL_MORE("Expected","false") \
    EGG_FAIL_END \
  } \
}

#define EGG_ASSERT_CALL(call,...) { \
  int _result=(call); \
  if (_result<0) { \
    EGG_FAIL_BEGIN(""__VA_ARGS__) \
    EGG_FAIL_MORE("As written","%s",#call) \
    EGG_FAIL_MORE("Expected","success") \
    EGG_FAIL_MORE("Result","%d",_result) \
    EGG_FAIL_END \
  } \
}

#define EGG_ASSERT_FAILURE(call,...) { \
  int _result=(call); \
  if (_result>=0) { \
    EGG_FAIL_BEGIN(""__VA_ARGS__) \
    EGG_FAIL_MORE("As written","%s",#call) \
    EGG_FAIL_MORE("Expected","failure") \
    EGG_FAIL_MORE("Result","%d",_result) \
    EGG_FAIL_END \
  } \
}

#define EGG_ASSERT_INTS_OP(a,op,b,...) { \
  const int _a=(a),_b=(b); \
  if (!(_a op _b)) { \
    EGG_FAIL_BEGIN(""__VA_ARGS__) \
    EGG_FAIL_MORE("As written","%s %s %s",#a,#op,#b) \
    EGG_FAIL_MORE("Values","%d %s %d",_a,#op,b) \
    EGG_FAIL_END \
  } \
}

#define EGG_ASSERT_INTS(a,b,...) EGG_ASSERT_INTS_OP(a,==,b,##__VA_ARGS__)

#define EGG_ASSERT_STRINGS(a,ac,b,bc,...) { \
  const char *_a=(char*)(a),*_b=(char*)(b); \
  int _ac=(ac),_bc=(bc); \
  if (!_a) _ac=0; else if (_ac<0) { _ac=0; while (_a[_ac]) _ac++; } \
  if (!_b) _bc=0; else if (_bc<0) { _bc=0; while (_b[_bc]) _bc++; } \
  if ((_ac!=_bc)||memcmp(_a,_b,_ac)) { \
    EGG_FAIL_BEGIN(""__VA_ARGS__) \
    EGG_FAIL_MORE("(A) As written","%s : %s",#a,#ac) \
    EGG_FAIL_MORE("(B) As written","%s : %s",#b,#bc) \
    if (egg_string_kosher(_a,_ac)) EGG_FAIL_MORE("(A) Value","'%.*s'",_ac,_a) \
    if (egg_string_kosher(_b,_bc)) EGG_FAIL_MORE("(B) Value","'%.*s'",_bc,_b) \
    EGG_FAIL_END \
  } \
}

#endif
