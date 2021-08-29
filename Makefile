include scripts/fuzz.mk

libname      := libthrdpool

cext         := c
oext         := o
aext         := a
soext        := so
dext         := d

archive      := $(libname).$(aext)
solib        := $(libname).$(soext)
sover        := 0
socompat     := 0

root         := $(abspath $(CURDIR))
srcdir       := $(root)/src
testdir      := $(root)/test
unitdir      := $(testdir)/unit
fuzzdir      := $(testdir)/fuzz

builddir     := $(root)/build
gendir       := $(builddir)/gen
unitbuilddir := $(builddir)/test
fuzzbuilddir := $(builddir)/fuzz

unitydir     := $(root)/unity
unityarchive := $(unitydir)/libunity.a
runsuffix    := _runner
rbgen        := $(unitydir)/auto/generate_test_runner.rb

fuzzbin      := $(builddir)/fuzzer
fuzzgen      := $(builddir)/fuzzgen
fuzzmerger   := $(builddir)/fuzzmerger

CC           := gcc
AR           := ar
LN           := ln
MKDIR        := mkdir
RM           := rm
RUBY         := ruby
CMAKE        := cmake

O            := 1
CFLAGS       := -Wall -Wextra -Wpedantic -std=c99 -g -fPIC -MD -MP -c -pthread -O$(O)
CPPFLAGS     := -I$(root) -I$(unitydir)/src -DNDEBUG
LDFLAGS      := -L$(unitydir) -L$(root)
LDLIBS       := -pthread
ARFLAGS      := -rc

so_LDFLAGS   := -shared -Wl,-soname,$(soname).$(socompat)

LNFLAGS      := -sf
MKDIRFLAGS   := -p
RMFLAGS      := -rf

QUIET        := @

