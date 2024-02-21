#!/bin/sh

if [ "$#" -ne 2 ] ; then
  echo "Usage: $0 OUTPUT INPUT"
  exit 1
fi
DSTPATH="$1"
SRCPATH="$2"
ADVISE_MANUAL=
TARGETS=

#------------------------
# Who's building?

UNAMES="$(uname -s)"
case "$UNAMES" in
  Linux)
    UNAMENM="$(uname -nm)"
    if [ "$UNAMENM" = "raspberrypi armv6l" ] ; then #TODO confirm this matches Pi 1 and not Pi 4.
      HOSTARCH=raspi
    else
      HOSTARCH=linux
    fi
  ;;
  Darwin)
    HOSTARCH=macos
  ;;
  MINGW*) #TODO confirm
    HOSTARCH=mswin
  ;;
  *)
    echo "$0:WARNING: Unable to determine host architecture from 'uname -s' = '$UNAMES'."
    ADVISE_MANUAL=1
    HOSTARCH=generic
  ;;
esac

# HOSTARCH is also a TARGET, unless it's "generic".
if [ "$HOSTARCH" != "generic" ] ; then
  TARGETS="$TARGETS $HOSTARCH"
fi

# Add "web" target for everybody. Can we get smarter about this?
TARGETS="$TARGETS web"

#TODO Plenty of other stuff we could check for an initial configuration. Decide what matters.

#---------------------------
# Commit and report.

sed 's/^TARGETS:=.*$/TARGETS:='"$TARGETS"'/' "$SRCPATH" > "$DSTPATH"

if [ -n "$ADVISE_MANUAL" ] ; then
  echo "$0: Finished with warnings. Please check $DSTPATH and update manually."
else
  echo "$0: Generated $DSTPATH. Rerun make to build for real."
fi
exit 1
