# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy. Part of cps2recomp.
#
# cps2recomp build rules for one game. Included by the top-level Makefile with GAME set (and by example/).
#
#   make            translate the game's code to C++ and build the game (build/$(GAME)/$(GAME)); on macOS also
#                   the launcher and the app bundle, so that the bundle is never older than the game (NO_APP=1: not)
#   make run        run it (windowed)
#   make explore    find the game's code by made-up play, in rounds (a fresh tree needs this once)
#   make record     run it and add the places it reaches by computed jumps to the entries file
#   make check      translated code against the interpreter run the same way, by state hash; two runs alike
#   make check-blocks  every translated block beside the interpreter (registers, cycles, writes)
#   make check-sound   the sound driver beside the game's sound program on the Z80 (every write to the chip)
#   make check-common  the copy of recomp-common (recomp-common/) is what tools/sync-common.sh recorded
#   make launcher   build the launcher (build/$(GAME)/$(GAME)-launcher)
#   make app        macOS app bundle (build/$(GAME)/<Name>.app)
#
# Per-game data lives in games/$(GAME)/ (game.cfg, movies); generated files in work/$(GAME)/
# (decrypted program, generated code, settings memory); builds in build/$(GAME)/. The ROM set is never
# part of the repository: ROM points at your own folder of its files.

# (CR: where the toolkit is, from where make runs: the repository's top, or .. from example/)
CR ?= .
CFG := python3 $(CR)/tools/gamecfg.py
WORK := work/$(GAME)
B := build/$(GAME)
GEN := $(WORK)/gen
PORT := $(B)/$(GAME)
export GAME B

ROM ?= rom
FRAMES ?= 3000
# (settings.ini: the features switched in the in-game menu; a run also notes the code it reaches that has
# no translation yet, for the next build)
EXPLORE_FEATURES ?= $(shell $(CFG) explore_features $(GAME))
EXPLORE_LADDER ?= $(shell $(CFG) explore_ladder_features $(GAME))
EXPLORE_LADDER_2 ?= $(shell $(CFG) explore_ladder_features_2 $(GAME))
RUN_ARGS = --rom "$(ROM)" --eeprom $(WORK)/eeprom.bin --settings $(WORK)/settings.ini --record-entries $(ENTRIES)

# The game's sources, and those of the games it shares sources with (game.cfg's sources: another game made from the
# same program, whose sources name its places as At(0x...); the game's family.h says where they are in it)
SHARES := $(shell $(CFG) sources $(GAME))
# Another set the game reads (game.cfg's extra_set: characters of another game taken in, tools/graft.py)
EXTRA_SET := $(shell $(CFG) extra_set $(GAME))
HOOK_SOURCES := games/$(GAME)/src$(foreach g,$(SHARES),:games/$(g)/src)

PROGRAM := $(WORK)/program.bin
ENTRIES := $(WORK)/entries.txt

.PHONY: all configure port gen run record explore check check-blocks check-sound check-rewrites check-common launcher app clean
# (on macOS the app bundle too, which is what the player opens: a bundle left from an older build hides what
# changed since)
ifeq ($(shell uname -s)$(NO_APP),Darwin)
all: port app
else
all: port
endif

# Generated code: the game's program as C++ (tools/recomp.py). It is made from the deciphered program, which
# the port itself writes; so the first build of a fresh tree is an interpreter-only one, and is done twice.
# (NO_ROM=1: there is no ROM set, so the build stops at the interpreter-only one; the toolkit's stand-in game)
gen:
	@mkdir -p $(GEN)
	@$(CFG) --header $(GEN)/game_info.h $(GAME)
	@if [ -f $(PROGRAM).extra ] && [ -f games/$(GAME)/graft.cfg ]; then \
		mkdir -p $(WORK)/extra; \
		ln -sf ../program.bin.extra $(WORK)/extra/program.bin; ln -sf ../program.bin.extra.data $(WORK)/extra/program.bin.data; \
		[ -f $(WORK)/extra/portmap.txt ] || python3 $(CR)/tools/portmap.py --from $(WORK) --to $(WORK)/extra \
			--out $(WORK)/extra/portmap.txt --entries $(WORK)/extra/entries.txt 2>/dev/null; \
		python3 $(CR)/tools/graft.py --spec games/$(GAME)/graft.cfg --work $(WORK) \
			--base $(shell $(CFG) extra_base $(GAME)) --out $(GEN)/graft_data.h; \
	fi
	@if [ -f $(PROGRAM) ]; then \
		python3 $(CR)/tools/recomp.py --program $(PROGRAM) --entries $(ENTRIES) --out $(GEN) \
			--limit $(shell $(CFG) code_end $(GAME)) --list $(WORK)/program.lst \
			--guess $(WORK)/guessed.txt --data $(PROGRAM).data \
			--hook-sources "$(HOOK_SOURCES)" --places games/$(GAME)/family.h --hooks $(GEN)/graft_data_hooks.txt; \
	fi
	@if [ -f $(PROGRAM).z80 ]; then \
		python3 $(CR)/tools/recompz80.py --program $(PROGRAM).z80 --entries $(ENTRIES).z80 --out $(GEN) \
			--list $(WORK)/sound.lst; \
	fi

