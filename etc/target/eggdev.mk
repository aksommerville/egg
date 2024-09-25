eggdev_MIDDIR:=mid/eggdev
eggdev_OUTDIR:=out
eggdev_EXE:=$(eggdev_OUTDIR)/eggdev

eggdev_OPT_ENABLE+=serial fs rom image midi http synth
eggdev_CC+=$(foreach U,$(eggdev_OPT_ENABLE),-DUSE_$U=1)
eggdev_LDPOST+=-lz -lm

eggdev_SRCFILES:=$(filter \
  src/eggdev/% \
  $(addprefix src/opt/,$(addsuffix /%,$(eggdev_OPT_ENABLE))) \
,$(SRCFILES))
eggdev_CFILES:=$(filter %.c,$(eggdev_SRCFILES))
eggdev_OFILES:=$(patsubst src/%.c,$(eggdev_MIDDIR)/%.o,$(eggdev_CFILES))
-include $(eggdev_OFILES:.o=.d)

$(eggdev_MIDDIR)/%.o:src/%.c;$(PRECMD) $(eggdev_CC) -o$@ $<
$(eggdev_MIDDIR)/eggdev/eggdev_buildcfg.o:src/eggdev/eggdev_buildcfg.c;$(PRECMD) $(eggdev_CC) -o$@ $< \
  -DEGGDEV_CC="\"$($(NATIVE_TARGET)_CC)\"" \
  -DEGGDEV_AS="\"$($(NATIVE_TARGET)_AS)\"" \
  -DEGGDEV_LD="\"$($(NATIVE_TARGET)_LD)\"" \
  -DEGGDEV_LDPOST="\"$($(NATIVE_TARGET)_LDPOST)\"" \
  -DEGGDEV_WEB_CC="\"$(web_CC)\"" \
  -DEGGDEV_WEB_LD="\"$(web_LD)\"" \
  -DEGGDEV_WEB_LDPOST="\"$(web_LDPOST)\"" \
  -DEGGDEV_EGG_SDK="\"$(abspath $(CURDIR))\"" \
  -DEGGDEV_NATIVE_TARGET="\"$(NATIVE_TARGET)\"" \
  -DEGGDEV_WABT_SDK="\"$(abspath $(WABT_SDK))\"" \
  -DEGGDEV_WAMR_SDK="\"$(abspath $(WAMR_SDK))\""
  
# Libraries we will use at runtime. They don't actually need to exist for us to build, but it's a good time to check.
eggdev_RTLIBS:= \
  out/$(NATIVE_TARGET)/libegg-true.a \
  out/$(NATIVE_TARGET)/libegg-fake.a \
  out/$(NATIVE_TARGET)/libegg-recom.a

$(eggdev_EXE):$(eggdev_OFILES) $(eggdev_RTLIBS);$(PRECMD) $(eggdev_LD) -o$@ $^ $(eggdev_LDPOST)
eggdev-all:$(eggdev_EXE)

