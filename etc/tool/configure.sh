#!/bin/sh

DSTPATH="$1"
rm -f "$DSTPATH"

echo "******************************************"
echo "* Generating build configuration."
echo "* This is a first-time thing only."
echo "* Delete $DSTPATH to rerun in the future."
echo "* Please review $DSTPATH, then make again."
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
  echo "web_CC:=clang -c -MMD -O3 --target=wasm32 -nostdlib -Werror -Wno-comment -Wno-parentheses -Isrc -Wno-incompatible-library-redeclaration -Wno-builtin-requires-header" >>$DSTPATH
  echo "web_LD:=$(web_LD) --no-entry -z stack-size=4194304 --no-gc-sections --allow-undefined --export-table \\" >>$DSTPATH
  echo "  --export=egg_client_init --export=egg_client_quit --export=egg_client_update --export=egg_client_render" >>$DSTPATH
  echo "web_LDPOST:=" >>$DSTPATH
  echo "web_OPT_ENABLE:=" >>$DSTPATH
fi

#-------------------------------------------------------------
# Guess compiler and linker for native builds.

NATIVE_CFLAGS=
NATIVE_LDFLAGS=
NATIVE_LDPOST=
NATIVE_OPT_ENABLE=

case "$NATIVE_TARGET" in

  linux)
    NATIVE_LDPOST="-lpthread"
    NEED_EGL=
    NEED_GLES2=
    # alsafd asound bcm drmgx evdev pulse xegl
    
    # alsafd and evdev are available on all Linux systems and don't have any library dependencies.
    NATIVE_OPT_ENABLE="alsafd evdev"
    
    # asound is usually available. Include if its header is found.
    if [ -f /usr/include/alsa/asoundlib.h ] ; then
      NATIVE_OPT_ENABLE="$NATIVE_OPT_ENABLE asound"
      NATIVE_LDPOST="$NATIVE_LDPOST -lasound"
    fi
    
    # pulse is usually available.
    if [ -f /usr/include/pulse/pulseaudio.h ] ; then
      NATIVE_OPT_ENABLE="$NATIVE_OPT_ENABLE pulse"
      NATIVE_LDPOST="$NATIVE_LDPOST -lpulse-simple"
    fi
    
    # drmgx is usually available. Needs some include paths too.
    if [ -f /usr/include/libdrm/drm.h ] ; then
      NATIVE_OPT_ENABLE="$NATIVE_OPT_ENABLE drmgx"
      NATIVE_CFLAGS="$NATIVE_CFLAGS -I/usr/include/libdrm"
      NATIVE_LDPOST="$NATIVE_LDPOST -ldrm -lgbm"
      NEED_EGL=1
      NEED_GLES2=1
    fi
    
    # xegl should be available on desktop systems.
    if [ -f /usr/include/X11/X.h ] && [ -f /usr/include/EGL/egl.h ] ; then
      NATIVE_OPT_ENABLE="$NATIVE_OPT_ENABLE xegl"
      NATIVE_LDPOST="$NATIVE_LDPOST -lX11"
      NEED_GLES2=1
      NEED_EGL=1
    fi
    
    # bcm is only for older Raspberry Pi, unusual.
    if [ -f /opt/vc/include/bcm_host.h ] ; then
      NATIVE_OPT_ENABLE="$NATIVE_OPT_ENABLE bcm"
      NATIVE_CFLAGS="$NATIVE_CFLAGS -I/opt/vc/include"
      NATIVE_LDFLAGS="$NATIVE_LDFLAGS -L/opt/vc/lib"
      NATIVE_LDPOST="$NATIVE_LDPOST -lbcm_host"
      NEED_GLES2=1
      NEED_EGL=1
    fi
    
    if [ -n "$NEED_GLES2" ] ; then
      NATIVE_LDPOST="$NATIVE_LDPOST -lGLESv2"
    fi
    if [ -n "$NEED_EGL" ] ; then
      NATIVE_LDPOST="$NATIVE_LDPOST -lEGL"
    fi
  ;;
  
  macos)
    NATIVE_OPT_ENABLE="macos macwm machid"
  ;;
  
  mswin)
    NATIVE_OPT_ENABLE="mswin"
  ;;
  
esac

echo "" >>$DSTPATH
echo "${NATIVE_TARGET}_CC:=gcc -c -MMD -O3 -Isrc -Werror -Wno-incompatible-library-redeclaration -Wno-builtin-declaration-mismatch $NATIVE_CFLAGS" >>$DSTPATH
echo "${NATIVE_TARGET}_AS:=\$(${NATIVE_TARGET}_CC) -xassembler-with-cpp" >>$DSTPATH
echo "${NATIVE_TARGET}_LD:=gcc $NATIVE_LDFLAGS" >>$DSTPATH
echo "${NATIVE_TARGET}_LDPOST:=-lm -lz $NATIVE_LDPOST" >>$DSTPATH
echo "${NATIVE_TARGET}_AR:=ar" >>$DSTPATH
echo "${NATIVE_TARGET}_OPT_ENABLE:=$NATIVE_OPT_ENABLE" >>$DSTPATH
echo "${NATIVE_TARGET}_RUN_ARGS:=--input-config=local/input" >>$DSTPATH

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