obj          := $(patsubst $(srcdir)/%.$(cext),$(builddir)/%.$(oext),$(wildcard $(srcdir)/*.$(cext)))
testobj      := $(patsubst $(unitdir)/%.$(cext),$(unitbuilddir)/%.$(oext),$(wildcard $(unitdir)/*.$(cext)))
fuzzbinobj   := $(patsubst $(fuzzdir)/%.$(cext),$(fuzzbuilddir)/%.$(oext),$(fuzzdir)/main.$(cext)) \
                $(patsubst $(srcdir)/%.$(cext),$(fuzzbuilddir)/%.$(oext),$(wildcard $(srcdir)/*.$(cext)))
fuzzgenobj   := $(patsubst $(fuzzdir)/%.$(cext),$(fuzzbuilddir)/%.$(oext),$(fuzzdir)/fuzzer.$(cext))
fuzzmergeobj := $(patsubst $(fuzzdir)/%.$(cext),$(fuzzbuilddir)/%.$(oext),$(fuzzdir)/merger.$(cext))

define gen-test-link-rules
$(strip
    $(foreach __o,$(testobj),
        $(eval
            $(eval __bin := $(builddir)/$(basename $(notdir $(__o))))
            $(__bin): $(__o) $(patsubst %.$(oext),%_runner.$(oext),$(__o)) $(archive) $(unityarchive) | $(builddir)
	            $$(info [LD] $$(notdir $$@))
	            $(QUIET)$(CC) -o $$@ $$^ $$(LDFLAGS) $$(LDLIBS)

            check_$(notdir $(__bin)): $(__bin)
	            $(QUIET)$$^

            test: $(__bin)
            check: check_$(notdir $(__bin)))))
endef

define find-fuzzsrc
$(strip
    $(if $(findstring $(patsubst $(fuzzbuilddir)/%.$(oext),$(srcdir)/%.$(cext),$(1)),$(wildcard $(srcdir)/*.$(cext))),
        $(patsubst $(fuzzbuilddir)/%.$(oext),$(srcdir)/%.$(cext),$(1)),
      $(patsubst $(fuzzbuilddir)/%.$(oext),$(fuzzdir)/%.$(cext),$(1))))
endef

define gen-fuzz-build-rules
$(strip
    $(foreach __o,$(fuzzbinobj),
        $(eval
                $(__o): $(call find-fuzzsrc,$(__o)) | $(fuzzbuilddir)
	            $$(info [CC] $$(notdir $$@))
	            $(QUIET)$$(CC) -o $$@ $$^ $$(CFLAGS) $$(CPPFLAGS) -fsanitize=thread,undefined))

    $(foreach __o,$(fuzzgenobj),
        $(eval
            $(__o): $(call find-fuzzsrc,$(__o)) | $(fuzzbuilddir)
	            $$(info [CC] $$(notdir $$@))
	            $(QUIET)$$(CC) -o $$@ $$^ $$(CFLAGS) $$(CPPFLAGS) -fsanitize=fuzzer,address,undefined))

    $(foreach __o,$(fuzzmergeobj),
        $(eval
            $(__o): $(call find-fuzzsrc,$(__o)) | $(fuzzbuilddir)
	            $$(info [CC] $$(notdir $$@))
	            $(QUIET)$$(CC) -o $$@ $$^ $$(CFLAGS) $$(CPPFLAGS) -fsanitize=fuzzer,address,undefined)))
endef

.PHONY: all
all: $(archive) $(solib)

$(archive): $(obj)
	$(info [AR] $@)
	$(QUIET)$(AR) $(ARFLAGS) -o $@ $^

$(solib): $(solib).$(sover)
	$(info [LN] $@)
	$(QUIET)$(LN) $(LNFLAGS) $(abspath $(CURDIR))/$^ $@

$(solib).$(sover): $(obj)
	$(info [LN] $@)
	$(QUIET)$(CC) -o $@ $^ $(LDFLAGS) $(so_LDFLAGS) $(LDLIBS)

$(builddir)/%.$(oext): $(srcdir)/%.$(cext) | $(builddir)
	$(info [CC] $(notdir $@))
	$(QUIET)$(CC) -o $@ $< $(CFLAGS) $(CPPFLAGS)

$(unitbuilddir)/%.$(oext): $(unitdir)/%.$(cext) $(unityarchive) | $(unitbuilddir)
	$(info [CC] $(notdir $@))
	$(QUIET)$(CC) -o $@ $< $(CFLAGS) $(CPPFLAGS)

$(unitbuilddir)/%.$(oext): $(gendir)/%.$(cext) $(unityarchive) | $(unitbuilddir)
	$(info [CC] $(notdir $@))
	$(QUIET)$(CC) -o $@ $< $(CFLAGS) $(CPPFLAGS)

$(gendir)/%$(runsuffix).$(cext): $(unitdir)/%.$(cext) $(unityarchive) | $(gendir)
	$(info [RB] $(notdir $@))
	$(QUIET)$(RUBY) $(rbgen) $< $@

$(unityarchive):
	$(QUIET)git submodule update --init
	$(QUIET)$(CMAKE) -B $(unitydir) $(unitydir)
	$(QIUET)$(MAKE) -C $(unitydir)

$(call gen-test-link-rules)

$(call gen-fuzz-build-rules)

$(fuzzbin): $(fuzzbinobj)
	$(info [LD] $(notdir $@))
	$(QUIET)$(CC) -o $@ $^ $(LDFLAGS) $(LDLIBS) -fsanitize=thread,undefined

$(fuzzgen): $(fuzzgenobj)
	$(info [LD] $(notdir $@))
	$(QUIET)$(CC) -o $@ $^ $(LDFLAGS) $(LDLIBS) -fsanitize=fuzzer,address,undefined

$(fuzzmerger): $(fuzzmergeobj)
	$(info [LD] $(notdir $@))
	$(QUIET)$(CC) -o $@ $^ $(LDFLAGS) $(LDLIBS) -fsanitize=fuzzer,address,undefined

.PHONY: test
test: CFLAGS         += -fsanitize=thread,undefined -g
test: CPPFLAGS       := $(filter-out -DNDEBUG,$(CPPFLAGS)) -D_GNU_SOURCE
test: LDFLAGS        += -fsanitize=thread,undefined

.PHONY: check
check: CFLAGS        += -fsanitize=thread,undefined -g
check: CPPFLAGS      := $(filter-out -DNDEBUG,$(CPPFLAGS)) -D_GNU_SOURCE
check: LDFLAGS       += -fsanitize=thread,undefined

.PHONY: fuzz
fuzz: CC             := clang
fuzz: CFLAGS         += -g
fuzz: CPPFLAGS       := $(filter-out -DNDEBUG,$(CPPFLAGS)) -D_GNU_SOURCE
fuzz: CPPFLAGS       +=-DFUZZ_MAXLEN=$(FUZZLEN) -DFUZZER_GENPATH=$(fuzzgen)
fuzz: LDLIBS         += -lrt
fuzz: $(fuzzbin) $(fuzzgen)

.PHONY: fuzzrun
fuzzrun: CC          := clang
fuzzrun: CFLAGS      += -g
fuzzrun: CPPFLAGS    := $(filter-out -DNDEBUG,$(CPPFLAGS)) -D_GNU_SOURCE
fuzzrun: CPPFLAGS    +=-DFUZZ_MAXLEN=$(FUZZLEN) -DFUZZER_GENPATH=$(fuzzgen)
fuzzrun: LDLIBS      += -lrt
fuzzrun: $(fuzzbin) $(fuzzgen)
	$(QUIET)$< $(FUZZFLAGS)

.PHONY: fuzzmerge
fuzzmerge: $(call require-corpora,fuzzmerge)
fuzzmerge: CC       := clang
fuzzmerge: CFLAGS   += -fsanitize=fuzzer,address,undefined -g
fuzzmerge: CPPFLAGS := $(filter-out -DNDEBUG,$(CPPFLAGS)) -D_GNU_SOURCE
fuzzmerge: CPPFLAGS += -DFUZZ_MAXLEN=$(FUZZLEN) -DFUZZER_GENPATH=$(fuzzgen)
fuzzmerge: LDFLAGS  += -fsanitize=fuzzer,address,undefined
fuzzmerge: $(fuzzmerger)
	$(QUIET)$^ $(FUZZMERGEFLAGS)

$(builddir):
	$(QUIET)$(MKDIR) $(MKDIRFLAGS) $@

$(unitbuilddir):
	$(QUIET)$(MKDIR) $(MKDIRFLAGS) $@

$(gendir):
	$(QUIET)$(MKDIR) $(MKDIRFLAGS) $@

$(fuzzbuilddir):
	$(QUIET)$(MKDIR) $(MKDIRFLAGS) $@

.PHONY: clean
clean:
	$(QUIET)$(RM) $(RMFLAGS) $(builddir) $(solib) $(solib).$(sover) $(archive)

.PHONY: distclean
distclean: clean
	$(QUIET)$(MAKE) -sC $(unitydir) clean

-include $(patsubst %.$(oext),%.$(dext),$(obj) $(testobj))
