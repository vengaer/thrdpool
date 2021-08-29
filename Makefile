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

builddir     := $(root)/build
gendir       := $(builddir)/gen
unitbuilddir := $(builddir)/test

unitydir     := $(root)/unity
unityarchive := $(unitydir)/libunity.a
runsuffix    := _runner
rbgen        := $(unitydir)/auto/generate_test_runner.rb

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

.PHONY: test
test: CFLAGS    += -fsanitize=thread,undefined -g
test: CPPFLAGS  := $(filter-out -DNDEBUG,$(CPPFLAGS)) -D_GNU_SOURCE
test: LDFLAGS   += -fsanitize=thread,undefined

.PHONY: check
check: CFLAGS   += -fsanitize=thread,undefined -g
check: CPPFLAGS := $(filter-out -DNDEBUG,$(CPPFLAGS)) -D_GNU_SOURCE
check: LDFLAGS  += -fsanitize=thread,undefined

$(builddir):
	$(QUIET)$(MKDIR) $(MKDIRFLAGS) $@

$(unitbuilddir):
	$(QUIET)$(MKDIR) $(MKDIRFLAGS) $@

$(gendir):
	$(QUIET)$(MKDIR) $(MKDIRFLAGS) $@

.PHONY: clean
clean:
	$(QUIET)$(RM) $(RMFLAGS) $(builddir) $(solib) $(solib).$(sover) $(archive)

.PHONY: distclean
distclean: clean
	$(QUIET)$(MAKE) -sC $(unitydir) clean

-include $(patsubst %.$(oext),%.$(dext),$(obj) $(testobj))
