linux_MIDDIR:=mid/linux
linux_OUTDIR:=out/linux

linux_OPT_ENABLE+=serial fs rom
linux_CC+=$(foreach U,$(linux_OPT_ENABLE),-DUSE_$U=1)
#TODO Update CC and LDPOST with anything needful in OPT_ENABLE

linux_CFILES:=$(filter \
  src/eggrt/% \
  $(addprefix src/opt/,$(addsuffix /%,$(linux_OPT_ENABLE))) \
,$(SRCFILES))

linux_OFILES:=$(patsubst src/%.c,$(linux_MIDDIR)/%.o,$(filter %.c,$(linux_CFILES)))
linux_OFILES:=$(filter-out $(linux_MIDDIR)/eggrt/eggrt_romsrc% $(linux_MIDDIR)/eggrt/eggrt_exec%,$(linux_OFILES))
linux_OFILES_CONDITIONAL:=$(addprefix $(linux_MIDDIR)/eggrt/, \
  eggrt_romsrc_embedded.o \
  eggrt_romsrc_external.o \
  eggrt_exec_native.o \
  eggrt_exec_wasm.o \
  eggrt_exec_recom.o \
)

# eggrt's ofiles are a little weird because we have these two files that get compiled with a special flag.
# That controls (romsrc) whether the ROM is embedded or provided from a file, and (exec) whether we're running native code or WebAssembly.
# There's also two C files we borrow from WABT and bake into libegg-recom.a.
-include $(linux_OFILES:.o=.d) $(linux_OFILES_CONDITIONAL:.o=.d)
$(linux_MIDDIR)/%.o:src/%.c;$(PRECMD) $(linux_CC) -o$@ $<
$(linux_MIDDIR)/eggrt/eggrt_romsrc_embedded.o:src/eggrt/eggrt_romsrc.c;$(PRECMD) $(linux_CC) -o$@ $< -DROMSRC=EMBEDDED
$(linux_MIDDIR)/eggrt/eggrt_romsrc_external.o:src/eggrt/eggrt_romsrc.c;$(PRECMD) $(linux_CC) -o$@ $< -DROMSRC=EXTERNAL
$(linux_MIDDIR)/eggrt/eggrt_exec_native.o:src/eggrt/eggrt_exec.c;$(PRECMD) $(linux_CC) -o$@ $< -DEXECFMT=NATIVE
$(linux_MIDDIR)/eggrt/eggrt_exec_wasm.o:src/eggrt/eggrt_exec.c;$(PRECMD) $(linux_CC) -o$@ $< -DEXECFMT=WASM -I$(WAMR_SDK)/core/iwasm/include
$(linux_MIDDIR)/eggrt/eggrt_exec_recom.o:src/eggrt/eggrt_exec.c;$(PRECMD) $(linux_CC) -o$@ $< -DEXECFMT=RECOM -I$(WABT_SDK)/wasm2c
$(linux_MIDDIR)/eggrt/%.o:$(WABT_SDK)/wasm2c/%.c;$(PRECMD) $(linux_CC) -o$@ $< -I$(WABT_SDK)/wasm2c -I$(WABT_SDK)/third_party/wasm-c-api/include

linux_LIB_TRUE:=$(linux_OUTDIR)/libegg-true.a
linux_OFILES_LIB_TRUE:=$(linux_OFILES) \
  $(linux_MIDDIR)/eggrt/eggrt_romsrc_embedded.o \
  $(linux_MIDDIR)/eggrt/eggrt_exec_native.o
$(linux_LIB_TRUE):$(linux_OFILES_LIB_TRUE);$(PRECMD) $(linux_AR) rc $@ $^
linux-all:$(linux_LIB_TRUE)

linux_LIB_FAKE:=$(linux_OUTDIR)/libegg-fake.a
linux_OFILES_LIB_FAKE:=$(linux_OFILES) \
  $(linux_MIDDIR)/eggrt/eggrt_romsrc_embedded.o \
  $(linux_MIDDIR)/eggrt/eggrt_exec_wasm.o
$(linux_LIB_FAKE):$(linux_OFILES_LIB_FAKE);$(PRECMD) $(linux_AR) rc $@ $^
linux-all:$(linux_LIB_FAKE)

linux_LIB_RECOM:=$(linux_OUTDIR)/libegg-recom.a
linux_OFILES_LIB_RECOM:=$(linux_OFILES) \
  $(linux_MIDDIR)/eggrt/eggrt_romsrc_embedded.o \
  $(linux_MIDDIR)/eggrt/eggrt_exec_recom.o \
  $(linux_MIDDIR)/eggrt/wasm-rt-impl.o \
  $(linux_MIDDIR)/eggrt/wasm-rt-mem-impl.o
$(linux_LIB_RECOM):$(linux_OFILES_LIB_RECOM);$(PRECMD) $(linux_AR) rc $@ $^
linux-all:$(linux_LIB_RECOM)

linux_EXE:=$(linux_OUTDIR)/egg
linux_OFILES_EXE:=$(linux_OFILES) \
  $(linux_MIDDIR)/eggrt/eggrt_romsrc_external.o \
  $(linux_MIDDIR)/eggrt/eggrt_exec_wasm.o
$(linux_EXE):$(linux_OFILES_EXE);$(PRECMD) $(linux_LD) -o$@ $^ $(linux_LDPOST) $(WAMR_SDK)/build/libvmlib.a
linux-all:$(linux_EXE)

linux-run:$(linux_EXE) $(demo_ROM);$(linux_EXE) $(demo_ROM) $(linux_RUN_ARGS)
