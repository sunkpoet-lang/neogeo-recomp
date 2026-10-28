# SPDX-License-Identifier: GPL-3.0-only
# Part of neogeo-recomp.
#
# The games and the collection. A game is a folder under games/ (game.cfg, its sources); its ROM set and the Neo Geo's
# system set (neogeo.zip) are the player's own, never part of the repository.
#
#   make GAME=<id>        build one game: build/<id>/<id>      (make GAME=kof97)
#   make games            build every game
#   make collection       the collection's launcher with every game beside it (build/collection/)
#   make clean
#
# Then: build/kof97/kof97 --rom /path/to/kof97.zip   (neogeo.zip beside it), or --headless for the checks
# (games/kof97/README.md).

GAMES := $(notdir $(patsubst %/game.cfg,%,$(wildcard games/*/game.cfg)))
CFG := python3 tools/gamecfg.py

ifdef GAME

.PHONY: all clean
all:
	@mkdir -p work/$(GAME)/gen
	@$(CFG) --header work/$(GAME)/gen/game_info.h $(GAME)
	@cmake -S . -B build/$(GAME) -G Ninja -DCMAKE_BUILD_TYPE=Release -DGAME=$(GAME) > /dev/null
	@ninja -C build/$(GAME) $(GAME)

clean:
	rm -rf build/$(GAME) work/$(GAME)

else

.PHONY: all games collection clean
all: games

games:
	@for g in $(GAMES); do $(MAKE) --no-print-directory GAME=$$g || exit 1; done

collection: games
	@tools/collection.sh

clean:
	rm -rf build work

endif
