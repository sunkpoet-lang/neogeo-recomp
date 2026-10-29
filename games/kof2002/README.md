# The King of Fighters 2002

The original MAME/FBNeo `kof2002` cartridge (NGM-2650): runtime P2, CMC50 graphics/M1, extracted FIX and PCM2 samples.
Use your own `kof2002.zip` and `neogeo.zip` (with `sp-s2.sp1`), beside each other. Bootleg, Plus and predecrypted sets
are not supported. The encrypted `265-m1.m1` is 128 KiB, CRC `85aaa632`; do not replace it with an expanded M1.
No ROM files are part of this change.

## Build and run

In the MSYS2 UCRT64 shell (the same dependencies as KOF97):

```sh
make GAME=kof2002
build/kof2002/kof2002 --rom /path/to/kof2002.zip
```

You can also double-click `kof2002.exe`. Without `--rom`, it searches for `kof2002.zip` in this order:

1. `roms/`, `ROM/`, `rom/`, then directly beside the executable.
2. `roms/` in the current working directory, then directly in that directory.

Keep `neogeo.zip` beside the game set. The collection's existing `ROM/` folder works without renaming or moving
anything, even when the executable is opened from another working directory. An explicit `--rom` always wins;
an invalid selected set reports its validation error instead of silently selecting a different copy.

The Windows executable embeds the actual title-screen logo as its icon, with sizes from 16 to 256 pixels, following
the collection's icon convention. Windows Explorer may briefly retain an older cached icon after replacement.

The collection discovers `games/kof2002/game.cfg` through `make collection`. Controls and the in-game menu are the
same as KOF97. The original ROMs are checked by CRC before any decryption. FIX is extracted from decoded sprites;
there is no S1 file. The loader rejects unexpected original-set region sizes.

## Checks

```sh
cmake -S . -B build/kof2002 -DNEOGEO_BUILD_TESTS=ON -DGAME=kof2002
cmake --build build/kof2002
ctest --test-dir build/kof2002 --output-on-failure
mkdir -p work/kof2002-check
build/kof2002/kof2002 --headless --rom /path/to/kof2002.zip --frames 6000 --stats \
  --script "3100:coin1:1,3250:start1:8" --shot-every 600 --shot-dir work/kof2002-check \
  --hash-log work/kof2002-check/hashes.txt --audio-dump work/kof2002-check/audio.wav
```

The synthetic tests need no game ROMs. They verify all five region CRCs against upstream MAME on two deterministic
artificial cartridges, all four Z80 bank windows, malformed-layout rejection without mutation, loading order,
CRC failure and isolation from other set IDs. See `tests/mame_decrypt_reference.py` for reproducing the MAME golden
CRCs. Test data belongs in ignored build/work folders.

The important sound-layout distinction: this board stores linear M1 data. MAME's extra 64 KiB audiocpu prefix must
not be inserted here. The encrypted 128 KiB input is zero-extended to 512 KiB before address decryption; the Z80
banks use MAME's 18-bit address limit. See the design and validation report under
`openspec/changes/neo-kof2002-decrypt/` for sources and observed results.
