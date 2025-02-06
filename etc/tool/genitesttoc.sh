#!/bin/sh
# genitesttoc.sh
# Generates the listing of integration tests.

if [ "$#" -lt 1 ] ; then
  echo "Usage: $0 OUTPUT [INPUT...]"
  exit 1
fi

DSTPATH="$1"
shift 1
TMPTOC=egg-toc-tmp
rm -rf $TMPTOC
touch $TMPTOC

cat - >"$DSTPATH" <<EOF
/* $(basename $DSTPATH)
 * Generated at $(date)
 */
#ifndef EGG_ITEST_TOC_H
#define EGG_ITEST_TOC_H

EOF

while [ "$#" -gt 0 ] ; do
  SRCPATH="$1"
  shift 1
  SRCBASE="$(basename $SRCPATH)"
  nl -ba -s' ' "$SRCPATH" | \
    sed -En 's/^ *([0-9]+) +(XXX_)?EGG_ITEST\( *([0-9a-zA-Z_]+) *(, *([^\)]*))?\).*/'"$SRCBASE"' \1 _\2 \3 \5/p' >>$TMPTOC
done

while read F L I N T ; do
  echo "int $N();" >>"$DSTPATH"
done <$TMPTOC 

cat - >>"$DSTPATH" <<EOF

static const struct egg_itest {
  int (*fn)();
  const char *name;
  const char *tags;
  int enable;
  const char *file;
  int line;
} egg_itestv[]={
EOF

while read F L I N T ; do
  if [ "$I" = _XXX_ ] ; then
    ENABLE=0
  else
    ENABLE=1
  fi
  echo "  {$N,\"$N\",\"$T\",$ENABLE,\"$F\",$L}," >>"$DSTPATH"
done <$TMPTOC

cat - >>"$DSTPATH" <<EOF
};

#endif
EOF

rm $TMPTOC
