if HAVE_PANDOC

if WITH_TRACKALERT
TRACKALERT_MANPAGES_TARGET = trackalert.1 trackalert.conf.5
TRACKALERT_APIDOC = trackalert_api.7
endif
MANPAGES_TARGET = wforce.1 wforce.conf.5 wforce_webhook.5 $(TRACKALERT_MANPAGES_TARGET)

APIDOC = wforce_api.7 report_api.7 $(TRACKALERT_APIDOC)
SWAG2MD_PROG = swag2md.pl

EXTRA_DIST = manpages swagger $(SWAG2MD_PROG)

.PHONY: all-manpages

all-manpages: $(MANPAGES_TARGET)

api-doc: $(APIDOC)

dist_man_MANS = $(MANPAGES_TARGET)

$(MANPAGES_TARGET): %: manpages/%.md
	$(PANDOC) -s -t man $< -o $@

$(APIDOC): %: swagger/%.yml
	$(PERL) $(SWAG2MD_PROG) $< >$@.amd
	$(PANDOC) -s -t man $@.amd -o $@
endif

clean:
	rm -rf *.1 *.5 *.7 *.amd
