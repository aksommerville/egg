# demo.mk
# Egg games in real life should borrow etc/tool/common.mk; `make project` hooks that up for you.
# We duplicate most of it here.

demo_MIDDIR:=mid/demo
demo_OUTDIR:=out

demo_ROM:=$(demo_OUTDIR)/demo.egg
demo-all:$(demo_ROM)

demo_OPT_ENABLE:=stdlib text rom password graf
demo_OPT_DEF:=$(foreach U,$(demo_OPT_ENABLE),-DUSE_$U=1)

demo_TOC:=$(demo_MIDDIR)/egg_rom_toc.h
demo_DATADIRS:=$(shell find src/demo/data -type d)
$(demo_TOC):$(demo_DATADIRS) $(eggdev_EXE);$(PRECMD) $(eggdev_EXE) list -ftoc src/demo/data >$@

demo_CFILES:=$(filter \
  src/demo/src/%.c \
  $(addprefix src/opt/,$(addsuffix /%.c,$(demo_OPT_ENABLE))) \
,$(SRCFILES))
ifneq (,$(strip $(web_CC)))
  demo_OFILES_WASM:=$(patsubst src/%.c,$(demo_MIDDIR)/wasm/%.o,$(demo_CFILES))
  -include $(demo_OFILES_WASM:.o=.d)
  $(demo_MIDDIR)/wasm/%.o:src/%.c|$(demo_TOC);$(PRECMD) $(web_CC) -o$@ $< $(demo_OPT_DEF) -I$(demo_MIDDIR)
  $(demo_MIDDIR)/wasm/opt/image/%.o:src/opt/image/%.c;$(PRECMD) $(web_CC) -o$@ $< $(demo_OPT_DEF) -DIMAGE_ENABLE_ENCODERS=0 -DIMAGE_USE_PNG=0 -I$(demo_MIDDIR)
endif
ifneq (,$(strip $($(NATIVE_TARGET)_CC)))
  demo_OFILES_NATIVE:=$(patsubst src/%.c,$(demo_MIDDIR)/$(NATIVE_TARGET)/%.o, \
    $(filter-out src/opt/stdlib/%,$(demo_CFILES)))
  -include $(demo_OFILES_NATIVE:.o=.d)
  $(demo_MIDDIR)/$(NATIVE_TARGET)/%.o:src/%.c|$(demo_TOC);$(PRECMD) $($(NATIVE_TARGET)_CC) -o$@ $< $(demo_OPT_DEF) -DUSE_REAL_STDLIB=1 -I$(demo_MIDDIR)
endif

ifneq (,$(strip $(web_LD)))
  ifeq (,$(strip $(WASMLESS_CLANG)))
    demo_CODE1:=$(demo_MIDDIR)/data/code.wasm
    $(demo_CODE1):$(demo_OFILES_WASM);$(PRECMD) $(web_LD) -o$@ $(demo_OFILES_WASM) $(web_LDPOST)
    demo_EXTRA_DATA:=$(demo_MIDDIR)/data
  endif
endif

ifneq (,$(strip $($(NATIVE_TARGET)_AR)))
  demo_LIB_NATIVE:=$(demo_MIDDIR)/libdemo.a
  $(demo_LIB_NATIVE):$(demo_OFILES_NATIVE);$(PRECMD) $($(NATIVE_TARGET)_AR) rc $@ $^
endif

demo_DATAFILES:=$(filter src/demo/data/%,$(SRCFILES)) $(demo_CODE1)
$(demo_ROM):$(demo_DATAFILES) $(eggdev_EXE);$(PRECMD) $(eggdev_EXE) pack -o$@ src/demo/data $(demo_EXTRA_DATA) --schema=src/demo/src/demo_symbols.h

# Blank any of these if you don't want them. You do want HTML.
ifneq (,$(strip $(WAMR_SDK)))
  ifeq (,$(strip $(WASMLESS_CLANG)))
    demo_EXE_FAKE:=$(demo_OUTDIR)/demo.fake
    demo_HTML:=$(demo_OUTDIR)/demo.html
  endif
endif
ifneq (,$(strip $(WABT_SDK)))
  ifeq (,$(strip $(WASMLESS_CLANG)))
    demo_EXE_RECOM:=$(demo_OUTDIR)/demo.recom
  endif
