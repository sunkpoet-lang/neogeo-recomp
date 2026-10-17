# Third-party notices

cps2recomp is licensed under the GNU General Public License v3.0 (see `LICENSE`). Most of it was
written for this project. What follows is everything that was not; the licenses of all of it are
compatible with the GPL-3.0.

## In this repository

### MAME (the CPS-2 program cipher)

`machine/cps2crypt.cpp` is MAME's `src/mame/capcom/cps2crypt.cpp`: the tables and the two Feistel
networks are as they are there, and only the entry point at the end of the file was written here.
The cipher was worked out by Andreas Naive and Nicola Salmoria. **BSD-3-Clause**:

```
Copyright (c) Paul Leaman, Andreas Naive, Nicola Salmoria, Charles MacDonald
All rights reserved.

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

It is compiled into a game's program and its launcher, so both carry this notice.

More was written here but follows MAME closely enough to be credited as coming from it (all
BSD-3-Clause there; the notice above covers the same terms, with these authors):

| Here | After | What |
| --- | --- | --- |
| `sound/qsound.cpp`, `qsound.h` | `src/devices/sound/qsoundhle.cpp`, by superctr and Valley Bell | What the QSound chip's program does with its registers: the voices, the echo, the two start-up states. Written again with this project's names; the pan curve is this project's own. |
| `machine/romset.cpp`: `Unshuffle`, `DecodeKey` | `src/mame/capcom/cps2.cpp` | How the graphics ROMs' address lines are untangled, and the layout of a key file (about thirty lines). |

The rest of the machine (`machine/`, `video/`, `sound/z80.cpp`) is this project's code. It uses
facts about the hardware that MAME's CPS-1/CPS-2 drivers record (register layouts, priority rules,
timings); the files that do say so. No emulator is compiled or linked into a game, and none is in
this repository.

The arcade timing (`machine/machine.cpp`, `LineDma`; `machine/m68k.h`, `busWait`; `docs/TIMING.md`) follows what
[jtcps2](https://github.com/jotego/jtcores) (jotego's CPS-2 core, GPL-3.0-or-later) does: the board's acknowledge
after Loic Petit's study of the DL-1827, and the CPS-A's DMA as jotego measured it. No code of it is here; the rules
were written again from reading it and from simulating it, and checked against the simulation.

### wiiparty-recomp (GPL-3.0-or-later) and recomp-common (GPL-3.0)

`recomp-common/` is a copy of [recomp-common](https://github.com/tmcleroy/recomp-common) (the
in-game menu, the settings rules and the launcher; this author's, GPL-3.0), made by
`tools/sync-common.sh` and never changed here. Its launcher descends from the launcher of
[wiiparty-recomp](https://github.com/arelkair/wiiparty-recomp) by arelkair (GPL-3.0-or-later):
`launcher/src/ui.cpp`, `ui.h` and `embedded.h` are that project's files nearly unchanged and are
marked GPL-3.0-or-later; `main.cpp`, parts of `app.cpp`, `game_process.cpp` and `embed_fonts.cmake`
follow it. recomp-common's own `THIRD_PARTY_NOTICES.md` has the table, file by file.

The 5x7 dot-matrix font of the menu (`recomp-common/text.h`) is the generic character set of LCD
and LED displays, typed in for recomp-common.

### Sound file decoders (public domain, or MIT / MIT-0)

The custom soundtrack (`port/soundtrack.cpp`) reads the player's music files with four single-file decoders,
kept unchanged in `third_party/` and compiled by `port/decoders.cpp`. Each is offered under either of two
licences, the first being the public domain; the licence statements are at the end of each file, and the release
archives carry them in `licenses/sound-decoders.txt`.

| File | Project | Licence |
| --- | --- | --- |
| `third_party/stb_vorbis.c` | [stb](https://github.com/nothings/stb) `stb_vorbis` 1.22 (commit 2c980bb), by Sean Barrett | Public domain, or MIT |
| `third_party/dr_flac.h` | [dr_libs](https://github.com/mackron/dr_libs) `dr_flac` 0.13.4 (commit dfe8377), by David Reid | Public domain, or MIT No Attribution |
| `third_party/dr_mp3.h` | dr_libs `dr_mp3` 0.7.4 (the same commit), by David Reid, after minimp3 by lieff | Public domain, or MIT No Attribution |
| `third_party/dr_wav.h` | dr_libs `dr_wav` 0.14.6 (the same commit), by David Reid | Public domain, or MIT No Attribution |

## Downloaded at build time

None of these is stored in this repository.

| Component | License | Used for |
| --- | --- | --- |
| [SDL 3](https://libsdl.org) | zlib. Copyright (C) 1997-2026 Sam Lantinga | Window, input, audio, GPU (game and launcher); the system's where there is one |
| [Dear ImGui](https://github.com/ocornut/imgui) 1.92.9b | MIT. Copyright (c) 2014-2026 Omar Cornut | The launcher's interface, compiled into the launcher |
| [Inter](https://rsms.me/inter/) 4.1 | SIL Open Font License 1.1. Copyright (c) 2016 The Inter Project Authors | The launcher's font, compiled into the launcher |
| [zlib](https://zlib.net) | zlib. Copyright (C) 1995-2024 Jean-loup Gailly and Mark Adler | Reading a ROM set's archive, screenshots (PNG); the system's |

Each download is checked against its published SHA-256. The release archives (`tools/release/`) carry the
license texts of SDL 3, Dear ImGui and Inter (the font is inside the launcher), MAME's notice above, and the
GPL-3.0 with the offer of source that goes with it (`SOURCE.txt`: the tag's source on GitHub). In release builds
SDL 3 (3.4.16) is linked into every program; `zlib1.dll` is in the Windows archive, and there the MinGW-w64
runtime (libstdc++, libgcc, winpthreads) is linked in: GPL-3.0 with the GCC Runtime Library Exception, and the
MinGW-w64 runtime licence, whose texts the Windows archive carries too.

## Tools used to build (not included in builds)

Python, CMake, Ninja, a C++ compiler; glslangValidator and SPIRV-Cross to regenerate
`port/shaders.inc` from the shaders in `port/shaders/` (which are this project's).

## What is not included

Nothing from any game or board is in this repository: no ROM or part of one, no graphics, sound or
text, no key, and nothing made from a game's ROMs. The games' folders (`games/<id>/`) hold this project's own
code and notes about each game (addresses and record layouts found by the author, the ROM set's file names and
checksums), input movies (text: which buttons were held on which frame), and the README's pictures of the games
in `docs/screenshots/`, made by the programs from the author's own ROM sets. Code translated from a game by the recompiler is
made on the machine of someone who has the game's ROM set, and is never part of this repository.

`sound/driver.cpp` is the one file here written from a program of Capcom's: it does in C++ what
version 2.01b of the sound program on these boards does (the Z80 program that plays a game's music
and sound effects on the QSound chip). It was written by hand from reading that program, routine by
routine, in the way a game's repository rewrites routines of its game (`docs/READABLE.md`), and is
checked against the program write for write on the player's own ROMs. It reads every table,
sequence and sample from the player's ROM set; the only bytes of the program it carries are the 29
characters of the version text it knows the program by.

Code added later from another project must have a license compatible with the GPL-3.0 and be listed
here, with its notice kept in the file. `tools/audit.sh` fails if a file marked as another
project's is not one of those listed above.
