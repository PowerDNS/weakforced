MANPAGES_TARGET = wforce.1 wforce.conf.5
APIDOC_TARGET = wforce_api.7
SWAG2MD_PROG = swag2md.pl

EXTRA_DIST = manpages swagger $(SWAG2MD_PROG)

.PHONY: all-manpages

all-manpages: $(MANPAGES_TARGET) $(APIDOC_TARGET)

if HAVE_PANDOC
dist_man_MANS = $(MANPAGES_TARGET) $(APIDOC_TARGET)

$(MANPAGES_TARGET): %: manpages/%.md
	$(PANDOC) -s -t man $< -o $@

$(APIDOC_TARGET): %: swagger/%.yml
	$(PERL) $(SWAG2MD_PROG) $< >$@.amd
	$(PANDOC) -s -t man $@.amd -o $@
endif

clean:
	rm -rf *.1 *.5 *.7 *.amd