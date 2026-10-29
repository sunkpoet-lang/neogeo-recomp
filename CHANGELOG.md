# Changelog

**Note:** All dates are in Mexico City CST (UTC-6).

<!-- markdownlint-disable MD013 MD024 MD022 MD032 -->
## [2026-10-29] - KOF2002 desktop integration

- docs: publish KOF2002 build, test and support notes with the OpenSpec implementation and desktop validation reports.
- feat: decrypt the original KOF2002 cartridge at runtime, preserve the linear M1 format and correct Z80 banking; compare all five transforms with MAME using synthetic fixtures.
- docs: document standalone ROM discovery, the game icon convention and verified portable deployment.
- test: exercise actual executable ROM discovery, explicit-path precedence and legacy layouts; verify unchanged KOF97 and KOF2002 frame hashes.
- build: embed KOF2002's title-screen logo as a Windows icon in seven sizes, following the collection convention.
- fix: find Neo Geo ROM sets in the portable collection's ROM folder when starting a game directly from another working directory.
