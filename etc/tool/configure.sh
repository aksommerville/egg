#!/bin/sh

DSTPATH="$1"
rm -f "$DSTPATH"

echo "******************************************"
echo "* Generating build configuration."
echo "* This is a first-time thing only."
echo "* Delete $DSTPATH to rerun in the future."
echo "******************************************"

# Final contents should be about like so:
#
# TARGETS:=eggdev demo web linux
# NATIVE_TARGET:=linux
# WAMR_SDK:=.../wasm-micro-runtime
# WABT_SDK:=.../wabt
#
# web_CC:=clang ...etc
# web_LD:=wasm-ld
# web_LDPOST:=
# web_OPT_ENABLE:=
#
# linux_CC:=gcc ...etc
# linux_AS:=...
# linux_LD:=gcc
# linux_LDPOST:=
# linux_OPT_ENABLE:=
# linux_RUN_ARGS:=

echo "# local/config.mk. Build configuration for Egg." >>$DSTPATH
echo "# This file is specific to this one build host, and does not get committed." >>$DSTPATH
echo "# Generated by $0 at $(date)" >>$DSTPATH
echo "" >>$DSTPATH

#-----------------------------------------------------------

AVAILABLE_TARGETS="$(echo etc/target/*.mk | sed -E 's/[^ ]*\/([^/]*)\.mk/\1/g')"

UNAMES="$(uname -s)"

case "$UNAMES" in
  Linux) NATIVE_TARGET=linux ;;
  Darwin) NATIVE_TARGET=macos ;;
  MINGW*) NATIVE_TARGET=mswin ;;
  *)
    echo "Unable to guess NATIVE_TARGET from 'uname -s' = '$UNAMES'."
    echo "Options: $AVAILABLE_TARGETS"
    read -p "Please specify NATIVE_TARGET: " NATIVE_TARGET
    if [ -z "$NATIVE_TARGET" ] ; then
      echo "$0: Abort"
      exit 1
    fi
  ;;
esac

echo "NATIVE_TARGET:=$NATIVE_TARGET" >>$DSTPATH
TARGETS="eggdev demo $NATIVE_TARGET"

echo "run:$NATIVE_TARGET-run" >>$DSTPATH
echo "edit:demo-edit" >>$DSTPATH

#----------------------------------------------------------

WAMR_SDK="$(find .. -type d -name wasm-micro-runtime -print -quit)"
WABT_SDK="$(find .. -type d -name wabt -print -quit)"
echo "WAMR_SDK=$WAMR_SDK" >>$DSTPATH
echo "WABT_SDK=$WABT_SDK" >>$DSTPATH

#-----------------------------------------------------------
# Take a guess at the compiler and linker for WebAssembly.

# Check whether clang can build wasm binaries.
if ( echo "int main() { return 0; }" | clang --target=wasm32 -ocfgtmp.o -c -xc - 2>/dev/null ) ; then
  rm cfgtmp.o
  WEB_OK=1
  web_LD="$( which wasm-ld wasm-ld-11 | head -n1 )"
  if [ -n "$web_LD" ] ; then
    true
  else
    echo "Failed to identify wasm-ld executable."
    WEB_OK=
  fi
else
  echo "Looks like 'clang' does not target wasm32"
fi

if [ -n "$WEB_OK" ] ; then
  TARGETS="$TARGETS web"
  echo "" >>$DSTPATH
  echo "web_CC:=clang -c -MMD -O3 --target=wasm32 -nostdlib -Werror -Wno-comment -Isrc -Wno-incompatible-library-redeclaration -Wno-builtin-requires-header" >>$DSTPATH
  echo "web_LD:=wasm-ld --no-entry -z stack-size=4194304 --no-gc-sections --allow-undefined --export-table \\" >>$DSTPATH
  echo "  --export=egg_client_init --export=egg_client_quit --export=egg_client_update --export=egg_client_render --export=egg_client_synth" >>$DSTPATH
  echo "web_LDPOST:=" >>$DSTPATH
  echo "web_OPT_ENABLE:=" >>$DSTPATH
fi

#-------------------------------------------------------------
# Guess compiler and linker for native builds.

echo "" >>$DSTPATH
echo "${NATIVE_TARGET}_CC:=gcc -c -MMD -O3 -Isrc -Werror -Wno-incompatible-library-redeclaration -Wno-builtin-declaration-mismatch" >>$DSTPATH
echo "${NATIVE_TARGET}_AS:=\$(${NATIVE_TARGET}_CC) -xassembler-with-cpp" >>$DSTPATH
echo "${NATIVE_TARGET}_LD:=gcc" >>$DSTPATH
echo "${NATIVE_TARGET}_LDPOST:=-lm -lz" >>$DSTPATH
echo "${NATIVE_TARGET}_AR:=ar" >>$DSTPATH
echo "${NATIVE_TARGET}_OPT_ENABLE:=" >>$DSTPATH

#------------------------------------------------------------
# eggdev builds with the native toolchain.

echo "" >>$DSTPATH
echo "eggdev_CC:=\$(${NATIVE_TARGET}_CC)" >>$DSTPATH
echo "eggdev_LD:=\$(${NATIVE_TARGET}_LD)" >>$DSTPATH
echo "eggdev_LDPOST:=\$(${NATIVE_TARGET}_LDPOST)" >>$DSTPATH
echo "eggdev_OPT_ENABLE:=\$(${NATIVE_TARGET}_OPT_ENABLE)" >>$DSTPATH

#------------------------------------------------------------

echo "" >>$DSTPATH
echo "TARGETS:=$TARGETS" >>$DSTPATH
