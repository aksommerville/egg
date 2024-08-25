all:
.SILENT:
.SECONDARY:
PRECMD=echo "  $@" ; mkdir -p $(@D) ;

ifneq (,$(strip $(filter clean,$(MAKECMDGOALS))))

clean:;rm -rf mid out

else

include local/config.mk
local/config.mk:;$(PRECMD) etc/tool/configure.sh $@

SRCFILES:=$(shell find src -type f)

$(foreach T,$(TARGETS),$(eval include etc/target/$T.mk))
all:$(addsuffix -all,$(TARGETS))

endif
