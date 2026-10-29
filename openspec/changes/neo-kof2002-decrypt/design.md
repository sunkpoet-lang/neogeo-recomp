## Context
OpenSpec already uses schema spec-driven; retain openspec/config.yaml. Artifacts for this change are proposal.md, design.md, tasks.md and specs/neogeo-decryption/spec.md, in English per project configuration. Initial worktree: only untracked games/kof2002/ and neogeo-recomp.rar. The requested onlyfights420/skills repository (6296b00b31918381df9ebc73d6e468703e32a84b) supplies commit/changelog/MR workflows; no installation or commit is needed.

## Decisions
Use machine/ (the actual repository layout), not src/machine/. Dispatch only list.set == kof2002 after CRC verification and complete cartridge/system loading. Validate exact original region sizes before modifying any bytes: P 500000, C 4000000, V 1000000, encrypted M1 20000; FIX must be empty. Reject unsupported layouts with a descriptive error.

MAME reference: mamedev/mame revision 8c285468743535fbbde018fef23b3cfb8c9a3ec8, src/devices/bus/neogeo/{prot_cmc.cpp,prot_kof2k2.cpp,prot_pcm2.cpp,kof2k2.cpp}, src/mame/snk/neogeo.cpp. Preserve BSD-3-Clause attribution and licence. Apply P2 on bytes already word-swapped by the loader, CMC50 on interleaved C pairs, then extract 20000 FIX bytes, decrypt M1 and PCM2 type 0. CMC50 tables are copied exactly.

### M1 and Z80 representation
NEO_BIOS_AUDIO_ENCRYPTED_128K loads 20000 bytes into a zero-initialised 80000-byte audiocrypt region (no ROM_RELOAD). The key is the modulo-65536 sum of the first 10000 bytes, not a sum of 16-bit words. Decrypt all 80000 bytes with block-specific address permutations. MAME copies the first 10000 bytes and then all 80000 into its 90000-byte audiocpu region; bank setup skips that 10000 prefix. This board already reads the fixed window and all four banks from linear cartridge data, so store the decrypted 80000 bytes directly, including the zero tail. MAME masks bank addresses with (linear_size - 1) & 0x3ffff. Correct the local modulo mapping to preserve this 256 KB bank-address limit; KOF97 is unchanged because its M1 is 128 KB. Test all four windows, high bank selection and BIOS/fixed selection.

## Validation
Before changes, record KOF97 headless hashes/audio/screenshots using local user ROMs. Compile both games; test algorithms against the upstream source using synthetic inputs, invalid sizes without mutation, dispatch isolation, and real loader output. Repeat KOF97 with identical inputs and compare hashes/audio. Run KOF2002 to title/attract, inspect screenshots and audio, and record limitations honestly. Store generated outputs under ignored work/. Do not commit or push.
