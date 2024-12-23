macos_MIDDIR:=mid/macos
macos_OUTDIR:=out/macos

macos_OPT_ENABLE+=serial fs rom image hostio render synth
macos_CC+=$(foreach U,$(macos_OPT_ENABLE),-DUSE_$U=1)
macos_OBJC+=$(foreach U,$(macos_OPT_ENABLE),-DUSE_$U=1)

macos_CFILES:=$(filter \
  src/eggrt/% \
  $(addprefix src/opt/,$(addsuffix /%,$(macos_OPT_ENABLE))) \
,$(SRCFILES))

macos_OFILES:=$(patsubst src/%,$(macos_MIDDIR)/%.o,$(basename $(filter %.c %.m,$(macos_CFILES))))
macos_OFILES:=$(filter-out $(macos_MIDDIR)/eggrt/eggrt_romsrc% $(macos_MIDDIR)/eggrt/eggrt_exec%,$(macos_OFILES))
macos_OFILES_CONDITIONAL:=$(addprefix $(macos_MIDDIR)/eggrt/, \
  eggrt_romsrc_embedded.o \
  eggrt_romsrc_external.o \
  eggrt_exec_native.o \
  eggrt_exec_wasm.o \
  eggrt_exec_recom.o \
)

# eggrt's ofiles are a little weird because we have these two files that get compiled with a special flag.
# That controls (romsrc) whether the ROM is embedded or provided from a file, and (exec) whether we're running native code or WebAssembly.
# There's also two C files we borrow from WABT and bake into libegg-recom.a.
-include $(macos_OFILES:.o=.d) $(macos_OFILES_CONDITIONAL:.o=.d)
$(macos_MIDDIR)/%.o:src/%.c;$(PRECMD) $(macos_CC) -o$@ $<
$(macos_MIDDIR)/%.o:src/%.m;$(PRECMD) $(macos_OBJC) -o$@ $<
$(macos_MIDDIR)/eggrt/eggrt_romsrc_embedded.o:src/eggrt/eggrt_romsrc.c;$(PRECMD) $(macos_CC) -o$@ $< -DROMSRC=EMBEDDED
$(macos_MIDDIR)/eggrt/eggrt_romsrc_external.o:src/eggrt/eggrt_romsrc.c;$(PRECMD) $(macos_CC) -o$@ $< -DROMSRC=EXTERNAL
$(macos_MIDDIR)/eggrt/eggrt_exec_native.o:src/eggrt/eggrt_exec.c;$(PRECMD) $(macos_CC) -o$@ $< -DEXECFMT=NATIVE
$(macos_MIDDIR)/eggrt/eggrt_exec_wasm.o:src/eggrt/eggrt_exec.c;$(PRECMD) $(macos_CC) -o$@ $< -DEXECFMT=WASM -I$(WAMR_SDK)/core/iwasm/include
$(macos_MIDDIR)/eggrt/eggrt_exec_recom.o:src/eggrt/eggrt_exec.c;$(PRECMD) $(macos_CC) -o$@ $< -DEXECFMT=RECOM -I$(WABT_SDK)/wasm2c -I$(WABT_SDK)/include
$(macos_MIDDIR)/eggrt/%.o:$(WABT_SDK)/wasm2c/%.c;$(PRECMD) $(macos_CC) -o$@ $< -I$(WABT_SDK)/wasm2c -I$(WABT_SDK)/third_party/wasm-c-api/include
$(macos_MIDDIR)/eggrt/%.o:$(WABT_SDK)/share/wabt/wasm2c/%.c;$(PRECMD) $(macos_CC) -o$@ $< -I$(WABT_SDK)/include -I$(WABT_SDK)/third_party/wasm-c-api/include

macos_LIB_TRUE:=$(macos_OUTDIR)/libegg-true.a
macos_OFILES_LIB_TRUE:=$(macos_OFILES) \
  $(macos_MIDDIR)/eggrt/eggrt_romsrc_embedded.o \
  $(macos_MIDDIR)/eggrt/eggrt_exec_native.o
$(macos_LIB_TRUE):$(macos_OFILES_LIB_TRUE);$(PRECMD) $(macos_AR) rc $@ $^
macos-all:$(macos_LIB_TRUE)

ifneq (,$(strip $(WAMR_SDK)))
  macos_LIB_FAKE:=$(macos_OUTDIR)/libegg-fake.a
  macos_OFILES_LIB_FAKE:=$(macos_OFILES) \
    $(macos_MIDDIR)/eggrt/eggrt_romsrc_embedded.o \
    $(macos_MIDDIR)/eggrt/eggrt_exec_wasm.o
  $(macos_LIB_FAKE):$(macos_OFILES_LIB_FAKE);$(PRECMD) $(macos_AR) rc $@ $^
  macos-all:$(macos_LIB_FAKE)
endif

ifneq (,$(strip $(WABT_SDK)))
  macos_LIB_RECOM:=$(macos_OUTDIR)/libegg-recom.a
  macos_OFILES_LIB_RECOM:=$(macos_OFILES) \
    $(macos_MIDDIR)/eggrt/eggrt_romsrc_embedded.o \
    $(macos_MIDDIR)/eggrt/eggrt_exec_recom.o \
    $(macos_MIDDIR)/eggrt/wasm-rt-impl.o \
    $(macos_MIDDIR)/eggrt/wasm-rt-mem-impl.o
  $(macos_LIB_RECOM):$(macos_OFILES_LIB_RECOM);$(PRECMD) $(macos_AR) rc $@ $^
  macos-all:$(macos_LIB_RECOM)
endif

ifneq (,$(strip $(WAMR_SDK)))
  macos_EXE:=$(macos_OUTDIR)/egg
  macos_OFILES_EXE:=$(macos_OFILES) \
    $(macos_MIDDIR)/eggrt/eggrt_romsrc_external.o \
    $(macos_MIDDIR)/eggrt/eggrt_exec_wasm.o
  $(macos_EXE):$(macos_OFILES_EXE);$(PRECMD) $(macos_LD) -o$@ $^ $(macos_LDPOST) $(WAMR_SDK)/build/libvmlib.a
  macos-all:$(macos_EXE)
endif

macos-run:$(demo_BUNDLE_BITS);open -W $(demo_BUNDLE) --args --reopen-tty=$(shell tty)
