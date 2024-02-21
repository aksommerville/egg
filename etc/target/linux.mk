#TODO This is a minimal Makefile for my simple initial setup where everything builds for one executable.
# It's going to get a lot more complicated in real life.

linux_MIDDIR:=mid/linux
linux_OUTDIR:=out/linux

linux_CC:=gcc -c -MMD -O3 -Isrc -Werror -Wimplicit -I/usr/include/libdrm -DUSE_alsafd=1 -DUSE_drmgx=1 -DUSE_glx=1 -DUSE_pulse=1 -DUSE_evdev=1
linux_AS:=gcc -xassembler-with-cpp -c -MMD -O3 -Isrc -I$(linux_MIDDIR)
linux_LD:=gcc
linux_LDPOST:=-lgbm -ldrm -lEGL -lX11 -lGLESv2 -lGLX -lpulse-simple

linux_CFILES:=$(filter %.c %.s,$(SRCFILES))
linux_OFILES:=$(filter-out \
  $(linux_MIDDIR)/tool/% \
  $(linux_MIDDIR)/opt/arw/% \
  $(linux_MIDDIR)/opt/png/% \
  ,$(patsubst src/%,$(linux_MIDDIR)/%.o,$(basename $(linux_CFILES))) \
)

# Build the archive, data only.
# TODO It will be a slightly different process if wasm resources are involved.
linux_DATAFILES:=$(filter src/fakegame/data/%,$(SRCFILES))
linux_ARCHIVE:=$(linux_MIDDIR)/archive
$(linux_MIDDIR)/fakegame/fakegame_archive.o:$(linux_ARCHIVE)
$(linux_ARCHIVE):$(tools_EXE_archive) $(linux_DATAFILES);$(PRECMD) $(tools_EXE_archive) -o$@ src/fakegame/data

# src/native/egg_native_rom.c is special. We build it three different ways, for the three ROM delivery methods.
linux_OFILES:=$(filter-out $(linux_MIDDIR)/native/egg_native_rom.o,$(linux_OFILES))
linux_OFILES+=$(addprefix $(linux_MIDDIR)/native/egg_native_rom.,native.o bundled.o external.o)
$(linux_MIDDIR)/native/egg_native_rom.native.o:src/native/egg_native_rom.c;$(PRECMD) $(linux_CC) -DEGG_NATIVE_ROM_MODE=NATIVE -o$@ $<
$(linux_MIDDIR)/native/egg_native_rom.bundled.o:src/native/egg_native_rom.c;$(PRECMD) $(linux_CC) -DEGG_NATIVE_ROM_MODE=BUNDLED -o$@ $<
$(linux_MIDDIR)/native/egg_native_rom.external.o:src/native/egg_native_rom.c;$(PRECMD) $(linux_CC) -DEGG_NATIVE_ROM_MODE=EXTERNAL -o$@ $<

-include $(linux_OFILES:.o=.d)
$(linux_MIDDIR)/%.o:src/%.c;$(PRECMD) $(linux_CC) -o$@ $<
$(linux_MIDDIR)/%.o:src/%.s;$(PRECMD) $(linux_AS) -o$@ $<

# In real life, the main executable should use "external" mode. Developing with "native" because it's much simpler.
linux_EXE:=$(linux_OUTDIR)/egg
linux-all:$(linux_EXE)
linux_EXE_OFILES:=$(filter-out \
  $(linux_MIDDIR)/native/egg_native_rom.bundled.o \
  $(linux_MIDDIR)/native/egg_native_rom.external.o \
  ,$(linux_OFILES))
$(linux_EXE):$(linux_EXE_OFILES);$(PRECMD) $(linux_LD) -o$@ $(linux_EXE_OFILES) $(linux_LDPOST)

#XXX TEMP: But I also want to see the external version and compare.
linux_EGGSTERNAL:=$(linux_OUTDIR)/eggsternal
linux-all:$(linux_EGGSTERNAL)
linux_EGGSTERNAL_OFILES:=$(filter-out \
  $(linux_MIDDIR)/native/egg_native_rom.bundled.o \
  $(linux_MIDDIR)/native/egg_native_rom.native.o \
  $(linux_MIDDIR)/fakegame/% \
  ,$(linux_OFILES))
$(linux_EGGSTERNAL):$(linux_EGGSTERNAL_OFILES);$(PRECMD) $(linux_LD) -o$@ $(linux_EGGSTERNAL_OFILES) $(linux_LDPOST)

linux-run:$(linux_EXE);$(linux_EXE)
