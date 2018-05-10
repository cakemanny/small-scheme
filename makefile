SHELL=/bin/bash

TOPTARGETS := all clean

SUBDIRS := ast-lang/. lambda-calculus/. reader/.

$(TOPTARGETS): $(SUBDIRS)

$(SUBDIRS):
	$(MAKE) -C $@ $(MAKECMDGOALS)

.PHONY: $(TOPTARGETS) $(SUBDIRS)