endif

COMMA:=,
EMPTY:=
SPACE:=$(EMPTY) $(EMPTY)
demo_EDIT_AUDIO_ARGS:=--audio=$(subst $(SPACE),$(COMMA),$(strip $(filter $(eggdev_OPT_ENABLE),pulse asound alsafd macaudio msaudio))) --audio-rate=44100 --audio-chanc=2
# If you prefer web audio only, enable this:
demo_EDIT_AUDIO_ARGS:=
demo-edit:$(eggdev_EXE);$(eggdev_EXE) serve \
  --htdocs=out:out --htdocs=rt:src/www --htdocs=src/editor --htdocs=src/demo --htdocs=src/demo/editor \
  --write=src/demo \
  --gamehtml=$(demo_HTML) \
  --schema=src/demo/src/demo_symbols.h \
  $(demo_EDIT_AUDIO_ARGS)

#-------------------------------------------------------------------------------------
# In addition to the ROM file, there are 4 other forms the final output can take.
# You almost certainly want the HTML.
# In real life, it would be weird to build with all 3 native executable strategies.
# Recommendation: "true" if you don't mind compiling twice, "fake" otherwise. "recom" is more for optimizing 3rd party games.

ifneq (,$(demo_LIB_NATIVE))
  demo_EXE_TRUE:=$(demo_OUTDIR)/demo.true
  demo-all:$(demo_EXE_TRUE)
  $(demo_EXE_TRUE):$(demo_ROM) $(demo_LIB_NATIVE) $(eggdev_EXE);$(PRECMD) $(eggdev_EXE) bundle -o$@ $(demo_ROM) $(demo_LIB_NATIVE)
endif

ifneq (,$(demo_EXE_FAKE))
  demo-all:$(demo_EXE_FAKE)
  $(demo_EXE_FAKE):$(demo_ROM) $(eggdev_EXE);$(PRECMD) $(eggdev_EXE) bundle -o$@ $(demo_ROM)
endif

ifneq (,$(demo_EXE_RECOM))
  demo-all:$(demo_EXE_RECOM)
  $(demo_EXE_RECOM):$(demo_ROM) $(eggdev_EXE);$(PRECMD) $(eggdev_EXE) bundle -o$@ $(demo_ROM) --recompile
endif

ifneq (,$(demo_HTML))
  demo-all:$(demo_HTML)
  $(demo_HTML):$(demo_ROM) $(eggdev_EXE);$(PRECMD) $(eggdev_EXE) bundle -o$@ $(demo_ROM)
endif

# Well, actually *5* other forms: For MacOS, we have to wrap the executable in an Application Bundle.
ifeq ($(NATIVE_TARGET),macos)
  demo_BUNDLE:=$(demo_OUTDIR)/demo.app
  demo_BUNDLE_EXE:=$(demo_BUNDLE)/Contents/MacOS/demo
  demo_BUNDLE_PLIST:=$(demo_BUNDLE)/Contents/Info.plist
  demo_BUNDLE_NIB:=$(demo_BUNDLE)/Contents/Resources/Main.nib
  demo_BUNDLE_ICONS:=$(demo_BUNDLE)/Contents/Resources/appicon.icns
  $(demo_BUNDLE_EXE):$(demo_EXE_TRUE);$(PRECMD) cp $< $@
  $(demo_BUNDLE_PLIST):src/opt/macos/Info.plist etc/tool/plist.sh;$(PRECMD) etc/tool/plist.sh src/opt/macos/Info.plist src/demo/data/metadata demo com.aksommerville.egg.demo > $@
  $(demo_BUNDLE_NIB):src/opt/macos/Main.xib;$(PRECMD) ibtool --compile $@ $<
  $(demo_BUNDLE_ICONS):$(demo_EXE_TRUE) $(eggdev_EXE);$(PRECMD) $(eggdev_EXE) macicon -o$@ $<
  demo_BUNDLE_BITS:=$(demo_BUNDLE_EXE) $(demo_BUNDLE_PLIST) $(demo_BUNDLE_NIB) $(demo_BUNDLE_ICONS)
  demo-all:$(demo_BUNDLE_BITS)
endif
