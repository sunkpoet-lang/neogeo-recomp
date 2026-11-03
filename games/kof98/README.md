# The King of Fighters '98

Original encrypted `kof98` (NGM-2420): 242-p1.p1 CRC 8893df89, 242-p2.sp2 CRC 980aba4c. Supply your own kof98.zip
and neogeo.zip together. Predecrypted and alternate M1 sets require their own definitions; no ROMs are distributed.

The loader validates P=6 MiB, S=128 KiB, M1=256 KiB, C=64 MiB and V=16 MiB before the MAME program transform.
Only P is transformed; the Altera header overlay is handled by the board at 0x100/0x102 and 0x20aaaa. Its state
is saved/restored for rollback; unrelated cartridge state layouts are unchanged. This native host uses the
existing 68000 interpreter, not a complete static translation.

Build in the existing UCRT64 environment: `make GAME=kof98`. Run `build/kof98/kof98 --rom /path/to/kof98.zip`.
For direct EXE startup keep both sets in ROM/ (also roms/ or rom/) beside the EXE. Shared library routing is provided
by ForkAde. The game carries its own title-screen icon in seven sizes.

Online uses the existing GekkoNet implementation and ForkAde host/join controls. Both peers need the same build,
sets and settings. Local peer/rollback checks are distinct from real Internet acceptance; see the OpenSpec report
under openspec/changes/neo-kof98 for the exact tested scope. No deployment replaces the working beta automatically.
