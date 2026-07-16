.PHONY: all plugin clean rebuild dist public-check release

VERSION ?= 1.0.0

all: plugin

plugin:
	$(MAKE) -C plugin

clean:
	$(MAKE) -C plugin clean
	rm -f dist/PSPAchievementsNG.prx dist/config.ini

rebuild: clean all

dist: plugin
	mkdir -p dist
	cp plugin/PSPAchievementsNG.prx dist/
	cp config.ini dist/

public-check:
	python3 scripts/check_public_tree.py .

release: public-check dist
	python3 scripts/make_release.py --version $(VERSION)
