MANPAGES_TARGET = wforce.1

EXTRA_DIST = manpages

.PHONY: all-manpages

all-manpages: $(MANPAGES_TARGET)

if HAVE_PANDOC
dist_man_MANS = $(MANPAGES_TARGET)

$(MANPAGES_TARGET): %: manpages/%.md
	$(PANDOC) -s -t man $< -o $@
endif

clean:
	rm -rf *.1