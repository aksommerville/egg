#!/bin/bash
# runtests.sh
# Run all the listed executables, monitor their output, and emit a report.
# We don't filter or pass parameters -- use the environment for that. (EGG_TEST_FILTER)
# Executables are not test cases! Each executable can be any number of tests.

# Feel free to comment this out. I like some indication that they're running, if it might take a while to complete.
echo "Running tests from $# executables..."

# Empty for plain text, non-empty for flashy colors. For the final one-liner report.
COLOR_FINAL_REPORT=1

before_exe() { # $1=path
  true # can't be empty
}
log_loose() { # $1=message
  #true # My preference is to silence these, unless I'm tracking down a specific bug.
  echo "$1"
}
log_detail() { # $1=message, conventionally logged only before a failure.
  echo "$1"
}
fail_test() { # $1=message
  echo -e "\x1b[31mFAIL\x1b[0m $1"
}
pass_test() { # $1=message
  true # Just noise IMHO.
  #echo -e "\x1b[32mPASS\x1b[0m $1"
}
skip_test() { # $1=message
  true # No need to tell us about each.
  #echo -e "\x1b[33mSKIP\x1b[0m $1"
}

#---------- Everything below this point should be automatic. -----------------------

EXECC=0
TESTC=0
PASSC=0
FAILC=0
SKIPC=0
while [ "$#" -gt 0 ] ; do
  EXE="$1"
  shift 1
  before_exe "$EXE"
  EXECC=$((EXECC+1))
  while IFS= read OUTPUT ; do
    read MARKER STATUS MESSAGE <<<"$OUTPUT"
    if [ "$MARKER" = EGG_TEST ] ; then
      case "$STATUS" in
        PASS)
          PASSC=$((PASSC+1))
          pass_test "$MESSAGE"
          ;;
        FAIL)
          FAILC=$((FAILC+1))
          fail_test "$MESSAGE"
          ;;
        SKIP)
          SKIPC=$((SKIPC+1))
          skip_test "$MESSAGE"
          ;;
        DETAIL)
          log_detail "$MESSAGE"
          ;;
        *)
          log_loose "$OUTPUT"
          ;;
      esac
    else
      log_loose "$OUTPUT"
    fi
  done < <( $EXE 2>&1 || echo "EGG_TEST FAIL $EXE nonzero exit status" )
done

if [ -n "$COLOR_FINAL_REPORT" ] ; then
  if [ "$FAILC" -gt 0 ] ; then
    FLAG="\x1b[41m    \x1b[0m"
  elif [ "$PASSC" -gt 0 ] ; then
    FLAG="\x1b[42m    \x1b[0m"
  else
    FLAG="\x1b[100m    \x1b[0m"
  fi
  echo -e "$FLAG $FAILC fail, $PASSC pass, $SKIPC skip"
else
  if [ "$FAILC" -gt 0 ] ; then
    FLAG="[FAIL]"
  elif [ "$PASSC" -gt 0 ] ; then
    FLAG="[PASS]"
  else
    FLAG="[----]"
  fi
  echo "$FLAG $FAILC fail, $PASSC pass, $SKIPC skip"
fi
