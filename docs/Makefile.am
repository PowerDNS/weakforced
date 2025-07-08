if WITH_TRACKALERT
TRACKALERT_MANPAGES_TARGET = trackalert.1 trackalert.conf.5
TRACKALERT_APIDOC = trackalert_api.7
endif
MANPAGES_TARGET = wforce.1 wforce.conf.5 wforce_webhook.5 wf_dump_entries.1 $(TRACKALERT_MANPAGES_TARGET)

APIDOC = wforce_api.7 $(TRACKALERT_APIDOC)
SWAG2MD_PROG = swag2md.pl

EXTRA_DIST = manpages swagger

.PHONY: all-manpages

all-manpages: $(MANPAGES_TARGET)

api-doc: $(APIDOC)

dist_man_MANS = $(MANPAGES_TARGET)

$(MANPAGES_TARGET): %: manpages/%.md
	asciidoctor -b manpage $< -o $@

$(APIDOC): %: swagger/%.yml
	openapi2markdown $< $@.amd
	sed 's/^#/##/g' <$@.amd >$@.mmd
	echo wforce_api.7 |  tr [:lower:] [:upper:] | sed -En 's/([a-zA-z_]+)\.([1-9])/= \1(\2)\n:doctype: manpage\n:manmanual: \1\n\n## NAME\n\n\1 - OpenAPI Spec\n/p' >$@.header
	cat $@.header $@.mmd >$@.fmd
	asciidoctor -b manpage $@.fmd -o $@

clean:
	rm -rf *.1 *.5 *.7 *.amd *.mmd *.fmd *.header
