REPORT_API_DIST = \
		  MANIFEST.in \
		  helpers \
		  instance \
		  nginx \
		  requirements.txt \
		  run.sh \
		  runreport.py \
		  setup.py \
		  wforce

if HAVE_PANDOC

MANPAGES_TARGET = wforce-report-api.5

.PHONY: all-manpages

all-manpages: $(MANPAGES_TARGET)

$(MANPAGES_TARGET): %: manpages/%.md
	$(PANDOC) -s -t man $< -o $@

dist_man_MANS = $(MANPAGES_TARGET)

endif

EXTRA_DIST= $(REPORT_API_DIST) manpages

clean:
	rm -f *.pyc *.pyo */*.pyc */*.pyo *.5
	rm -rf venv __pycache__ */__pycache__
