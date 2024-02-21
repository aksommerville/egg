# tools.mk
# Rules for building tools that can be run at compile time.

tools_MIDDIR:=mid/tools
tools_OUTDIR:=out/tools

tools_CC:=gcc -c -MMD -O3 -Isrc -Werror -Wimplicit
tools_LD:=gcc
tools_LDPOST:=-lz

#TODO Specific opt unit selection.
tools_CFILES:=$(filter \
  src/tool/%.c \
  src/opt/arr/%.c \
  src/opt/arw/%.c \
  src/opt/fs/%.c \
  src/opt/serial/%.c \
  src/opt/png/%.c \
  src/opt/qoi/%.c \
  src/opt/ico/%.c \
  src/opt/rlead/%.c \
  ,$(SRCFILES) \
)
tools_OFILES:=$(patsubst src/%.c,$(tools_MIDDIR)/%.o,$(tools_CFILES))
tools_OFILES_COMMON:=$(filter $(tools_MIDDIR)/tool/common/%,$(tools_OFILES)) $(filter-out $(tools_MIDDIR)/tool/%,$(tools_OFILES))

$(tools_MIDDIR)/%.o:src/%.c;$(PRECMD) $(tools_CC) -o$@ $<

tools_TOOLS:=$(notdir $(wildcard src/tool/*))
define tools_RULES
  tools_EXE_$1:=$(tools_OUTDIR)/$1
  tools_OFILES_$1:=$(tools_OFILES_COMMON) $(filter $(tools_MIDDIR)/tool/$1/%,$(tools_OFILES))
  $$(tools_EXE_$1):$$(tools_OFILES_$1);$$(PRECMD) $(tools_LD) -o$$@ $$(tools_OFILES_$1) $(tools_LDPOST)
  all:$$(tools_EXE_$1)
endef
$(foreach T,$(tools_TOOLS),$(eval $(call tools_RULES,$T)))

