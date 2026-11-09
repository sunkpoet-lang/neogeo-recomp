# KOF97-only local acceptance

Date: 6 November 2026. Source base e3f334b, branch codex/kof97-match-flow,
with the exact changed source manifest retained in the ignored acceptance
folder. See validation.json for compiler, EXE/reference/recipe hashes and
per-case results. Final EXE: build/kof97-flow-acceptance-c-20261106/kof97.exe.
No installed collection, source pin or vendored common was changed.

## Passed
- 6000 offline per-frame hashes and WAV bytes exactly match the pre-change EXE.
- 6000 rollback stress frames include a recognised human fight. Pure controller
 tests cover team-win debounce, opponent wins and save/load of presentation.
- 34000 normal headless frames through the SAME production flow observer contain
 two complete human team matches and streak 2. This is not 34000 stress frames.
- Twenty isolated opted-in loopback pairs pass with equal latest SDK board AND
 presentation checkpoints, excluding the last 32 prediction frames. A delayed
 challenger discards the host's solo progress and matches the guest's start.
- Symmetric and one-way 7-second outages each recover 3689 matching checkpoints,
 over 3600 frames after reset. Recovered initial states match the original start;
 stale epoch packets are rejected and pending chat is received exactly once.
- Permanent interruption ends after the fixed 60-second recovery window (69.78s
 total includes startup, initial frames and detection). Both peers exit3.
- The actual hidden window owns one synthetic tunnel across recovery (1383
 recovered matching checkpoints), retains its admitted room and stops it.
- The supplied backup bytes, game settings and Unicode launcher preferences
 survive Host/Copy/Hide toggles/Stop unchanged in a separate isolated profile.
- Default legacy netplay connects to the saved EXE in BOTH host directions.
- All 8 embedded icon resources (group plus seven images) equal the baseline.

## Native result evidence and provenance
Fightcade's raw detector facts were used only as investigation leads:
https://github.com/fightcadeorg/fightcade-detectors/blob/main/kof97.inf.
No detector implementation or Lua code was copied. Native probes demonstrate
that raw offsets DAF4/DAF6 map to bus bytes 0x10DAF5/0x10DAF7. Probe C records P1
increments at 15980 and 29738; individual teammate defeats at 7409/11757 do NOT
increment the team counter. Probe E records the opposite player's increment
at 15789. BIOS mode/player flags and active phases 1/2 were read in the native
runtime. BIOS field documentation:
https://wiki.neogeodev.org/index.php?title=BIOS_RAM_locations&oldid=9321.
The game-specific callback stays in games/kof97/src/match_score.h. No game RAM
is written by the result reader or the presentation tracker. KOF97 owns no
separate Game::state callback; the unchanged board archive covers its state.
Names, joined flags, score history and streaks remain outside board checksums,
with their own rollback checkpoints. Recovery and the socket envelope adapt
the credited ForkAde CPS implementation; no common/vendor tree was edited.

## Retained failures and refinement
The first input probe remained in attract because credits arrived too early;
it is not human-play evidence. A later stress probe showed phase 2 is a valid
human fight, fixing the first reader's phase 1-only guard. The first acceptance
was stopped after that defect was identified. The next combined 34000-frame
stress exceeded 900 seconds and remains a non-pass. Validation now separates
6000 stress, 34000 normal observed frames, and presentation restore tests. A
menu-injection unit exposed cursor retention; injection now resets its own
navigation while preserving speed/slot. The player's normal menu retains its
usual cursor behaviour. Final acceptance uses a fresh output folder; earlier
folders and failure logs have not been overwritten or deleted.

## Reproduce and limits
Build only KOF97 with FORKADE_BROKER_DIR pointing to the maintained ForkAde
product. tools/check_kof97_flow.py --help specifies the CPS-derived recovery
fixture owner, baseline, ROM folder and NEW output directory. Backup/menu
preservation is reproducible with tools/check_kof97_backup.py --help.
Raw state/audio/PNG/EXE/RAM observation files stay in ignored build outputs,
never source commits or public packages. Public evidence is this summary and
validation.json, not raw game or player data.

Loopback acceptance only: the native owner uses a synthetic tunnel and does
not prove Iroh/game end-to-end or Internet play. No KOF98/KOF2002/Magic Plus II
acceptance, menu-bar migration, training implementation, service deployment,
new installed beta, commit or push. ForkAde's aggregate 5.1/5.2 stay pending;
only the KOF97 child tasks may close. User/friend acceptance remains 6.2.
