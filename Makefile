# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy. Part of cps2recomp.
#
# The toolkit and its games. A game is a folder under games/ (game.cfg, its sources, movies, checks); its ROM
# set is the player's own, in rom/<id>/ unless ROM says where (never part of the repository).
#
# One game, as GAME=<id> (build.mk has the targets):
#   make GAME=<id>                  translate its code and build it (on macOS its launcher and app bundle too)
#   make GAME=<id> explore          find its code by made-up play (a fresh tree needs this once)
#   make GAME=<id> check | check-blocks | check-rewrites | check-release | check-widescreen
#
# Every game, and the collection (one launcher for all of them):
#   make games          build every game whose ROM set is in rom/<id>/
#   make collection     the collection's launcher, with the games built beside it (build/collection/)
#   make collection-app macOS app bundle of the collection (build/collection/<Name>.app)
#
# The toolkit's own checks, none of which needs a game:
#   make audit      nothing from a game, no keys, no personal paths (tools/audit.sh)
#   make example    the program with no translated code and the launcher, for the stand-in game in example/
#   make smoke      start what make example built (tools/ci_smoke.sh)
#   make check      the audit, the copy of recomp-common, and the smoke test

GAMES := $(notdir $(patsubst %/game.cfg,%,$(wildcard games/*/game.cfg)))

ifdef GAME

CR := .
ROM ?= rom/$(GAME)
export ROM
include build.mk

# The build with no translated code (the interpreters; what CI builds) against the translated build
.PHONY: check-release
check-release: port
	@tools/check_release.sh $(FRAMES)

# The game's own check of its widescreen
.PHONY: check-widescreen
check-widescreen: port
	@games/$(GAME)/check_widescreen.sh

else

.PHONY: check audit example smoke check-common clean games collection collection-app
check: audit check-common smoke

audit:
	@tools/audit.sh

check-common:
	@tools/sync-common.sh --check

example:
	@$(MAKE) --no-print-directory -C example NO_APP=1
	@$(MAKE) --no-print-directory -C example launcher

smoke: example
	@tools/ci_smoke.sh

# (each game whose ROM set is here; the others are left out, and said so)
games:
	@for g in $(GAMES); do \
		if [ -e rom/$$g ]; then $(MAKE) --no-print-directory GAME=$$g NO_APP=1 port || exit 1; \
		else echo "$$g: no ROM set in rom/$$g, not built"; fi; \
	done

collection: games
	@tools/collection.sh

collection-app: collection
	@tools/make_app.sh --collection

clean:
	rm -rf example/build example/work

endif
