mswin_MIDDIR:=mid/mswin
mswin_OUTDIR:=out/mswin

mswin_OPT_ENABLE+=serial fs rom image hostio render synth
mswin_CC+=$(foreach U,$(mswin_OPT_ENABLE),-DUSE_$U=1)

mswin_CFILES:=$(filter \
  src/eggrt/% \
  $(addprefix src/opt/,$(addsuffix /%,$(mswin_OPT_ENABLE))) \
,$(SRCFILES))

mswin_OFILES:=$(patsubst src/%.c,$(mswin_MIDDIR)/%.o,$(filter %.c,$(mswin_CFILES)))
mswin_OFILES:=$(filter-out $(mswin_MIDDIR)/eggrt/eggrt_romsrc% $(mswin_MIDDIR)/eggrt/eggrt_exec%,$(mswin_OFILES))
mswin_OFILES_CONDITIONAL:=$(addprefix $(mswin_MIDDIR)/eggrt/, \
  eggrt_romsrc_embedded.o \
  eggrt_romsrc_external.o \
  eggrt_exec_native.o \
  eggrt_exec_wasm.o \
  eggrt_exec_recom.o \
)

# eggrt's ofiles are a little weird because we have these two files that get compiled with a special flag.
# That controls (romsrc) whether the ROM is embedded or provided from a file, and (exec) whether we're running native code or WebAssembly.
# There's also two C files we borrow from WABT and bake into libegg-recom.a.
-include $(mswin_OFILES:.o=.d) $(mswin_OFILES_CONDITIONAL:.o=.d)
$(mswin_MIDDIR)/%.o:src/%.c;$(PRECMD) $(mswin_CC) -o$@ $<
$(mswin_MIDDIR)/eggrt/eggrt_romsrc_embedded.o:src/eggrt/eggrt_romsrc.c;$(PRECMD) $(mswin_CC) -o$@ $< -DROMSRC=EMBEDDED
$(mswin_MIDDIR)/eggrt/eggrt_romsrc_external.o:src/eggrt/eggrt_romsrc.c;$(PRECMD) $(mswin_CC) -o$@ $< -DROMSRC=EXTERNAL
$(mswin_MIDDIR)/eggrt/eggrt_exec_native.o:src/eggrt/eggrt_exec.c;$(PRECMD) $(mswin_CC) -o$@ $< -DEXECFMT=NATIVE
$(mswin_MIDDIR)/eggrt/eggrt_exec_wasm.o:src/eggrt/eggrt_exec.c;$(PRECMD) $(mswin_CC) -o$@ $< -DEXECFMT=WASM -I$(WAMR_SDK)/core/iwasm/include
$(mswin_MIDDIR)/eggrt/eggrt_exec_recom.o:src/eggrt/eggrt_exec.c;$(PRECMD) $(mswin_CC) -o$@ $< -DEXECFMT=RECOM -I$(WABT_SDK)/wasm2c
$(mswin_MIDDIR)/eggrt/%.o:$(WABT_SDK)/wasm2c/%.c;$(PRECMD) $(mswin_CC) -o$@ $< -I$(WABT_SDK)/wasm2c -I$(WABT_SDK)/third_party/wasm-c-api/include

# libegg-true is always an option, no reason not to build it.
mswin_LIB_TRUE:=$(mswin_OUTDIR)/libegg-true.a
mswin_OFILES_LIB_TRUE:=$(mswin_OFILES) \
  $(mswin_MIDDIR)/eggrt/eggrt_romsrc_embedded.o \
  $(mswin_MIDDIR)/eggrt/eggrt_exec_native.o
$(mswin_LIB_TRUE):$(mswin_OFILES_LIB_TRUE);$(PRECMD) $(mswin_AR) rc $@ $^
mswin-all:$(mswin_LIB_TRUE)

# libegg-fake only if we have WAMR.
ifneq (,$(strip $(WAMR_SDK)))
  mswin_LIB_FAKE:=$(mswin_OUTDIR)/libegg-fake.a
  mswin_OFILES_LIB_FAKE:=$(mswin_OFILES) \
    $(mswin_MIDDIR)/eggrt/eggrt_romsrc_embedded.o \
    $(mswin_MIDDIR)/eggrt/eggrt_exec_wasm.o
  $(mswin_LIB_FAKE):$(mswin_OFILES_LIB_FAKE);$(PRECMD) $(mswin_AR) rc $@ $^
  mswin-all:$(mswin_LIB_FAKE)
endif

# libegg-recom only if we have WABT
ifneq (,$(strip $(WABT_SDK)))
  mswin_LIB_RECOM:=$(mswin_OUTDIR)/libegg-recom.a
  mswin_OFILES_LIB_RECOM:=$(mswin_OFILES) \
    $(mswin_MIDDIR)/eggrt/eggrt_romsrc_embedded.o \
    $(mswin_MIDDIR)/eggrt/eggrt_exec_recom.o \
    $(mswin_MIDDIR)/eggrt/wasm-rt-impl.o \
    $(mswin_MIDDIR)/eggrt/wasm-rt-mem-impl.o
  $(mswin_LIB_RECOM):$(mswin_OFILES_LIB_RECOM);$(PRECMD) $(mswin_AR) rc $@ $^
  mswin-all:$(mswin_LIB_RECOM)
endif

# ROMless executable only if we have WAMR.
ifneq (,$(strip $(WAMR_SDK)))
  mswin_EXE:=$(mswin_OUTDIR)/egg
  mswin_OFILES_EXE:=$(mswin_OFILES) \
    $(mswin_MIDDIR)/eggrt/eggrt_romsrc_external.o \
    $(mswin_MIDDIR)/eggrt/eggrt_exec_wasm.o
  $(mswin_EXE):$(mswin_OFILES_EXE);$(PRECMD) $(mswin_LD) -o$@ $^ $(mswin_LDPOST) $(WAMR_SDK)/build/libvmlib.a
  mswin-all:$(mswin_EXE)
endif

mswin-run:$(mswin_EXE) $(demo_ROM);$(mswin_EXE) $(demo_ROM) $(mswin_RUN_ARGS)
