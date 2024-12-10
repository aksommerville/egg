all:
.SILENT:
.SECONDARY:
PRECMD=echo "  $@" ; mkdir -p "$(@D)" ;

ifneq (,$(strip $(filter clean,$(MAKECMDGOALS))))
clean:;rm -rf mid out
else

OPT_ENABLE:=stdlib graf text rom

ifndef EGG_SDK
  EGG_SDK:=../egg
endif

# Let eggdev configure us.
include mid/eggcfg
mid/eggcfg:;$(PRECMD) ( $(EGG_SDK)/out/eggdev config > $@ ) || ( rm -f $@ ; exit 1 )
WEB_CC+=-I$(EGG_SDK)/src -Imid $(foreach U,$(OPT_ENABLE),-DUSE_$U=1) -DIMAGE_ENABLE_ENCODERS=0 -DIMAGE_USE_PNG=0
CC+=-I$(EGG_SDK)/src -Imid $(foreach U,$(OPT_ENABLE),-DUSE_$U=1) -DUSE_REAL_STDLIB=1

SRCFILES:=$(shell find src -type f)
CFILES:=$(filter %.c,$(SRCFILES))
DATAFILES:=$(filter src/data/%,$(SRCFILES))

OPT_CFILES:=$(filter $(addprefix $(EGG_SDK)/src/opt/,$(addsuffix /%,$(OPT_ENABLE))),$(shell find $(EGG_SDK)/src/opt -name '*.c'))

TOC_H:=mid/egg_rom_toc.h
DATADIRS:=$(shell find src/data -type d)
$(TOC_H):$(DATADIRS);$(PRECMD) $(EGG_SDK)/out/eggdev list src/data -ftoc > $@

WEB_OFILES:=$(patsubst src/%.c,mid/web/%.o,$(CFILES)) $(patsubst $(EGG_SDK)/src/opt/%.c,mid/web/%.o,$(OPT_CFILES))
-include $(WEB_OFILES:.o=.d)
mid/web/%.o:src/%.c|$(TOC_H);$(PRECMD) $(WEB_CC) -o$@ $<
mid/web/%.o:$(EGG_SDK)/src/opt/%.c|$(TOC_H);$(PRECMD) $(WEB_CC) -o$@ $<
WEB_LIB:=mid/web/code.wasm
$(WEB_LIB):$(WEB_OFILES);$(PRECMD) $(WEB_LD) -o$@ $(WEB_OFILES) $(WEB_LDPOST)

ROM:=out/game.egg
all:$(ROM)
$(ROM):$(WEB_LIB) $(DATAFILES);$(PRECMD) $(EGG_SDK)/out/eggdev pack -o$@ $(WEB_LIB) src/data --schema=src/game/shared_symbols.h

HTML:=out/game.html
all:$(HTML)
$(HTML):$(ROM);$(PRECMD) $(EGG_SDK)/out/eggdev bundle -o$@ $(ROM)

ifneq (,$(strip $(NATIVE_TARGET)))
  NATIVE_OPT_CFILES:=$(filter-out $(EGG_SDK)/src/opt/stdlib/%,$(OPT_CFILES))
  NATIVE_OFILES:=$(patsubst src/%.c,mid/$(NATIVE_TARGET)/%.o,$(CFILES)) $(patsubst $(EGG_SDK)/src/opt/%.c,mid/$(NATIVE_TARGET)/%.o,$(NATIVE_OPT_CFILES))
  -include $(NATIVE_OFILES:.o=.d)
  mid/$(NATIVE_TARGET)/%.o:src/%.c|$(TOC_H);$(PRECMD) $(CC) -o$@ $<
  mid/$(NATIVE_TARGET)/%.o:$(EGG_SDK)/src/opt/%.c|$(TOC_H);$(PRECMD) $(CC) -o$@ $<
  NATIVE_LIB:=mid/$(NATIVE_TARGET)/code.a
  $(NATIVE_LIB):$(NATIVE_OFILES);$(PRECMD) ar rc $@ $^
  NATIVE_EXE:=out/game.$(NATIVE_TARGET)
  all:$(NATIVE_EXE)
  $(NATIVE_EXE):$(NATIVE_LIB) $(ROM);$(PRECMD) $(EGG_SDK)/out/eggdev bundle -o$@ $(ROM) $(NATIVE_LIB)
  run:$(NATIVE_EXE);$(NATIVE_EXE) --input-config=out/input
else
  run:;echo "NATIVE_TARGET unset" ; exit 1
endif

COMMA:=,
EMPTY:=
SPACE:=$(EMPTY) $(EMPTY)
AUDIO_DRIVERS:=$(strip $(filter pulse asound alsafd macaudio msaudio,$(patsubst -DUSE_%=1,%,$(CC))))
EDIT_AUDIO_ARGS:=--audio=$(subst $(SPACE),$(COMMA),$(AUDIO_DRIVERS)) --audio-rate=44100 --audio-chanc=2
# If you prefer web audio only, enable this:
EDIT_AUDIO_ARGS:=
edit:;$(EGG_SDK)/out/eggdev serve \
  --htdocs=rt:$(EGG_SDK)/src/www \
  --htdocs=$(EGG_SDK)/src/editor \
  --htdocs=src/editor \
  --htdocs=src \
  --schema=src/game/shared_symbols.h \
  --write=src $(EDIT_AUDIO_ARGS)

web-run:$(ROM);$(EGG_SDK)/out/eggdev serve --htdocs=$(EGG_SDK)/src/www --htdocs=out --default-rom=/$(notdir $(ROM))

endif
