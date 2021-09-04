FUZZTIME          ?= 240
FUZZLEN           ?= 4096
FUZZVALPROF       ?= 1
FUZZTIMEO         ?= 10
FUZZCORPORA        = $(fuzzdir)/corpora
FUZZFLAGS          = -max_len=$(FUZZLEN) -max_total_time=$(FUZZTIME) -use_value_profile=$(FUZZVALPROF) \
                     -timeout=$(FUZZTIMEO) $(FUZZCORPORA)
FUZZMERGEFLAGS     = -merge=1 $(FUZZCORPORA) $(CORPORA)

LLVM_PROFILE_FILE  = $(builddir)/.fuzz.profraw
PROFDATA           = $(builddir)/.fuzz.profdata
PROFFLAGS          = merge -sparse $(LLVM_PROFILE_FILE) -o $(PROFDATA)

COVFLAGS           = show $(fuzzgen) -instr-profile=$(PROFDATA)
COVREPFLAGS        = report $(fuzzgen) -instr-profile=$(PROFDATA)

define require-corpora
$(if $(findstring -_-$(MAKECMDGOALS)-_-,-_-$(1)-_-),$(if $(CORPORA),,$(error CORPORA is empty)))
endef
