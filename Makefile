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
test         := thrdpooltest

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
CPPFLAGS     := -I$(root) -I$(unitydir)/src -D_POSIX_C_SOURCE=200112l
LDFLAGS      := -L$(unitydir) -L$(root)
LDLIBS       := -pthread
ARFLAGS      := -rc

so_LDFLAGS   := -shared -Wl,-soname,$(soname).$(socompat)
test_LDLIBS  := $(unityarchive) $(archive)

LNFLAGS      := -sf
MKDIRFLAGS   := -p
RMFLAGS      := -rf

QUIET        := @

obj          := $(patsubst $(srcdir)/%.$(cext),$(builddir)/%.$(oext),$(wildcard $(srcdir)/*.$(cext)))
testobj      := $(patsubst $(testdir)/%.$(cext),$(testbuilddir)/%.$(oext),$(wildcard $(testdir)/*.$(cext))) \
                $(patsubst $(testdir)/%.$(cext),$(testbuilddir)/%$(runsuffix).$(oext),$(wildcard $(testdir)/*.$(cext)))

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

$(testbuilddir)/%.$(oext): $(testdir)/%.$(cext) | $(testbuilddir)
	$(info [CC] $(notdir $@))
	$(QUIET)$(CC) -o $@ $< $(CFLAGS) $(CPPFLAGS)

$(testbuilddir)/%.$(oext): $(gendir)/%.$(cext) | $(testbuilddir)
	$(info [CC] $(notdir $@))
	$(QUIET)$(CC) -o $@ $< $(CFLAGS) $(CPPFLAGS)

$(gendir)/%$(runsuffix).$(cext): $(testdir)/%.$(cext) | $(gendir)
	$(info [RB] $(notdir $@))
	$(QUIET)$(RUBY) $(rbgen) $^ $@

$(unityarchive):
	$(QUIET)$(CMAKE) $(unitydir)
	$(QIUET)$(MAKE) -C $(unitydir)

$(test): $(testobj) $(archive) $(unityarchive)
	$(info [LD] $@)
	$(QUIET)$(CC) -o $@ $(testobj) $(LDFLAGS) $(LDLIBS) $(test_LDLIBS)

.PHONY: test
test: $(test)

.PHONY: check
check: $(test)
	$(QUIET)./$^

$(builddir):
	$(QUIET)$(MKDIR) $(MKDIRFLAGS) $@

$(testbuilddir):
	$(QUIET)$(MKDIR) $(MKDIRFLAGS) $@

$(gendir):
	$(QUIET)$(MKDIR) $(MKDIRFLAGS) $@

.PHONY: clean
clean:
	$(QUIET)$(RM) $(RMFLAGS) $(builddir) $(solib) $(solib).$(sover) $(test) $(archive)

-include $(obj:$(oext):.$(dext))
