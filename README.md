# neogeo-recomp

Native PC ports of Neo Geo MVS arcade games, built on the static recompilation toolkit of
[**CPS-2 Recomp**](https://github.com/tmcleroy/cps2recomp) by **Tommy McLeroy**. Bring your own game files.

> **Not affiliated with SNK** or any rights holder. No game, BIOS or part of one is in this repository: you need
> your own ROM sets, dumped from boards you own.

## Credits first

This project stands on other people's work:

- **Tommy McLeroy**, [cps2recomp](https://github.com/tmcleroy/cps2recomp) and
  [recomp-common](https://github.com/tmcleroy/recomp-common): the 68000 and Z80 and their translators to C++, the set
  reader, the in-game menu, the settings, the presenter, the HUD and the launcher. This repository started from his,
  and keeps the history of every file it took from it.
- **The MAME team**: the Neo Geo board here follows MAME's Neo Geo driver (Bryan McPhail, Ernesto Corvi, Andrew Prime,
  Zsolt Vasvari, thanks to Fuzz), and the set reader carries MAME's CPS-2 cipher.
- **Aaron Giles**, [ymfm](https://github.com/aaronsgiles/ymfm): the YM2610 sound chip.
- **arelkair**, [wiiparty-recomp](https://github.com/arelkair/wiiparty-recomp): where the launcher comes from.
- **peon2 and contributors**, [fbneo-training-mode](https://github.com/peon2/fbneo-training-mode): the facts about
  the games' memory that the games' own sources use.

`THIRD_PARTY_NOTICES.md` has every licence.

## What works

| Game | Status |
|---|---|
| The King of Fighters '97 (`kof97`, NGM-2320) | Plays from the launcher: picture, sound, controls, in-game menu, save states, cheats |
| The King of Fighters 2002 (`kof2002`, NGM-2650) | Original encrypted set: runtime decryption, title/attract graphics and sound; see `games/kof2002/README.md` |

The **board** is modelled: the 68000's memory map and banks, the system ROM and its vectors, the interrupts and the
display position timer, the inputs, the LSPC video (sprites with shrink and chaining, the fix layer, the palettes)
and the Z80 playing the YM2610. Games run in the 68000's **interpreter**, several times faster than the board needs;
translation to C++, the toolkit's other half, is the next big step. Runs are repeatable frame for frame (a hash of
the whole state each frame), which the checks and, later, online play rely on.

## Playing

You need your own sets, as MAME names them: the game's (`kof97.zip` or the original `kof2002.zip`) and the Neo Geo's system set (`neogeo.zip`,
with the MVS system ROM `sp-s2.sp1`), in the same folder.

Open `neogeo-recomp-launcher`, show it the folder your sets are in, and press Play. The Controllers page sets the
keys and gamepads (the Neo Geo's A and C on a six-button stick's top row, B and D on its bottom row). While playing,
Esc or F1 (or the left stick's click, or Start and Select together) opens the menu: save states, coin, speed, sound,
cheats, display. The first time a game starts, the board sets itself up and restarts once, as a new MVS does.

For direct startup, place both sets in `roms/`, `ROM/` or `rom/` beside the game's executable, or directly beside
it. An explicit `--rom /path/to/game.zip` takes precedence. See [KOF2002's notes](games/kof2002/README.md) for the
encrypted set format, standalone startup and title-screen icon.

## Building

Windows with [MSYS2](https://www.msys2.org) (UCRT64), Linux or macOS: CMake, Ninja, a C++20 compiler, Python 3,
SDL 3 and zlib. In MSYS2's UCRT64 shell:

    pacman -S --needed mingw-w64-ucrt-x86_64-{gcc,cmake,ninja,python,SDL3,zlib}
    make GAME=kof97          # build/kof97/kof97
    make collection          # build/collection/: the launcher with every game beside it

Without a window, for checks: `build/kof97/kof97 --headless --rom /path/to/kof97.zip --frames 3000 --hash-log
hashes.txt` (more in `games/kof97/README.md`).

## How it is laid out

| Where | What |
|---|---|
| `machine/neogeo*`, `video/neogeo_video*` | The Neo Geo board: memory, interrupts, inputs, the YM2610, the LSPC |
| `port/neogeo_*` | A Neo Geo game's port: its loop, its window, its options |
| `runtime/neogeo_game.h` | What a game's own sources do with the board (cheats today; training mode next) |
| `games/<id>/` | Each game: `game.cfg` (its set's files and checksums), its sources, its notes |
| `machine/m68k*`, `sound/z80*`, `port/menu*`, `port/present*`, `recomp-common/`, `launcher/` | From cps2recomp |
| `openspec/` | The specs and the plans of each change ([OpenSpec](https://github.com/Fission-AI/OpenSpec)) |

## Checks

Enable synthetic decryption, ROM-loader, Z80-bank and executable-startup checks without supplying ROMs:

```sh
make GAME=kof2002
cmake -S . -B build/kof2002 -G Ninja -DGAME=kof2002 -DNEOGEO_BUILD_TESTS=ON
cmake --build build/kof2002
ctest --test-dir build/kof2002 --output-on-failure
```

The [decryption report](openspec/changes/neo-kof2002-decrypt/validation.md) records the pinned MAME reference,
synthetic golden values and KOF97 regression. The [desktop report](openspec/changes/neo-kof2002-desktop/validation.md)
records discovery checks and Windows icon verification. Actual game checks require the player's own sets.

## Next

- The custom soundtrack (your own music in place of the board's, as CPS-2 Recomp's Super Turbo has it)
- Training mode for the fighting games (frame data, inputs, hitboxes, a dummy that records and replays)
- Further validation of online rollback ([GekkoNet](https://github.com/HeatXD/GekkoNet)) over direct and relayed connections; see [KOF97's online instructions](games/kof97/README.md)
- Translation to C++
- More games: Garou, the Fatal Fury and Real Bout series, other KOFs, Samurai Shodown, Last Blade, Metal Slug

## Contributing

Issues and pull requests are welcome. Never add a game's or the system's files, or anything made from them. Each
change starts as an OpenSpec proposal in `openspec/changes/`; new code from another project needs a GPL-3.0
compatible licence and its line in `THIRD_PARTY_NOTICES.md`.

## Licence

GPL-3.0 (`LICENSE`), as CPS-2 Recomp's.
