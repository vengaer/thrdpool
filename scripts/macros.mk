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
    $(if $(findstring $(patsubst $(1)/%.$(oext),$(srcdir)/%.$(cext),$(2)),$(wildcard $(srcdir)/*.$(cext))),
        $(patsubst $(1)/%.$(oext),$(srcdir)/%.$(cext),$(2)),
      $(patsubst $(1)/%.$(oext),$(fuzzdir)/%.$(cext),$(2))))
endef

define gen-fuzz-build-rules
$(strip
    $(foreach __o,$(fuzzbinobj),
        $(eval
            $(__o): $(call find-fuzzsrc,$(fuzzbindir),$(__o)) | $(fuzzbindir)
	            $$(info [CC] $$(notdir $$@))
	            $(QUIET)$$(CC) -o $$@ $$^ $$(CFLAGS) $$(CPPFLAGS) -fsanitize=thread,undefined))

    $(foreach __o,$(fuzzgenobj),
        $(eval
            $(__o): $(call find-fuzzsrc,$(fuzzgendir),$(__o)) | $(fuzzgendir)
	            $$(info [CC] $$(notdir $$@))
	            $(QUIET)$$(CC) -o $$@ $$^ $$(CFLAGS) $$(CPPFLAGS) -fsanitize=fuzzer,address,undefined -fprofile-instr-generate -fcoverage-mapping))

    $(foreach __o,$(fuzzmergeobj),
        $(eval
            $(__o): $(call find-fuzzsrc,$(fuzzbuilddir),$(__o)) | $(fuzzbuilddir)
	            $$(info [CC] $$(notdir $$@))
	            $(QUIET)$$(CC) -o $$@ $$^ $$(CFLAGS) $$(CPPFLAGS) -fsanitize=fuzzer,address,undefined)))
endef

