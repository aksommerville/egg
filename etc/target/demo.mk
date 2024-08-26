demo_MIDDIR:=mid/demo
demo_OUTDIR:=out

demo_ROM:=$(demo_OUTDIR)/demo.egg
demo-all:$(demo_ROM)

demo_OPT_ENABLE:=stdlib

demo_CFILES:=$(filter \
  src/demo/src/%.c \
  $(addprefix src/opt/,$(addsuffix /%.c,$(demo_OPT_ENABLE))) \
,$(SRCFILES))
ifneq (,$(strip $(web_CC)))
  demo_OFILES_WASM:=$(patsubst src/%.c,$(demo_MIDDIR)/wasm/%.o,$(demo_CFILES))
  -include $(demo_OFILES_WASM:.o=.d)
  $(demo_MIDDIR)/wasm/%.o:src/%.c;$(PRECMD) $(web_CC) -o$@ $<
endif
ifneq (,$(strip $($(NATIVE_TARGET)_CC)))
  demo_OFILES_NATIVE:=$(patsubst src/%.c,$(demo_MIDDIR)/$(NATIVE_TARGET)/%.o, \
    $(filter-out src/opt/stdlib/%,$(demo_CFILES)))
  -include $(demo_OFILES_NATIVE:.o=.d)
  $(demo_MIDDIR)/$(NATIVE_TARGET)/%.o:src/%.c;$(PRECMD) $($(NATIVE_TARGET)_CC) -o$@ $< -DUSE_REAL_STDLIB=1
endif

ifneq (,$(strip $(web_LD)))
  demo_CODE1:=$(demo_MIDDIR)/data/code.wasm
  $(demo_CODE1):$(demo_OFILES_WASM);$(PRECMD) $(web_LD) -o$@ $(demo_OFILES_WASM) $(web_LDPOST)
  demo_EXTRA_DATA:=$(demo_MIDDIR)/data
endif

ifneq (,$(strip $($(NATIVE_TARGET)_AR)))
  demo_LIB_NATIVE:=$(demo_MIDDIR)/libdemo.a
  $(demo_LIB_NATIVE):$(demo_OFILES_NATIVE);$(PRECMD) $($(NATIVE_TARGET)_AR) rc $@ $^
endif

demo_DATAFILES:=$(filter src/demo/data/%,$(SRCFILES)) $(demo_CODE1)
$(demo_ROM):$(demo_DATAFILES) $(eggdev_EXE);$(PRECMD) $(eggdev_EXE) pack -o$@ src/demo/data $(demo_EXTRA_DATA)

demo-edit:$(eggdev_EXE);$(eggdev_EXE) serve --htdocs=src/editor --htdocs=src/demo --htdocs=src/demo/editor --write=src/demo

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

demo_EXE_FAKE:=$(demo_OUTDIR)/demo.fake
demo-all:$(demo_EXE_FAKE)
$(demo_EXE_FAKE):$(demo_ROM) $(eggdev_EXE);$(PRECMD) $(eggdev_EXE) bundle -o$@ $(demo_ROM)

demo_EXE_RECOM:=$(demo_OUTDIR)/demo.recom
demo-all:$(demo_EXE_RECOM)
$(demo_EXE_RECOM):$(demo_ROM) $(eggdev_EXE);$(PRECMD) $(eggdev_EXE) bundle -o$@ $(demo_ROM) --recompile

demo_HTML:=$(demo_OUTDIR)/demo.html
demo-all:$(demo_HTML)
$(demo_HTML):$(demo_ROM) $(eggdev_EXE);$(PRECMD) $(eggdev_EXE) bundle -o$@ $(demo_ROM)
