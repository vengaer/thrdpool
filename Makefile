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
builddir     := $(root)/build
gendir       := $(builddir)/gen
testbuilddir := $(builddir)/test
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

CFLAGS       := -Wall -Wextra -Wpedantic -std=c99 -g -fPIC -MD -MP -c -pthread
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
testobj      := $(patsubst $(testdir)/%.$(cext),$(testbuilddir)/%.$(oext),$(wildcard $(testdir)/*.$(cext)))

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

$(testbuilddir)/%.$(oext): $(testdir)/%.$(cext) $(unityarchive) | $(testbuilddir)
	$(info [CC] $(notdir $@))
	$(QUIET)$(CC) -o $@ $< $(CFLAGS) $(CPPFLAGS)

$(testbuilddir)/%.$(oext): $(gendir)/%.$(cext) $(unityarchive) | $(testbuilddir)
	$(info [CC] $(notdir $@))
	$(QUIET)$(CC) -o $@ $< $(CFLAGS) $(CPPFLAGS)

$(gendir)/%$(runsuffix).$(cext): $(testdir)/%.$(cext) | $(gendir)
	$(info [RB] $(notdir $@))
	$(QUIET)$(RUBY) $(rbgen) $^ $@

$(unityarchive):
	$(QUIET)git submodule update --init
	$(QUIET)$(CMAKE) -B $(unitydir) $(unitydir)
	$(QIUET)$(MAKE) -C $(unitydir)

$(call gen-test-link-rules)

.PHONY: test
test: CFLAGS    += -fsanitize=thread,undefined -g
test: CPPFLAGS  := $(filter-out -DNDEBUG,$(CPPFLAGS))
test: LDFLAGS   += -fsanitize=thread,undefined

.PHONY: check
check: CFLAGS   += -fsanitize=thread,undefined -g
check: CPPFLAGS := $(filter-out -DNDEBUG,$(CPPFLAGS))
check: LDFLAGS  += -fsanitize=thread,undefined

$(builddir):
	$(QUIET)$(MKDIR) $(MKDIRFLAGS) $@

$(testbuilddir):
	$(QUIET)$(MKDIR) $(MKDIRFLAGS) $@

$(gendir):
	$(QUIET)$(MKDIR) $(MKDIRFLAGS) $@

.PHONY: clean
clean:
	$(QUIET)$(RM) $(RMFLAGS) $(builddir) $(solib) $(solib).$(sover) $(archive)

.PHONY: distclean
distclean: clean
	$(QUIET)$(MAKE) -sC $(unitydir) clean

-include $(obj:$(oext):.$(dext))