configure:
	@cmake -S $(CR) -B $(B) -G Ninja -DCMAKE_BUILD_TYPE=Release -DGAME=$(GAME) \
	  -DGAME_DIR=$(CURDIR)/games/$(GAME) -DWORK_DIR=$(CURDIR)/work/$(GAME) > /dev/null

port: gen configure
	@ninja -C $(B) $(GAME)
	@if [ -z "$(NO_ROM)" ] && { [ ! -f $(PROGRAM).z80 ] || { [ -n "$(EXTRA_SET)" ] && [ ! -f $(PROGRAM).extra ]; }; }; then \
		$(PORT) --rom "$(ROM)" --dump-program $(PROGRAM) && $(MAKE) --no-print-directory port; fi

run: port
	$(PORT) $(RUN_ARGS) $(ARGS)

# Run the game and add the places it reaches by computed jumps to the entries file; 'make' then translates
# from those too (ARGS="--headless --frames 20000 --auto 1")
record: port
	$(PORT) $(RUN_ARGS) --stats $(ARGS)

# Find the game's code by playing it with made-up input, in rounds, translating what each round found before
# the next (a fresh tree knows almost none of it, and runs mostly in the interpreter until this is done; about
# ten minutes). EXPLORE_FEATURES: features that help the made-up play get far (cheats). EXPLORE_LADDER: features
# with which one made-up player wins every match, for the one-player game to its end (a few minutes more);
# EXPLORE_LADDER_2: another such set, for a second way through it (a hidden opponent, say).
explore: port
	@$(CR)/tools/explore.sh 1 6 12000
	@$(MAKE) --no-print-directory port
	@$(CR)/tools/explore.sh 10 12 40000 $(if $(EXPLORE_FEATURES),--features $(EXPLORE_FEATURES))
	@$(MAKE) --no-print-directory port
	@AUTO=":400:1" $(CR)/tools/explore.sh 30 12 60000 $(if $(EXPLORE_FEATURES),--features $(EXPLORE_FEATURES))
	@$(CR)/tools/explore.sh 50 6 30000 --test-switch 600:30000
	@if [ -n "$(EXPLORE_LADDER)" ]; then AUTO=":400:1" $(CR)/tools/explore.sh 100 12 130000 --features $(EXPLORE_LADDER); fi
	@if [ -n "$(EXPLORE_LADDER_2)" ]; then AUTO=":400:1" $(CR)/tools/explore.sh 120 6 130000 --features $(EXPLORE_LADDER_2); fi
	@$(MAKE) --no-print-directory port

# Every translated block beside the interpreter, compared (FRAMES=n, MOVIE=path or ARGS=...)
check-blocks: gen configure
	@ninja -C $(B) $(GAME)-check
	$(B)/$(GAME)-check --rom "$(ROM)" --headless --cpu check --frames $(FRAMES) --stats $(if $(MOVIE),--movie $(MOVIE)) $(ARGS)

# The game's rewritten routines against the code they replace (docs/READABLE.md); the game says how
check-sound: port
	@$(CR)/tools/check_sound.sh

check-rewrites: port check-common
	@games/$(GAME)/check_rewrites.sh

# The code shared with the other recomp ports (recomp-common/: the in-game menu) is a copy of recomp-common's, made
# by tools/sync-common.sh and never changed here; this fails if it was
check-common:
	@$(CR)/tools/sync-common.sh --check

# Translated code against the interpreter run the same way, frame by frame; and two runs alike
check: port check-common
	@$(CR)/tools/check.sh $(FRAMES) "$(MOVIE)" $(ARGS)

# The launcher (SDL3 + Dear ImGui; cps2recomp/launcher): the player's ROM set, features, controllers, Play.
# Built beside the port, which it runs.
launcher: port
	@cmake -S $(CR)/launcher -B $(B)/launcher -G Ninja -DCMAKE_RUNTIME_OUTPUT_DIRECTORY=$(CURDIR)/$(B) \
	  -DGAME_NAME="$(shell $(CFG) name $(GAME))" -DGAME_EXE=$(GAME) \
	  -DAPP_ID=$(shell $(CFG) app_id $(GAME)) -DHOME_ENV=$(shell echo $(GAME) | tr a-z A-Z)_HOME \
	  -DDISCLAIMER="$(shell $(CFG) disclaimer $(GAME))" \
	  -DROM_SET="$(shell $(CFG) rom_set $(GAME))" -DROM_PARENT="$(shell $(CFG) rom_parent $(GAME))" \
	  -DROM_NAME="$(shell $(CFG) rom_name $(GAME))" -DROM_PROGRAM="$(shell $(CFG) program $(GAME))" \
	  -DROM_GRAPHICS="$(shell $(CFG) graphics $(GAME))" -DROM_SOUND_PROGRAM="$(shell $(CFG) sound_program $(GAME))" \
	  -DROM_SAMPLES="$(shell $(CFG) samples $(GAME))" -DROM_KEY="$(shell $(CFG) key $(GAME))" \
	  -DDISPLAY_SWITCHES="$(shell $(CFG) display $(GAME))" > /dev/null
	@cmake --build $(B)/launcher
	@echo "built $(B)/$(GAME)-launcher"

# macOS app bundle: the launcher, the port and SDL3 (no game data)
app: launcher
	@$(CR)/tools/make_app.sh $(GAME)

clean:
	rm -rf $(B)
