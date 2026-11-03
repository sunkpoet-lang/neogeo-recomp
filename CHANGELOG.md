# Changelog

**Note:** All dates are in Mexico City CST (UTC-6).

<!-- markdownlint-disable MD013 MD024 MD022 MD032 -->
## [2026-10-30] - Magic Plus II cartridge support

- feat: support the original KOF98 cartridge with validated program decryption and a serialised Altera header overlay, preserving unrelated cartridges' state layouts.
- test: compare KOF98 with pinned MAME synthetic goldens, exercise protection commands/byte lanes/reset/state restoration and cover 256 KiB M1 banks.
- build: embed the variant's own title-screen icon in seven sizes and verify its Windows resources and isolated test package.
- test: compare Magic Plus II with upstream MAME synthetic fixtures, exercise loader isolation and invalid layouts, and retain identical KOF97/KOF2002 frame hashes and audio.
- feat: add the kf2k2mp2 split-set definition and validated variant-specific program/S1 transforms, reusing CMC50 and PCM2 without changing the linear M1 layout.

## [2026-10-29] - KOF2002 desktop integration

- docs: publish KOF2002 build, test and support notes with the OpenSpec implementation and desktop validation reports.
- feat: decrypt the original KOF2002 cartridge at runtime, preserve the linear M1 format and correct Z80 banking; compare all five transforms with MAME using synthetic fixtures.
- docs: document standalone ROM discovery, the game icon convention and verified portable deployment.
- test: exercise actual executable ROM discovery, explicit-path precedence and legacy layouts; verify unchanged KOF97 and KOF2002 frame hashes.
- build: embed KOF2002's title-screen logo as a Windows icon in seven sizes, following the collection convention.
- fix: find Neo Geo ROM sets in the portable collection's ROM folder when starting a game directly from another working directory.
