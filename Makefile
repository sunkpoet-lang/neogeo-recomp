# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy. Part of cps2recomp.
# The toolkit's own checks, none of which needs a game. A game is built from its own repository
# (README.md); build.mk has the targets for that.
#
#   make audit      nothing from a game, no keys, no personal paths (tools/audit.sh)
#   make example    the program with no translated code and the launcher, for the stand-in game in example/
#   make smoke      start what make example built, and try tools/sync-toolkit.sh (tools/ci_smoke.sh)
#   make check      all three, and the copy of recomp-common

.PHONY: check audit example smoke check-common clean
check: audit check-common smoke

audit:
	@tools/audit.sh

check-common:
	@tools/sync-common.sh --check

example:
	@$(MAKE) --no-print-directory -C example
	@$(MAKE) --no-print-directory -C example launcher

smoke: example
	@tools/ci_smoke.sh

clean:
	rm -rf example/build example/work
