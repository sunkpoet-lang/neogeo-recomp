# Changelog

**Note:** All dates are in Mexico City CST (UTC-6).

<!-- markdownlint-disable MD013 MD024 MD022 MD032 -->

## [2026-11-27] - KOF98 training combo counter and Neo Geo training adapters

- docs: record the KOF2002 Orochi versions code (hold Start, then choose Yashiro, Shermie or Chris) in the README as checked with scripted input, with images/orochi.png comparing the screen with and without Start
- docs: add a hidden characters section to the KOF2002 README with the Kusanagi code, checked with scripted input and shown in images/kusanagi.png, plus the unchecked Orochi versions and the Omega Rugal note
- docs: record the KOF family's ForkAde base in neo-kof-family-match-flow: automatic replays, rebuilt acceptance, relay spectators and the hidden-character findings
- feat: record two-human versus matches automatically with round markers in KOF97, KOF98, KOF2002 and Magic Plus II (their bout clocks observed natively), and keep training sessions out of automatic replays
- docs: record the KOF98 combo counter, refills, meter and input history in the neogeo-training change, with the evidence and what remains from fbneo-training-mode
- feat: add a KOF98 training combo counter with last-hit and total damage, an after-combo health refill, a meter-always-full option and a facing-relative input history
- feat: add Codex's KOF2002 and Magic Plus II training adapters (hitboxes, random guard, recovery response), KOF98 random guard and recovery response, read-only RAM observation and the input display changes

## [2026-11-26] - KOF95 match flow and boss unlock

- docs: record KOF95's native result reader, ForkAde match flow, spectators, relay fix and boss unlock evidence in the neo-kof95 change (native-results, validation, tasks)
- feat: open Saisyu Kusanagi and Omega Rugal in KOF95 (the game's own flag set before every frame; the arcade program has no code for it), with a Team Edit capture and README notes
- feat: add KOF95 to ForkAde's online match flow: a native result reader (the loser leaving play), streaks, spectators, chat and names; acceptance tools for spectators and the tunnel
- fix: let Neo Geo spectators watch through the tunnel's relay: GekkoNet sends a joining spectator's state at a paced rate instead of all at once every 200 ms (tools/patch_gekkonet_neogeo.cmake)

## [2026-11-24] - Training development evidence and coverage

- docs: consolidate the local KOF98, KOF2002 and Magic Plus II training work, native demonstration captures, save compatibility, independent validation and remaining publication gates; distinguish development candidates from the published source
- docs: record sampled projectile geometry for Athena, Terry, Kensou and Andy across player roles and initial facings, with per-set evidence and unchanged accepted executable hashes

## [2026-11-20] - Native replays and KOF98 practice

- docs: document bring-your-own-ROM builds, actual demonstration captures, source provenance and the limits of local validation; retain original credits and exclude private test payloads
- test: compare KOF98 player and projectile rectangles with the original Lua viewer on native RAM snapshots, exercise bounded object traversal and preserve state/audio with the inspector enabled
- feat: add native KOF98 offline practice, bounded dummy recording slots, deterministic playback, read-only collision inspection and recorder-style current-input panels above the super meters
- feat: add automatic local and confirmed online replay recording, source compatibility checks and a native replay viewer, with rollback and local-peer validation kept distinct from Internet acceptance
- feat: add original KOF95 MVS cartridge assembly, its own title-screen icon, build notes and native local-play candidate

## [2026-11-10] - Online strip

- fix: a stranger's garbage hello no longer ends the host's match (taken as the peer only if it is this game's; refusals once a second per address; pending hellos capped); checked with 455 000-515 800 hostile packets per KOF '97 match, both sides equal
- docs: the README and the launcher's keys say the strip, the rows, Tab, [ ] for speed and - = for volume
- feat: names and win streaks in one see-through strip at the top (player 1 blue, player 2 red), the ping, delay and rollback one a row at the top right in a small yellow font, and Tab online to show or hide them; offline frames unchanged
- chore: recomp-common synced to 4a34db7 (the palette and small font, Tommy's languages and accented font, the launcher's volume slider)

## [2026-11-10] - Speed keys

- fix: [ and ] change the speed from the start, before the in-game menu was ever opened; offline frames unchanged

## [2026-11-09] - Sound level

- feat: a sound level from silent to full in 5 percent steps (the in-game menu's Sound row, - and = or the keypad's - and +, --volume N), kept per game in settings.ini; full level sounds as before; the speed keys move to [ and ]

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
