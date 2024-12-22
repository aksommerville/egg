#!/bin/sh
# plist.sh
# For generating the MacOS Info.plist.

if [ "$#" -ne 4 ] ; then
  echo "Usage: $0 INPUT METADATA EXEBASENAME REVDNS > OUTPUT"
  exit 1
fi

GAMEEXENAME="$3"
REVDNS_IDENTIFIER="$4"
GAMETITLE=
PUBYEAR=
AUTHOR=

while IFS== read K V ; do
  case "$K" in 
    title) GAMETITLE="$V" ;;
    author) AUTHOR="$V" ;;
    time) PUBYEAR="$V" ;;
  esac
done <$2

if [ -z "$GAMETITLE" ] ; then
  GAMETITLE=Game
fi
if [ -z "$PUBYEAR" ] ; then
  PUBYEAR=$(date +%Y)
fi
if [ -z "$AUTHOR" ] ; then
  AUTHOR="$USER"
fi

sed "s/GAMEEXENAME/$GAMEEXENAME/;s/REVDNS_IDENTIFIER/$REVDNS_IDENTIFIER/;s/GAMETITLE/$GAMETITLE/;s/PUBYEAR/$PUBYEAR/;s/AUTHOR/$AUTHOR/" $1
