# Decisions

Pin MAME 8c285468743535fbbde018fef23b3cfb8c9a3ec8 prot_kof98.cpp, kof98.cpp and neogeo.cpp. The 6 MiB P region is word-swapped before decryption; the first 2 MiB is reorganised and the 4 MiB at 0x200000 moved to 0x100000, preserving the region length as MAME does. S1=128 KiB, M1=256 KiB, C=64 MiB and V=16 MiB are unencrypted and untouched. Validate all sizes before mutation.

KOF98-only reads at 0x100/0x102 return original decrypted words in state 0, 0x00c2/0x00fd in state 1 and 0x4e45/0x4f2d in state 2. Writes at 0x20aaaa with 0x0090/0x00f0 select those states; unknown writes retain the state. Support byte lanes via the existing on-bus representation. Protection state starts at 0, survives ordinary reset like MAME's empty device_reset and is serialised for deterministic state restoration. Other sets never activate this handler.

Use synthetic upstream-body decryption goldens and direct board overlay tests including byte reads, writes, reset and save/load. Record KOF97/KOF2002/Magic Plus II baselines before modifying board state. Real captures verify title, selection and combat; PCM output is distinct from human listening approval. Local peers and stress rollback do not prove real Internet acceptance.

ForkAde's maintained build recipe exports pinned Git revisions. New uncommitted source must enter through an explicit reviewed, hash-verified allowlist overlay, recorded in the recipe/build manifest, never silently through dirty working-tree copying. Preserve existing source configuration and package alongside all existing KI/FBNeo/CPS games, DLLs, notices and tunnel. Separate candidate, no player data or ROM copies. Launcher recognises each exact set, parent and online arguments.
