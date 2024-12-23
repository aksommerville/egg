# common.mk
# Games spawned via `eggdev project` link back to this Makefile.
# Contains all the Egg boilerplate.
# Before including, games must:
#  - Pick off 'clean' and handle that separate.
#  - Declare PRECMD.
#  - Declare OPT_ENABLE with optional units to borrow from Egg.
#  - Declare PROJNAME and PROJRDNS.
#  - Declare ENABLE_SERVER_AUDIO: Empty for WebAudio only, nonempty for server-side audio too. (for `make edit`)
#  - Declare BUILDABLE_DATA_TYPES, for resource types that you will compile. And define your rules for that.

# Cache `eggdev config`; all the little particulars of each host's build environment are Egg's problem, not the games'.
include mid/eggcfg
mid/eggcfg:;$(PRECMD) ( $(EGG_SDK)/out/eggdev config > $@ ) || ( rm -f $@ ; exit 1 )
ifneq (,$(WEB_CC))
  WEB_CC+=-I$(EGG_SDK)/src -Imid $(foreach U,$(OPT_ENABLE),-DUSE_$U=1) -DIMAGE_ENABLE_ENCODERS=0 -DIMAGE_USE_PNG=0
endif
ifneq (,$(CC))
  CC+=-I$(EGG_SDK)/src -Imid $(foreach U,$(OPT_ENABLE),-DUSE_$U=1) -DUSE_REAL_STDLIB=1
endif

SRCFILES:=$(shell find src -type f)
CFILES:=$(filter %.c,$(SRCFILES))
DATAFILES:=$(filter src/data/%,$(SRCFILES))

ifneq (,$(BUILDABLE_DATA_TYPES))
  DATPAT:=$(addprefix src/data/,$(addsuffix /%,$(BUILDABLE_DATA_TYPES)))
  MIDDATA:=$(patsubst src/data/%,mid/data/%,$(filter $(DATPAT),$(DATAFILES)))
  DATAFILES:=$(filter-out $(DATPAT),$(DATAFILES))
endif

OPT_CFILES:=$(filter $(addprefix $(EGG_SDK)/src/opt/,$(addsuffix /%,$(OPT_ENABLE))),$(shell find $(EGG_SDK)/src/opt -name '*.c'))

TOC_H:=mid/egg_rom_toc.h
DATADIRS:=$(shell find src/data -type d)
$(TOC_H):$(DATADIRS);$(PRECMD) $(EGG_SDK)/out/eggdev list src/data -ftoc > $@

# Compile to WebAssembly and pack into one lib that becomes resource code:1.
ifneq (,$(WEB_CC))
  WEB_OFILES:=$(patsubst src/%.c,mid/web/%.o,$(CFILES)) $(patsubst $(EGG_SDK)/src/opt/%.c,mid/web/%.o,$(OPT_CFILES))
  -include $(WEB_OFILES:.o=.d)
  mid/web/%.o:src/%.c|$(TOC_H);$(PRECMD) $(WEB_CC) -o$@ $<
  mid/web/%.o:$(EGG_SDK)/src/opt/%.c|$(TOC_H);$(PRECMD) $(WEB_CC) -o$@ $<
  WEB_LIB:=mid/web/code.wasm
  $(WEB_LIB):$(WEB_OFILES);$(PRECMD) $(WEB_LD) -o$@ $(WEB_OFILES) $(WEB_LDPOST)
endif

# Pack the ROM. This can work without WEB_LIB, but obviously won't be playable.
ROM:=out/$(PROJNAME).egg
all:$(ROM)
$(ROM):$(WEB_LIB) $(DATAFILES) $(MIDDATA);$(PRECMD) $(EGG_SDK)/out/eggdev pack -o$@ $(WEB_LIB) src/data $(if $(MIDDATA),mid/data) --schema=src/game/shared_symbols.h

# If we're building for web, bundle it to HTML.
# Normally this is the main event, the thing you're going to distribute.
ifneq (,$(WEB_LIB))
  HTML:=out/$(PROJNAME).html
  all:$(HTML)
  $(HTML):$(ROM);$(PRECMD) $(EGG_SDK)/out/eggdev bundle -o$@ $(ROM)
endif

