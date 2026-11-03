# Changelog

**Note:** All dates are in Mexico City CST (UTC-6).

<!-- markdownlint-disable MD013 MD024 MD022 MD032 -->
## [2026-11-03] - Menu bar

- docs: describe the window's menu bar, its right-click menu and full-screen behaviour, and list the keys while playing.
- feat: give the game window a menu bar above the picture and the same menus on a right click, built from the in-game menu's rows plus the window's own; pause offline, dim what would change an online match, reveal the bar at the top edge in full screen, take screenshots with F8 and keep a picture with each save state; add `--present-test`, `--present-bar` and `--window-events` for tests.
- feat: let the presenter draw a window-sized overlay inside given regions only, uploaded when it changes, for the menu bar's strip and open menus.
- chore: sync recomp-common from the sunkpoet-lang fork (b78bd64), which adds the menu bar view, entry pictures and the in-game menu's rows as bar entries.

## [2026-10-29] - KOF2002 desktop integration

- docs: publish KOF2002 build, test and support notes with the OpenSpec implementation and desktop validation reports.
- feat: decrypt the original KOF2002 cartridge at runtime, preserve the linear M1 format and correct Z80 banking; compare all five transforms with MAME using synthetic fixtures.
- docs: document standalone ROM discovery, the game icon convention and verified portable deployment.
- test: exercise actual executable ROM discovery, explicit-path precedence and legacy layouts; verify unchanged KOF97 and KOF2002 frame hashes.
- build: embed KOF2002's title-screen logo as a Windows icon in seven sizes, following the collection convention.
- fix: find Neo Geo ROM sets in the portable collection's ROM folder when starting a game directly from another working directory.
