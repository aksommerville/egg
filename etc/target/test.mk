# test.mk
# Unit, integration, and automation tests against eggdev.
# I'm not thinking about test scaffolding for the client yet, but we ought to provide some support there in the future.

test_MIDDIR:=mid/test
test_OUTDIR:=out/test

test_CC:=$(eggdev_CC) -I$(test_MIDDIR)
test_LD:=$(eggdev_LD)
test_LDPOST:=$(eggdev_LDPOST)

test_CFILES_COMMON:=$(filter src/test/common/%.c,$(SRCFILES))
test_CFILES_UTEST:=$(filter src/test/unit/%.c,$(SRCFILES))
test_CFILES_ITEST:=$(filter src/test/int/%.c,$(SRCFILES))
test_EXES_ATEST:=$(filter src/test/auto/%,$(SRCFILES))

test_ITEST_TOC:=$(test_MIDDIR)/egg_itest_toc.h
$(test_ITEST_TOC):$(test_CFILES_ITEST);$(PRECMD) etc/tool/genitesttoc.sh $@ $^

test_OFILES_EGGDEV:=$(filter-out $(eggdev_MIDDIR)/eggdev/eggdev_main.o,$(eggdev_OFILES))
test_OFILES_COMMON:=$(patsubst src/test/%.c,$(test_MIDDIR)/%.o,$(test_CFILES_COMMON))
test_OFILES_UTEST:=$(patsubst src/test/%.c,$(test_MIDDIR)/%.o,$(test_CFILES_UTEST))
test_OFILES_ITEST:=$(patsubst src/test/%.c,$(test_MIDDIR)/%.o,$(test_CFILES_ITEST))
$(test_MIDDIR)/%.o:src/test/%.c|$(test_ITEST_TOC);$(PRECMD) $(test_CC) -o$@ $<
-include $(patsubst %.o,%.d,$(test_OFILES_COMMON) $(test_OFILES_UTEST) $(test_OFILES_ITEST))

test_EXES_UTEST:=$(patsubst $(test_MIDDIR)/%.o,$(test_OUTDIR)/%,$(test_OFILES_UTEST))
test-all:$(test_EXES_UTEST)
$(test_OUTDIR)/unit/%:$(test_MIDDIR)/unit/%.o $(test_OFILES_COMMON);$(PRECMD) $(test_LD) -o$@ $^ $(test_LDPOST)

test_EXE_ITEST:=$(test_OUTDIR)/itest
test-all:$(test_EXE_ITEST)
$(test_EXE_ITEST):$(test_OFILES_ITEST) $(test_OFILES_COMMON) $(test_OFILES_EGGDEV);$(PRECMD) $(test_LD) -o$@ $^ $(test_LDPOST)

test_EXES:=$(test_EXES_UTEST) $(test_EXE_ITEST) $(test_EXES_ATEST)
test:$(test_EXES);etc/tool/testrunner.sh $^
test-%:$(test_EXES);EGG_TEST_FILTER="$*" etc/tool/testrunner.sh $^
