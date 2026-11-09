# Design

## Context and boundaries
Base e3f334b on codex/kof97-match-flow. Existing NeoGeoOnline handles GekkoNet
and chat; the Neo Geo window has Esc menu but no native room owner. Claude's
neogeo-menu-and-features proposal is preserved; a full menu-bar migration and
training research remain separate. Add only the hosting entries needed here.

## Decisions
- Gate all new behaviour with --forkade-online AND GAME_ID kof97. Refuse other
sets explicitly. No new flow is activated in normal offline/default netplay.
- Adapt the verified CPS recovery socket/epoch barrier and presentation tracker
with origin credit; retain Neo Geo serialization/checksums and its chat screen.
Snapshot the clean board and game-source archive; both peers reset to it on
admission/recovery, clearing queued sound, pause, Start and presentation state.
- Same fixed 60-second retry budget, peer and room/epoch tagging as CPS. Keep
native GameRoom's admitted child/code throughout recovery; desync is terminal.
- Native optional FORKADE_BROKER_DIR links product headers, not a vendored
common edit. Restore the player's backup RAM on Stop, never persist online
backup/cheat changes. Service off and code copying remain available.
- Names/streaks are presentation-only, with rollback checkpoint history. A
completed team-match callback belongs in games/kof97/src, not the toolkit.
Fightcade's detector (kof97.inf: RAM A83B/DAF4/DAF6) provides facts to investigate,
not proof or copied detector code. Verify native bus addresses and full-team
outcomes with input-only play and held-result/debounce tests before enabling.
If not verified, keep streak acceptance pending rather than inventing wins.

## Validation
Save the pre-change EXE. Compare 6000 offline frame hashes and WAV bytes with
identical scripts/settings. Test 6000 stress frames, twenty local pairs with
latest SDK board/flow checkpoints (exclude final32 predicted frames), names,
independent Start, delayed challenger reset and legacy compatibility. Exercise
7-second symmetric/unilateral outage, stale epoch rejection, pending chat,
>=3600 recovered frames and fixed expiry; native menu uses a synthetic owned
tunnel fixture and local/off service. Internet and package acceptance remain
separate. Fresh output folders, max3 build jobs, max2 independent test tasks.


Native result observations (read-only probes B/C): the two Fightcade raw RAM
counter offsets map to bus bytes10DAF5/10DAF7, not10DAF4/10DAF6. The native
P1 counter advances at15980 and29738, after completed team matches; individual
health depletion at7409/11757 does not change it. BIOS_USER_MODE10FDAF=2 and
PLAYER_MOD1/2 at10FDB6/7=1 plus native fight phase10B092=1 identify a human
fight before the result. They were sampled in the native board, not assumed
from the documentation. Keep score history/arming in presentation checkpoints;
count a single counter increment, not a held positive value. Generic test-only
--memory-watch/--memory-log reads bounded work RAM without bus side effects.


Acceptance refinement: read-only stress probe D shows a real human fight with
phase10B092=2 and BIOS_PLAYER_MOD1/2=1. Phase1 alone was too restrictive; both
native-observed phases1/2 arm the score tracker. The first acceptance run's
zero streak is retained as a failed reader check; rerun against a fresh EXE.


Validation performance refinement: the combined34000-frame stress run exceeded
900 seconds on the interpreting Neo Geo runtime. Retain its timeout as not
passed. Acceptance separates6000 stress frames (including a human fight),
34000 normal headless frames through the SAME production flow observer (two
actual team results), and the tracker restore/debounce unit. --observe-flow
is an explicit headless/offline diagnostic; it neither writes game RAM nor
changes normal defaults. This avoids pretending an expensive uncompleted run
passed. Also reset the scripted menu cursor before semantic selections while
preserving slot/speed; the normal player's menu retains its cursor behaviour.
