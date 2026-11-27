# Local acceptance — KOF family

6 November 2026; native Windows/UCRT64, G++ 16.2.0. OpenSpec was strictly
validated before implementation. Base `e3f334b821673161a6379542b91985fd7b268255`; local uncommitted
branch `codex/kof97-match-flow`. Source/executable fingerprints rechecked.

## Per-game results

| Set | Offline state/WAV | Stress | Full team observer | Local pairs | Recovered checkpoints (both / one-way) | Native owner |
|---|---|---|---|---|---|---|
| kof2002 | 6000 identical | 6000 PASS | 34000; streak 2 | 20 PASS (min 369) | 3689 / 3689 | 1316 matching |
| kf2k2mp2 | 6000 identical | 6000 PASS | 34000; streak 2 | 20 PASS (min 368) | 3689 / 3689 | 1308 matching |
| kof98 | 6000 identical | 6000 PASS | 34000; streak 2 | 20 PASS (min 368) | 3689 / 3689 | 1377 matching |
| kof97 | 6000 identical | 6000 PASS | 34000; streak 2 | 20 PASS (min 369) | 3689 / 3689 | 1419 matching |

Every set passed real-menu/unit result checks, unchanged embedded icon resources
(one group plus seven sizes), fresh arcade admission, stale-epoch rejection,
exactly-once pending chat, fixed 60-second recovery expiry, default legacy wire
in both host directions, and the isolated native backup/settings/Unicode/code/
Hide-toggle/Stop check. Each native owner test retained one synthetic tunnel
process; admission/recovery preserved its code and Stop ended ownership.

The observer uses natural full-team matches separately from rollback stress.
The read-only probes also established the opposite winner independently for
each new variant; see [native results](native-results.md). No character KO is
used as the team result. KOF97 was rebuilt and received the complete same
regression battery after the shared allow-list change.

## Reproduction and provenance

Maintained recipes: `tools/check_neogeo_flow.py --game <set> --forkade-root
<product> --baseline <reference.exe> --rom-dir <sets> --out <fresh-build-dir>`
and `tools/check_kof97_backup.py --game <set> --exe <exe> --fake-tunnel <fixture>
--seed-backup <isolated-65536-byte-fixture> --rom <set.zip> --out <fresh-build-dir>`.
Use one game recipe at a time (its cut tests use at most two tasks); native
owner port 7845 is exclusive. Builds are serial with at most three jobs.

Reference EXEs: `build/kof-family-baseline-20261106`; native reader probes:
`build/kof-family-reader-probes-20261106`; acceptance receipts, commands,
source/toolchain hashes and immutable copied EXEs:
`build/kof-family-acceptance-20261106/<set>` and `<set>-backup`. Local artifacts
containing game state, RAM, audio or screenshots are ignored and are not
public source/release assets. Machine-readable non-ROM receipts: [validation.json](validation.json).

| Set | Tested EXE SHA-256 |
|---|---|
| kof2002 | `e566cdb0fcffa272752ce865bec66c6737bdb45796a730378360e60142144f87` |
| kf2k2mp2 | `11b19f10b42606a5946bf3cc2cf382d71ec0ece77bd17b43ea8e103cd6293fad` |
| kof98 | `d4783eab2663f70746d35039a1d39ea9b44078301cc54b97766a1d779218abb1` |
| kof97 | `54d40f75daafd4a284db7306df38c52c4dd4f8b0cf232bb546e69fb735e5d62e` |

## Limits

These are loopback tests with a synthetic native tunnel process, not real
Iroh/Internet/player acceptance. No candidate or installed launcher was updated.
ROMs, protection/decryption layouts, settings, pins, vendor trees and Claude's
menu/training proposal were preserved. No commit, push or deployment.
The previous KOF97-only acceptance and its retained failures remain historical
records; these new receipts do not rewrite those runs.

## Final documentation checks

Strict OpenSpec validation passed for this change, neo-kof97-match-flow and
ForkAde forkade-lobby. Both repositories passed UTF-8 and git diff checks.
Production source and all four tested EXE fingerprints were checked again
after documentation updates. All owned game/tunnel test processes exited.

## ForkAde base for the KOF family — 27 November 2026 (Claude)

Branch `claude/kof-family-base` (worktree `D:/CPS2/neogeo-kof98-training`) from `claude/kof98-training` 022fc8f.

- Automatic replays and round markers: `runtime/kof_replay.h` gives KOF97/98/2002/Magic Plus II the two-human
  versus test KOF95 uses (BIOS 0x10fdaf at 2, both player modes at 1) and their bout clocks, observed natively in a
  scripted versus: 0x10a83a (KOF97, KOF98) and 0x10a7d2 (KOF2002, Magic Plus II), each starting at 0x6032 like KOF95's,
  so `port/replay_round_clock.h` serves unchanged. `port/neogeo_replay.cpp` no longer records a training session
  (both players count as playing there).
- `tools/check_kof_auto_replays.py` (`build/auto-replays/report.json`): each game recorded one replay of a scripted
  versus (KOF97 22001 bytes, KOF98 21703, KOF2002 19303, Magic Plus II 19285), played back to its end with no checksum
  difference and three round markers (KOF97 1896/6222/10451, KOF98 1626/5647/9738, KOF2002 and Magic Plus II
  2317/6156/10015); a one-player game against the computer recorded nothing. A KOF98 training session from boot
  (6515 frames with both players playing) recorded nothing (`build/train-norec`).
- `tools/check_neogeo_flow.py` per game against the programs built before (`build/baselines`): KOF97, KOF98 and
  KOF2002 PASS (41 cases each: offline 6000 frames and sound unchanged, stress, streak 2, 20 pairs, outages 3689/3689/
  3688, native owner 1381/1369/1318, the legacy wire). Magic Plus II's first run stopped at pair 3, both sides left at
  "Connecting" (kept in `build/flow-kf2k2mp2-fail1`); the rerun PASSED all 41. Not explained; watch it.
- Spectators through `cps-tunel`, relay only, 3600 frames (`build/relay-<game>`): players 3590/3590 each; spectator
  KOF98 2543/2543, KOF2002 2515/2515, Magic Plus II 2522/2522; chat both ways.
- Hidden characters: Magic Plus II's select screen already has a sixth column of six extra characters. A RAM sweep
  of KOF98's select screen (groups of 512 constant-zero bytes written 1) changed only the debug display, the cursor and
  the clock: Omega Rugal and the EX versions have no cell of their own, so this method cannot find them. KOF97's
  Orochi characters are the same kind. KOF2002 arcade has no hidden characters known to us; not verified in its program.

