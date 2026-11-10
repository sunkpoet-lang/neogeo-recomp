# Changelog

**Note:** All dates are in Mexico City CST (UTC-6).

<!-- markdownlint-disable MD013 MD024 MD022 MD032 -->

## [2026-11-09] - Match spectators

- feat: let up to six spectators watch an online match (--watch, or --join with --spectate), joining at any point from the host's confirmed state, through the arcade wait and same-peer recovery, with their own chat relayed by the host and a Mute spectators switch; KOF '97 and KOF '98 checked with offline, stress and chat regression unchanged

## [2026-11-06] - KOF97 ForkAde match flow

- test: pass full local acceptance separately for KOF2002, Magic Plus II, KOF98 and the rebuilt KOF97 regression, including 80 peer pairs, unchanged offline state/audio, team streaks, rollback, recovery and native player-data preservation
- feat: extend the explicit match-flow gate to KOF2002, Magic Plus II and KOF98 with independently observed native team-result readers and per-game acceptance recipes
- test: verify unchanged 6000-frame state/audio, rollback stress, completed team-match streaks, 20 local pairs, recovery, native ownership, preserved player data, legacy wire and embedded icon resources
- feat: add explicit KOF97-only native hosting, arcade admission, Start prompts, player names, team-match streaks and same-peer recovery with a fixed 60-second retry window

## [2026-11-04] - In-game chat

- feat: chat with the other player during an online match (ForkAde's in-game chat): T opens a line (Y where T is a game key), Enter sends and Esc cancels, the lines shown at the bottom-left for eight seconds, Chat and Chat history in the in-game menu, each player's name in the hello outside the compared part, and each match's log with --chat-log; --nick, and --net-say and --chat-loss for tests.
- chore: sync recomp-common with ForkAde's fork at 1b18eac (the chat module, the font's Spanish letters, the menu bar and the launcher's CHAT card), and take the launcher's tunnel card from Description::tunnel.
- fix: read the in-game menu's speed as full speed until one is chosen, as recomp-common's menu now says -1 for it.
- test: KOF '97, KOF '98, KOF 2002 and Magic Plus II keep their offline frame hashes and audio; KOF '97 and KOF '98 chat online with the same frame hashes as without, every line once and in order, also with half the chat's packets dropped.

## [2026-11-03] - Player acceptance of KOF98, KOF2002 and Magic Plus II

- docs: record the player's acceptance of KOF98 (about three hours with nearly every team), KOF2002 and Magic Plus II, with sound and GekkoNet play correct and offline play to the ending, in their validation reports, tasks and the README.

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