# Build natively if we're doing that.
# Couple gotchas: We use real stdlib for native, so eliminate the 'stdlib' opt unit.
# And if we're on a Mac, build an application bundle too.
ifneq (,$(strip $(NATIVE_TARGET)))
  NATIVE_OPT_CFILES:=$(filter-out $(EGG_SDK)/src/opt/stdlib/%,$(OPT_CFILES))
  NATIVE_OFILES:=$(patsubst src/%.c,mid/$(NATIVE_TARGET)/%.o,$(CFILES)) $(patsubst $(EGG_SDK)/src/opt/%.c,mid/$(NATIVE_TARGET)/%.o,$(NATIVE_OPT_CFILES))
  -include $(NATIVE_OFILES:.o=.d)
  mid/$(NATIVE_TARGET)/%.o:src/%.c|$(TOC_H);$(PRECMD) $(CC) -o$@ $<
  mid/$(NATIVE_TARGET)/%.o:$(EGG_SDK)/src/opt/%.c|$(TOC_H);$(PRECMD) $(CC) -o$@ $<
  NATIVE_LIB:=mid/$(NATIVE_TARGET)/code.a
  $(NATIVE_LIB):$(NATIVE_OFILES);$(PRECMD) ar rc $@ $^
  NATIVE_EXE:=out/$(PROJNAME).$(NATIVE_TARGET)
  all:$(NATIVE_EXE)
  $(NATIVE_EXE):$(NATIVE_LIB) $(ROM);$(PRECMD) $(EGG_SDK)/out/eggdev bundle -o$@ $(ROM) $(NATIVE_LIB)

  ifeq (macos,$(NATIVE_TARGET))
    BUNDLE:=out/$(PROJNAME).app
    BUNDLE_EXE:=$(BUNDLE)/Contents/MacOS/$(PROJNAME)
    BUNDLE_PLIST:=$(BUNDLE)/Contents/Info.plist
    BUNDLE_NIB:=$(BUNDLE)/Contents/Resources/Main.nib
    #TODO icons
    $(BUNDLE_EXE):$(NATIVE_EXE);$(PRECMD) cp $< $@
    $(BUNDLE_PLIST):$(EGG_SDK)/src/opt/macos/Info.plist $(EGG_SDK)/etc/tool/plist.sh; \
      $(PRECMD) $(EGG_SDK)/etc/tool/plist.sh $(EGG_SDK)/src/opt/macos/Info.plist src/data/metadata $(PROJNAME) $(PROJRDNS) > $@
    $(BUNDLE_NIB):$(EGG_SDK)/src/opt/macos/Main.xib;$(PRECMD) ibtool --compile $@ $<
    all:$(BUNDLE_EXE) $(BUNDLE_PLIST) $(BUNDLE_NIB)
    run:$(BUNDLE_EXE) $(BUNDLE_PLIST) $(BUNDLE_NIB);open -W $(BUNDLE) --args --reopen-tty=$(shell tty)
  else
    run:$(NATIVE_EXE);$(NATIVE_EXE)
  endif
else
  run:;echo "NATIVE_TARGET unset" ; exit 1
endif

# `make edit` has some extra drama due to audio. Server can create its own audio context for playing sounds as you work on them.
ifneq (,$(ENABLE_SERVER_AUDIO))
  COMMA:=,
  EMPTY:=
  SPACE:=$(EMPTY) $(EMPTY)
  AUDIO_DRIVERS:=$(strip $(filter pulse asound alsafd macaudio msaudio,$(patsubst -DUSE_%=1,%,$(CC))))
  EDIT_AUDIO_ARGS:=--audio=$(subst $(SPACE),$(COMMA),$(AUDIO_DRIVERS)) --audio-rate=44100 --audio-chanc=2
endif
edit:;$(EGG_SDK)/out/eggdev serve \
  --htdocs=rt:$(EGG_SDK)/src/www \
  --htdocs=$(EGG_SDK)/src/editor \
  --htdocs=src/editor \
  --htdocs=src \
  --schema=src/game/shared_symbols.h \
  --write=src $(EDIT_AUDIO_ARGS)

# web-run is a real thing, but I usually prefer `make edit` and launch game from the editor.
web-run:$(ROM);$(EGG_SDK)/out/eggdev serve --htdocs=$(EGG_SDK)/src/www --htdocs=out --default-rom=/$(notdir $(ROM))
