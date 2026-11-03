# The King of Fighters 2002 Magic Plus II

The `kf2k2mp2` bootleg variant, using the Neo Geo runtime (68000 interpreter). This is not the original
KOF2002 cartridge or Magic Plus I. Supply your own `kf2k2mp2.zip`, `kof2002.zip` and `neogeo.zip` beside each other.
No game or BIOS files are distributed.

The split clone contains `k2k2m2p1.bin` (CRC 1016806c), `k2k2m2p2.bin` (432fdf53) and `k2k2m2s1.bin` (446e74c5).
Graphics, encrypted M1 and samples come from the original parent. The system set needs `sp-s2.sp1` and the
other BIOS members named in game.cfg. Keep the encrypted 128 KiB M1; do not substitute a prefixed/expanded dump.

## Build and run

In the existing MSYS2 UCRT64 build environment:

```sh
make GAME=kf2k2mp2
build/kf2k2mp2/kf2k2mp2 --rom /path/to/kf2k2mp2.zip
```

Direct startup searches `roms/`, `ROM/`, `rom/` and the executable directory, then the working directory's
`roms/` and its root. Parent and BIOS remain beside the selected set. An explicit `--rom` takes precedence.
The portable test helper uses the central private library explicitly; the deployed ForkAde beta is unchanged.

The pipeline validates all sizes first, relocates the 6 MiB program, applies PCM2 type 0, decrypts CMC50 M1,
swaps each S1 block's eight-byte halves, and decrypts CMC50 sprites. S1 is not extracted from the graphics.
The Z80 continues using linear M1 bytes and the existing fixed/four-bank-window mapping.

## Validation and limits

See [the OpenSpec report](../../openspec/changes/neo-kf2k2mp2/validation.md) for the pinned MAME revision,
independent synthetic oracle, loader/startup tests and original-game regressions. Own title/character-selection/
combat captures were inspected; nonzero PCM audio was measured. These are not full playthrough or Internet
netplay acceptance. Human sound-quality/controller verification remains a player test.

The executable's seven-size Windows icon comes from this variant's own title screen, including MAGIC PLUS II.
The collection's generic discovery includes this game.cfg; the separate maintained ForkAde package/catalogue
has not been updated or deployed by this change.
