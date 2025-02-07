#include "test/egg_test.h"
#include "eggdev/eggdev_internal.h"

/* Little pieces of core functionality.
 */

EGG_ITEST(minify_whitespace) {
  const char src[]=
    " // line comment\n"
    " console . log /* block comment */ ( 'hello cruel world' ) ; \n"
    "\n"
    " /* another block comment */\n"
  "";
  struct sr_encoder min={0};
  EGG_ASSERT_CALL(eggdev_minify_inner(&min,src,sizeof(src)-1,__func__,EGGDEV_FMT_JS))
  EGG_ASSERT_STRINGS(min.v,min.c,"console.log('hello cruel world');",-1)
  sr_encoder_cleanup(&min);
  return 0;
}

EGG_ITEST(minify_number) {
  const char src[]=
    "console.log(0x00000010);\n"
  "";
  struct sr_encoder min={0};
  EGG_ASSERT_CALL(eggdev_minify_inner(&min,src,sizeof(src)-1,__func__,EGGDEV_FMT_JS))
  EGG_ASSERT_STRINGS(min.v,min.c,"console.log(16);",-1,"Literal numbers should be converted to their smallest form. Usually decimal.")
  sr_encoder_cleanup(&min);
  return 0;
}

EGG_ITEST(minify_undefined) {
  const char src[]=
    "console.log(undefined);\n"
  "";
  struct sr_encoder min={0};
  EGG_ASSERT_CALL(eggdev_minify_inner(&min,src,sizeof(src)-1,__func__,EGGDEV_FMT_JS))
  EGG_ASSERT_STRINGS(min.v,min.c,"console.log({}._);",-1,"'undefined' is too long, there are shorter ways to do it.")
  sr_encoder_cleanup(&min);
  return 0;
}

EGG_ITEST(minify_resolve_expressions) {
  const char src[]=
    "_(-(-(1)));\n"
    "_(!!!!!'');\n"
    "_(1+2*3);\n"
    "_('abc'+'def'+4);\n"
    "_(typeof(0));\n"
  "";
  struct sr_encoder min={0};
  EGG_ASSERT_CALL(eggdev_minify_inner(&min,src,sizeof(src)-1,__func__,EGGDEV_FMT_JS))
  EGG_ASSERT_STRINGS(min.v,min.c,"_(1);_(!0);_(7);_(\"abcdef4\");_(\"number\");",-1)
  sr_encoder_cleanup(&min);
  return 0;
}

EGG_ITEST(minify_add_sub_precedence) {
  // Had a bug here initially, "<" where we meant "<=", that turned this into "1-(2+3)", which is incorrect.
  const char src[]=
    "_(1-2+3);\n"
  "";
  struct sr_encoder min={0};
  EGG_ASSERT_CALL(eggdev_minify_inner(&min,src,sizeof(src)-1,__func__,EGGDEV_FMT_JS))
  EGG_ASSERT_STRINGS(min.v,min.c,"_(2);",-1)
  sr_encoder_cleanup(&min);
  return 0;
}
