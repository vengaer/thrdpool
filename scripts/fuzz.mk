FUZZTIME          ?= 240
FUZZLEN           ?= 4096
FUZZTIMEO         ?= 10
FUZZCORPORA        = $(fuzzdir)/corpora
FUZZFLAGS          = -max_len=$(FUZZLEN) -max_total_time=$(FUZZTIME) -timeout=$(FUZZTIMEO) $(FUZZCORPORA)
FUZZMERGEFLAGS     = -merge=1 $(FUZZCORPORA) $(CORPORA)

define require-corpora
$(if $(findstring -_-$(MAKECMDGOALS)-_-,-_-$(1)-_-),$(if $(CORPORA),,$(error CORPORA is empty)))
endef