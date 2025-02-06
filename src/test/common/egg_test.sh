# egg_test.sh
# Utilities to include in an Egg automation test.

# Call with the name of a function, and optionally any non-empty string to ignore by default.
# Function must return zero (ie default) to pass, or return nonzero to fail.
EGG_ATEST() {
  if [ -n "$EGG_TEST_FILTER" ] ; then
    if ( echo "$EGG_TEST_FILTER" | grep -wq "$1" ) ; then
      break
    else
      echo "EGG_TEST SKIP $1 $0"
      return
    fi
  elif [ -n "$2" ] ; then
    echo "EGG_TEST SKIP $1 $0"
    return
  fi
  if $1 ; then
    echo "EGG_TEST PASS $1"
  else
    echo "EGG_TEST FAIL $1 $0"
  fi
}
