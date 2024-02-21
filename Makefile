all:
.SILENT:
.SECONDARY:
PRECMD=echo "  $@" ; mkdir -p $(@D) ;

# Easy things we can do without loading configuration or examining source tree.
ifneq (,$(strip $(filter clean help,$(MAKECMDGOALS))))

clean:;rm -rf mid out

help:; \
  echo "" ; \
  echo "Egg Makefile Help" ; \
  echo "  make [all]       Build everything." ; \
  echo "  make help        Print this message." ; \
  echo "  make clean       Remove output and intermediaries." ; \
  echo "" ; \
  echo "Build is influenced by etc/config.mk, which gets created the first time. Edit to taste." ; \
  echo ""

# Real build...
else

-include etc/config.mk
etc/config.mk:|etc/config.mk.default;etc/tool/genconfig.sh $@ etc/config.mk.default

include etc/make/common.mk
include etc/make/tools.mk

$(foreach T,$(TARGETS), \
  $(eval include etc/target/$T.mk) \
  $(eval all:$T-all) \
)

endif
