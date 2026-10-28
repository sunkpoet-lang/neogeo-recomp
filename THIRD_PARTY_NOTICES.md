# Third-party notices

neogeo-recomp is licensed under the GNU General Public License v3.0 (see `LICENSE`). It is built on
[cps2recomp](https://github.com/tmcleroy/cps2recomp) by Tommy McLeroy (GPL-3.0-only): the 68000 and Z80 and their
translators, the set reader, the in-game menu, the settings, the presenter, the HUD and the launcher come from it,
with their history (see the README). What follows is everything that was written for neither project; the licenses
of all of it are compatible with the GPL-3.0.

## In this repository

### MAME's Neo Geo driver (BSD-3-Clause)

The Neo Geo board (`machine/neogeo.cpp`, `machine/neogeo.h`, `video/neogeo_video.cpp`, `video/neogeo_video.h`)
follows MAME's Neo Geo driver closely enough to be credited as coming from it: `src/mame/snk/neogeo.cpp`, `neogeo.h`,
`neogeo_spr.cpp` and `neogeo_v.cpp` (the memory map and its registers, the interrupts and the display position timer,
the Z80's banks and command latches, the sprites' list, shrink and drawing, the fixed layer, the palette and its
resistor networks). The horizontal shrink table (`kShrinkX`) is MAME's as it is. **BSD-3-Clause**, by the copyright
holders MAME lists for those files: Bryan McPhail, Ernesto Corvi, Andrew Prime, Zsolt Vasvari (thanks to Fuzz):

```
Redistribution and use in source and binary forms, with or without modification, are permitted
provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions
   and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice, this list of
   conditions and the following disclaimer in the documentation and/or other materials provided
   with the distribution.
3. Neither the name of the copyright holder nor the names of its contributors may be used to
   endorse or promote products derived from this software without specific prior written
   permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
```

### ymfm (BSD-3-Clause)

The YM2610 (`machine/neogeo_sound.cpp`) is [ymfm](https://github.com/aaronsgiles/ymfm) by Aaron Giles (commit
81aec25), the FM chips' emulation MAME uses, kept unchanged: `third_party/ymfm.h`, `ymfm_fm.h`, `ymfm_fm.ipp`,
`ymfm_opn.h`, `ymfm_opn.cpp`, `ymfm_ssg.h`, `ymfm_ssg.cpp`, `ymfm_adpcm.h`, `ymfm_adpcm.cpp`. **BSD-3-Clause**,
Copyright (c) 2021, Aaron Giles, All rights reserved, with the conditions and disclaimer above.

### MAME (the CPS-2 program cipher, BSD-3-Clause)

`machine/cps2crypt.cpp` is MAME's `src/mame/capcom/cps2crypt.cpp` (worked out by Andreas Naive and Nicola Salmoria;
Copyright (c) Paul Leaman, Andreas Naive, Nicola Salmoria, Charles MacDonald, with the conditions and disclaimer
above). It comes with cps2recomp's set reader (`machine/romset.cpp`), which the Neo Geo shares and which still reads
CPS sets; `romset.cpp`'s `Unshuffle` and `DecodeKey` follow MAME's `src/mame/capcom/cps2.cpp`.

### recomp-common (GPL-3.0) and wiiparty-recomp (GPL-3.0-or-later)

`recomp-common/` is a copy of [recomp-common](https://github.com/tmcleroy/recomp-common) (the in-game menu, the
settings rules and the launcher; Tommy McLeroy's, GPL-3.0), never changed here. Its launcher descends from the
launcher of [wiiparty-recomp](https://github.com/arelkair/wiiparty-recomp) by arelkair (GPL-3.0-or-later):
`launcher/src/ui.cpp`, `ui.h` and `embedded.h` are that project's files nearly unchanged and are marked
GPL-3.0-or-later; `main.cpp`, parts of `app.cpp`, `game_process.cpp` and `embed_fonts.cmake` follow it.
recomp-common's own `THIRD_PARTY_NOTICES.md` has the table, file by file.

### Facts from fbneo-training-mode

The addresses in a game's memory that a game's sources use (for example KOF '97's players' health, in
`games/kof97/src/kof97.cpp`) come from [fbneo-training-mode](https://github.com/peon2/fbneo-training-mode) by peon2
and its contributors, which documents them for FBNeo's Lua scripts. Only those facts about the games are used; none
of its code is here.

## Downloaded at build time

None of these is stored in this repository.

| Component | License | Used for |
| --- | --- | --- |
| [SDL 3](https://libsdl.org) | zlib. Copyright (C) 1997-2026 Sam Lantinga | Window, input, audio, GPU (game and launcher); the system's where there is one |
| [Dear ImGui](https://github.com/ocornut/imgui) 1.92.9b | MIT. Copyright (c) 2014-2026 Omar Cornut | The launcher's interface, compiled into the launcher |
| [Inter](https://rsms.me/inter/) 4.1 | SIL Open Font License 1.1. Copyright (c) 2016 The Inter Project Authors | The launcher's font, compiled into the launcher |
| [zlib](https://zlib.net) | zlib. Copyright (C) 1995-2024 Jean-loup Gailly and Mark Adler | Reading a ROM set's archive, screenshots (PNG); the system's |

## Tools used to build (not included in builds)

Python, CMake, Ninja, a C++20 compiler; glslangValidator and SPIRV-Cross to regenerate `port/shaders.inc` from the
shaders in `port/shaders/`.

## What is not included

Nothing from any game or board is in this repository: no ROM or part of one, no BIOS, no graphics, sound or text, and
nothing made from a game's or the system's ROMs. The games' folders (`games/<id>/`) hold this project's own code and
notes about each game (addresses, the ROM set's file names and checksums). Code translated from a game by the
recompiler is made on the machine of someone who has the game's ROM set, and is never part of this repository.

Code added later from another project must have a license compatible with the GPL-3.0 and be listed here, with its
notice kept in the file.
