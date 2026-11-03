# Context

Existing source starts at 4bb5830; only an unrelated untracked neogeo-recomp.rar was present. Existing OpenSpec configuration is retained (English). The first Workbench variant remains Magic Plus II; this runtime implementation does not constitute the still-pending Workbench GUI/CLI trial.

# Decisions

MAME boot_kof2k2.cpp dispatches kf2k2mp2_px_decrypt, PCM2 swap(0), CMC50 M1, sx_decrypt(1), CMC50 graphics(0xEC). prot_misc.cpp relocates a zero-initialised 6 MiB buffer: destination/source/length (0,0x1c0000,0x40000), (0x40000,0x140000,0x80000), (0xc0000,0x100000,0x40000), (0x100000,0x200000,0x400000). The last MiB remains zero. S1 swaps the two 8-byte halves of every 16-byte block; it is not extracted from C-ROMs. Pin upstream provenance before porting.

Require raw P=6 MiB, C=64 MiB, V=16 MiB, M1=128 KiB, S1=128 KiB before changing any region. P words are already byte-swapped by LoadNeoGeoSet. Reuse shared CMC/PCM helpers without changing the original pipeline. Expand M1 with zero fill to 512 KiB, decrypt and retain its linear buffer rather than MAME's prefixed audiocpu view. Loader dispatch uses exact set ID; parent identity never selects a pipeline.

# Validation

Extend the independent upstream-body synthetic oracle for this cartridge and compare all transformed regions. Exercise invalid sizes with no mutation, actual loader interleaving/word swap/S1 and unchanged unknown-set path. Retain exhaustive fixed/four-window Z80 tests. Capture KOF97/KOF2002 deterministic frame and audio baselines before rebuilding shared code. Run real variant boot and scripted start separately from synthetic checks. Title-screen capture and embedded ICO are required for the finished executable; no inherited parent image.
