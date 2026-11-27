# KOF98 native training first slice — 20 November 2026

Local uncommitted work on `codex/neo-kof95`; existing changes and user archive
preserved. Candidate SHA-256:
`6dd08137357ce9a4a6b0a090f6af1bc1135896fba976b216612c720b91523259`.
State/gameplay identity:
`cd4e8fe6010a80599d477fd75e0117dff53f1e3fb7e5805793270e97b0c44f5d`.
The final build's generated identity and source receipt remain private. No cross-build replay compatibility is
inferred from matching board hashes.

## Native observations
The original set was already checksum-validated by the loader. Read-only native
traces inspected MVS mode/participant flags, battle phases, health, clock, facing,
position and meter. Both facing directions were observed by a scripted jump and
side switch. Resource writer values were tested in the executable; meter is
displayed only. The first draft accepted phase2 alone; an actual fight in phase1
exposed that mistake, and the final predicate accepts both observed phases with
resource/participant guards. No game phase or character-selection memory is
forced. Coin/Start and P1 mirroring drive the ordinary selection screens.

The sources independently implement native hooks; no willococa/peon2 Lua code or
artwork was imported. Their URLs are recorded in design/game documentation as
research leads. Pinning complete inherited licences remains open before any
future code reuse. The old baseline already emits a watchdog reset and one
unmapped-access diagnostic; these are present in the unchanged control run, not
claimed to be new zero-diagnostic acceptance.

## Final executable tests
- Training off: all 8,000 board hashes and the complete WAV match the retained
  pre-training executable. Packaged Windows-only-PATH run repeats all 8,000 hashes.
- Training on: ordinary entry reaches a two-human fight; 4,840 observed battle
  frames retain living-fighter health103 and clock0x6000 with aids enabled. Dummy
  low guard follows the observed facing flag. Physical-controller guard/drill
  acceptance remains pending.
- Save at5,000, cold load in another process: all3,000 subsequent board hashes and
  audio samples match with the same feature choices. Training counters/history
  and first-round checkpoint are serialised.
- Backspace reset at tick6,200 restores frame3,162; all4,839 following states match
  their first traversal. Headless frame labels now follow restored board frames.
- Real menu hooks start training and enable its aids; End training stops clock
  writes. These are program hooks, not desktop clicks or physical controller tests.
- A truncated slot load at6,200 leaves all board hashes AND the final complete
  board/training state byte-for-byte identical to the control. The port now backs
  up/restores `Game::state` along with the board when loading fails.
- Online mutation guard:300 forced-rollback frames with training preferences on
  match the off control. Replay mutation guard:600 frames match linear truth even
  when the replay header retains enabled training preferences. Recording while
  training is active is refused; dummy recordings are not implemented.
- CTest8/8 passes, including bounded optional-state allocation/preflight, unchanged
  fixed-layout checks, replay codec/confirmation, decryption/layout/startup.
- Rebuilt KOF97 retains its previous1,200 hashes and full WAV. Both OpenSpec
  changes validate strictly; whitespace checks pass.

Optional application checkpoint vectors use an explicit1MiB bound. Fixed chip RAM
vectors retain their exact-length preflight. This is structural validation, not
a comprehensive semantic fuzz audit of native chip states. Save/load uses the
same training feature choices; feature preferences are not silently embedded as
new global state policy.

## Delivery and remaining gates
The separate local candidate is `work/kof98-training-ready`, with practice and
normal-play BAT files, its seven-size embedded game icon and runtime DLLs. ROMs
are read from the user's common folder; preferences remain in its own `user/`.
No installed beta/launcher, ROM/BIOS, credentials or firewall were modified.
Actual offscreen training/menu screenshots are visually inspected and have
executable/image hashes in `games/kof98/images/training-provenance.json`.

Private receipts: `work/kof98-training/native-before`, `facing-before`,
`accept-final`, `controls-final`, `bad-state`, `package` and
`work/kof97-training-regression`. Earlier failed drafts remain separately. States,
RAM, WAV and replay payloads are private and are not public release assets.

Pending: human training/controller acceptance, all other KOF variants, MAX refill,
random guard, dummy slots/reversals/recovery, hitboxes/frame data, catalogue and
launcher promotion, real Internet requalification. Current HUD directions are
absolute screen numpad directions. Training does not record match replays yet.
No commit, push, deployment, new dependency download or resumed Sunset work.


## 20 November 2026 — dummy slots and current-input panel

This section supersedes the earlier pending dummy-slot/history-display notes.
The user reported that the first training slice works; acceptance of this new
slot UI and new artwork is still pending. KOF98 executable SHA256:
`95a5364dd113e00745fa19ab924cd21264e8d96027c4c6afecbe0dd9a6af5fec`.

- CTest 10/10 passed, including bounded recording/state tests and panel sprite,
  direction, simultaneous-button, guard-buffer and player-isolation tests.
- Native harness linked the exact production object list (not a replacement
  implementation): record120, mid-record restore60, third slot20, once120,
  loop245, random restore300;120 observations of each native facing. Ordinary
  gameplay generated the opposite-facing fixture. Truncated new slot state is
  rejected and leaves the complete board/training state byte-identical.
- Online stress300 and replay-viewer120 exercise the no-injection guard with a
  populated library. These are local checks, not Internet/player acceptance.
- Actual EXE cold random playback continues200 frames exactly against the native
  harness; two fresh process runs have identical hashes and WAV.
- All five KOF targets built sequentially with the cached local GekkoNet source.
  For each, panel-enabled1,200-frame hashes and complete WAV equal its retained
  pre-change executable. Seven embedded icon sizes verified in every candidate.
- Original InputDisplay PNG blobs/SHA256 and pinned source revision are recorded
  under assets/input-display; RGBA embedding is lossless. No InputDisplay
  executable, .NET runtime or UTM code/assets were imported.
- Actual offscreen GPU/menu captures were visually inspected. They show current
  inputs only; the legacy history fields remain in saved state but are not drawn.

Private recipes/results: work/neogeo-input-accept (commands, full logs, summary,
delivery), work/kof98-slots/native-final-input, and work/neogeo-input-family.
Earlier failed drafts are retained. Never publish the state/replay/WAV payloads.
Updated isolated practice package: work/kof98-training-ready; retained previous
package files in work/kof98-training-ready-before-slots, with user/ unchanged.
Separate normal-play panel candidates: work/NeoGeo-Input-Panel-candidates.
No installed launcher/beta replacement or new Internet qualification. Source
changes can change replay identities; peers/viewers require compatible builds.
Remaining: MAX, random guard, reversals, hitboxes/frame data, per-set training
adapters, new UI human acceptance and separate Street Fighter presentation.


### Compact presentation revision
User confirms the panel/slots work, requests half-size boxes. The shared renderer
now compacts the input boxes from116x36 to58x18 pixels and restores the uncovered
original game picture. P1/P2 labels are redrawn using a legible3x5 font. Resource
headers move to50x9 blue/red-accented badges above the character portraits,
outside the life bars. Other HUD elements remain unchanged. This is a general
fighting-game design rule, but resources require individually verified readers;
only the KOF98 training adapter currently supplies these numeric values.
Focused panel test passes,
including exact half-scale samples, restored background and player isolation.
All five frontends rebuilt;1200-frame hashes/WAV retain the prior reference.
Native slot/state/guard harness repeated with the exact new production objects.
Private evidence: work/neogeo-input-portrait-accept, neogeo-input-portrait-family,
and kof98-slots/native-portrait. Updated separate candidates; user files retained.
The earlier binary hashes describe the earlier full-size display. Current capture
and executable hashes are in games/kof98/images/dummy-provenance.json.


### Native crisp controls revision
User found the scaled art unclear. Active rendering is now original native pixel
geometry/letters, not decoded or resampled InputDisplay sprites.90x22 boxes have
no P1/P2 labels and sit over reserve-team names in training (y42 at224 lines).
HP/POW stays above portraits. Explicit On in normal play retains lower placement.
Directional mapping, effective-input isolation, guards, tiny surfaces and raised
panel bounds pass the focused renderer test. All five targets rebuilt; their
panel-enabled1200-frame hashes/WAV equal retained references. Two cold random
dummy continuations of200 frames remain exact; presentation changes do not alter
the slot implementation. No new Internet test or launcher integration claimed.
Actual EXE screenshots were inspected, including a real scripted up/right+A+B
frame3623 and the training menu. Initial snapshot scripts used the wrong frame
(0 or3622), produced neutral input and were not accepted as active-input evidence;
the script is one-based and3623 produced the expected changed hash and controls.
Private evidence: work/neogeo-input-crisp, work/neogeo-input-crisp-accept and
work/neogeo-input-crisp-family. Updated separate candidates, preserved user/ and
retained work/kof98-training-ready-before-crisp. Current capture/EXE hashes in
games/kof98/images/dummy-provenance.json. Earlier art/size notes are historical.


### Arcade button finish
User supplied their earlier arcade mock; its CSS was read as visual reference.
Native integer shading adds a raised lip, lower shadow and upper-left reflection,
with brighter pressed caps. No CSS/script/font or metadata was imported. ABCD
colours, letters, neutral/arrows, layout and current-only behaviour are unchanged.
The focused renderer test passes; all five executables rebuilt. Actual offscreen
captures were inspected; scripted diagonal+AB frame3623 retains c195bc58 and
neutral frame3623 retains db68636d from the previous appearance. This cosmetic
revision does not repeat or newly claim the earlier full gameplay/Internet tests.
Private receipts: work/neogeo-input-arcade and neogeo-input-arcade-family. Candidate
user/ unchanged; prior top-level files retained in training-ready-before-arcade.
New capture and EXE provenance replaces the previous appearance's image receipt.


### Existing per-system layout reused
The user's actual recorder defines neogeo4 as the MVS arc (A low-left, B/C/D
above) and sf6 as staggered Astro City rows. The native four-button adapter now
uses the MVS arrangement in90x32 boxes. SF's six-button arrangement is documented
for its separate integration; no six-button mapping is enabled in Neo Geo.
Neo Geo colours, white-arrow directions, current-only input and arcade cap
shading remain. The focused renderer test passes, including raised panel bounds
and player isolation. All five executables rebuilt. Actual offscreen captures
were inspected; active/neutral frame3623 retain c195bc58/db68636d respectively.
No new full gameplay or Internet claim. Private receipts: work/neogeo-input-mvs
and neogeo-input-mvs-family. user/ unchanged; prior package files retained in
training-ready-before-mvs. Image provenance updated to the exact current EXE.


### Actual recorder artwork and presentation-resolution composition
The user explicitly requires the same style, not another shader approximation.
Generated17RGBA assets with the actual recorder Pintor._boton/_bola and a crop
of its _base stick gate, preserving its original palette/font/pixels. AST extraction
executes only those reviewed drawing methods, not the recorder module/service.
PNG/source/font hashes and OFL notice retained. Runtime uses existing zlib only.
The recorder's ball-top joystick supersedes earlier N/arrow artwork.

Input presentation expands the native picture3x then composes original controls
there. Interactive GPU/SDL and headless GPU capture share it; normal menu layers
stay above it and the Esc-menu path bypasses detailed recomposition. No mutation
of the board, effective pads or audio. The focused test validates17asset decodes,
guards, directions, player isolation, raised placement and expanded-buffer bounds.
GPU screenshots of active controls/menu were inspected. The initial generator
missed AST node line numbers; fixed with fix_missing_locations before generation.

All five targets rebuilt; panel-enabled1200hashes and WAV equal retained
references. Cold random dummy continuation200frames and two-run WAV remain exact.
No new real Internet claim or interactive GPU/SDL human acceptance. Runtime CPU/
upload cost of the3x visible-control picture still needs user-window review.
Native raw224-line screenshots intentionally remain low resolution.

Fightcade local inventory and Neo Geo scrolling-input atlas were inspected as
additional reference; no Fightcade asset/code imported. SF integration remains
separate. Private evidence: work/neogeo-input-exact-accept, exact-family and
exact-final. Updated isolated candidates; user/ and installed beta preserved.
Source changes can alter conservative replay identities: compatible builds needed.


### Accepted recorder appearance: smaller and lightly transparent
User accepts the recorder style, requests20–25% reduction and10–15% transparency.
Implemented25% reduction, with integer pixel rounding, and224/255 opacity
(12.16% transparency) across the composed panel. Original assets, colours, MVS
layout and outer/top anchors stay unchanged; portrait HP/POW is unaffected.
The detailed presentation restores the underlying board picture in the native
control region before compositing, preventing duplicate opacity or ghost boxes.
Focused renderer tests pass: known border/background alpha result, equality of
clean and previously painted input, reduced bounds, player isolation and guards.
All five targets rebuilt. Actual GPU active/menu screenshots inspected; active/
neutral frame3623 retain c195bc58/db68636d. No new full gameplay/audio/Internet
claim. Evidence work/neogeo-input-soft and soft-family; isolated candidates and
user/ preserved, prior package retained in training-ready-before-soft.


### Bottom anchor above the super meters
User accepts size/transparency and requests controls immediately above super
meters as the fighting-game layout convention. Anchor top174 at224 native lines,
with24-line controls and26 lines reserved below. Both training and normal On use
this placement. Original art,25% reduction,224/255 opacity and portrait resources
stay unchanged. Native/detailed presentation share the anchor and restoration.
Focused bounds/blending/player-isolation tests pass. All five targets rebuilt.
KOF98 GPU capture visually confirms clear super meters; active/neutral frame3623
retain c195bc58/db68636d. Other HUD variations still require per-game review.
No new full gameplay/audio/Internet claim. Evidence work/neogeo-input-meter and
meter-family; user/ preserved, prior package retained in training-ready-before-meter.


## KOF98 read-only player-box slice —20 November2026

Independent fixed-address reader in games/kof98/src/hitboxes.h; no pointer-list
traversal or guest RAM writes. A const-board presentation inspection hook produces
fresh rectangles even when paused/loaded/reset. Active offline KOF98 session and
verified MVS battle phases gate output; outside session/online/viewer clears it.
New native Hitboxes page offers per-type controls and body show/hide commands.

- Actual installed original Lua (hash91db43f2...) executed using its existing32bit
  Lua DLL against120 private native RAM snapshots; reference writes are rejected
  and throw hook callbacks are not invoked.730 body rectangles agree exactly:
  426hurt,32attack,32guard,240push. Both facings for both players,22 camera
  positions, P1Y131–220. This is body-reference agreement, not complete collision
  engine/roster/projectile/throw certification.
- Exact production-object native harness: reading/drawing leaves every sampled
  board hash unchanged. Paused load and reset show fresh geometry. Explicit
  online/replay flags and session exit clear overlays. Actual Internet not tested.
- Final executable900-frame old/off/on training hashes and complete WAV match;
  enabled/off preferences match300 local forced-rollback frames. Rebuilt KOF97
  retains1200 hashes/WAV. CTest11/11 includes independent signed-coordinate,
  facing, invalid-ID, transient attack-slot, phase and range checks.
- Actual GPU game/menu captures visually inspected, original executable icon
  retained. Isolated training-ready candidate updated with a new hitbox BAT;
  user/ unchanged, prior files in work/kof98-training-ready-before-hitboxes.
- Initial32bit compiler attempt showed a missing libwinpthread dialog. The owned
  test process was closed; existing opt/i686 runtime was supplied only through
  the child compiler PATH. No system DLL copying/install/global PATH change.
  Reference runner now captures child failures without desktop error dialogs.

Aggregate source-only receipt: hitboxes-receipt.json. Private commands, RAM,
states and WAV: work/kof98-hitboxes, kof98-hitbox-accept, kof98-hitbox-menu and
kof97-hitbox-regression. These payloads never belong in source/releases.
Native GUI expansion, projectiles/throws, broader moves/roster, other game
adapters, human review and new-build replay/Internet qualification remain open.

## Projectile slice — 20 November 2026

Independent bounded native traversal matches the original Lua on37 Yuri-scene
RAM snapshots (291 rectangles, including73 projectile rectangles). Native
inspection remains read-only across160 frames, paused load and display toggles.
Old/off/on900-frame hashes and WAV match;300-frame rollback stress matches.
Full CTest11/11 passed. See [aggregate receipt](projectiles-receipt.json).
No new Internet acceptance, throw capture or full roster coverage is claimed.

## Private throw-capture investigation — 20 November 2026

A separately linked, instrumented board observes writes after the real byte
write at 0x10EBDF or word write at 0x10EBDE. Production sources and the delivered
candidate are unchanged. A4 accepts only the two known player pointers. Nine P1
events with nonzero throw IDs were captured in a native 1,200-frame run. All
frame hashes match the uninstrumented execution with identical state and inputs.
An independent geometry calculation agrees on all nine rectangles with the
actual original Lua `set_wp` callback. The Lua write fallback is never invoked.
Private RAM/CPU-context data remain under ignored work directories.

This is research evidence, not a released throw viewer: P2/facing, lifetime,
paused load/reset/rollback clearing, audio and production integration remain
separate gates. No throw switch is enabled in the menu by this investigation.

A second independent P2 run captures eleven further events; all eleven also
match the actual original Lua callback and all 1,200 instrumented frame hashes
match its uninstrumented baseline. Total sampled coverage is twenty events
across both players, not all characters or throw types. Both initial facing
directions are exercised by the two players; side-switch coverage remains open.

## Native throw-attempt candidate — 20 November 2026

The production `ReadThrow` matches all twenty original-Lua event rectangles.
The real native game/board/port objects display eight P1 and nine P2 event frames
in the final lifecycle test. This checks expiration, repeated paused display,
live-event online/viewer/session exclusion, switch-off clearing, state-load and
round-reset invalidation, byte/word delivery and observer detachment. Test setup
stops inherited dummy playback; P2 uses the existing input-only recording route.
Captures are presentation-only and excluded from machine state serialisation.

Old/off/on 900-frame hashes and full WAV match; 300-frame rollback stress with
training preferences enabled matches its disabled control. KOF95, KOF97, KOF2002
and Magic Plus II each retain 1,200 frame hashes and WAV after the shared board
change. Full build and CTest pass 11/11. GPU captures use actual event frames in
a native harness linked to the exact production objects, not synthetic art.
See [aggregate evidence](throws-receipt.json). No new Internet acceptance,
throwability, complete roster or side-switch/aerial/command-throw coverage claimed.

## Human feedback and six-section navigation

The user confirms the throw candidate works. This is human acceptance of the
tested candidate, not full throw-roster coverage. The next slice reorganises
existing controls without changing their IDs, native command pairing, bindings,
state schema or algorithms. Seven offscreen native menu captures cover the
Training root and all six sections. A linked native check confirms the nine
command IDs retain their order. Old/off/on 900-frame hashes and full WAV remain
equal against the accepted throw candidate; 300-frame rollback stress also
matches with the training preferences enabled/disabled. Additional HUD placement
controls and advanced unverified capabilities are not added by this change.

## Optional HUD controls — 21 November 2026

Offline KOF98 now has input visibility/anchor, status-text and portrait-resource
choices plus a scoped restore command. Native checks cover paused refresh,
local/global precedence, reset without touching dummy/slots/hitbox preferences,
unchanged machine and recording-state bytes, and ordinary display restoration
outside practice. Renderer tests cover alternate/clamped anchors and single
alpha blending. The default GPU PNG is byte-identical to the accepted six-section
candidate's capture. Old/off/on and customised HUD 900-frame hashes/WAV match;
300-frame rollback stress matches. Full CTest passes 11/11. KOF95/97/2002/Magic
Plus II each retain 1,200 frame hashes and WAV with the shared rendering update.
See [aggregate receipt](hud-receipt.json). No draggable layout or new gameplay
adapter is claimed. Human review of the optional layouts remains separate.

## Ordinary throws after crossing — 21 November 2026

An existing native state reached through ordinary jump/crossing inputs (no RAM
pokes) exercises the opposite player facings. Production observer off/on runs
retain all 1,200 state hashes and raw audio samples for each player. Nine P1 and
ten P2 valid events match both production ReadThrow and the actual original Lua
callback. Combined with the original twenty events, sampled coverage totals39
rectangles, with each player observed facing both directions across the scenes.
This proves the sampled orientations after crossing, not a continuous full-round
crossing movie, all characters, aerial/command throws or throwability areas.
No runtime fix or executable replacement was required. The existing HUD
candidate remains unchanged. See [aggregate receipt](throw-sides-receipt.json).

## Character-label correction and command investigation — 21 November 2026

The earlier projectile scene was incorrectly labelled Iori. Its captured game
HUD identifies Yuri. Corrected local documentation and the aggregate receipt;
geometry counts, original-Lua agreement, source hashes and accepted binaries are
unaffected. Historical private directory names are retained as execution evidence
and must not be interpreted as character identification.

Two batches of A-only candidate directional patterns and forward+A controls
produced no qualifying throw events in the original/opposite fixtures. Each
1,200-frame observer on/off pair retains hashes, audio samples and motion logs,
but this is not functional command-throw acceptance. Selection experiments also
ended with Kyo/Yuri, so these states cannot validate an Iori-specific move.
The user will create a manually confirmed Iori state with F5. Wait for their
completion notice before using it. No runtime patch, candidate replacement or
new command-throw claim results from this investigation.

## Confirmed Iori command-throw sample — 21 November 2026

The user supplied F5 state contains Iori at team-order selection. Copied it,
completed order with ordinary A inputs and waited for the fight, preserving the
original file hash. Native VS/game captures confirm Iori P1 against Brian P2.
The prior Yuri fixtures are not used to identify this movement.

Walking to range, releasing to neutral and entering two directional variants
ending in A produce ten events each. The forward+A control produces zero.
No C/D buttons occur in these traces; Brian's HP remains103 while relative
positions and facings swap. Both actor facings occur in each successful run.
Production ReadThrow and the actual original Lua callback agree on all twenty
event rectangles. Each 1,200-frame observer on/off pair retains every state hash,
audio sample and motion-log entry. A production-object native harness displays
ten yellow event frames without changing the board hash during inspection.

The measured variants are forward/down-forward/down/down-back/back/forward and
forward/down/down-forward/down/down-back/back/forward, relative to current facing.
Each direction lasts two frames; the last forward is held two frames before A.
These are successful recorded input recipes, not universal move-parser advice.
Earlier no-neutral/out-of-range attempts are retained as failed qualification.
No new runtime fix, executable replacement or Internet acceptance is claimed.
See [aggregate evidence](iori-command-receipt.json). Aerial/other command throws,
throwability and full roster qualification remain open.

## Random standing guard — 21 November 2026

Dummy choice4 preserves existing mode values0-3. A saved xorshift decision chooses
relative back or neutral at the start of a P1 input/body-attack episode, held
through thirty quiet frames. Native Iori/Brian trials produce twelve guard and
twelve pass decisions, with damage totals0 and96 respectively over24 reset trials.
The input expectation matches the saved decision; this is a standing-guard input
exercise, not universal low/projectile/reversal coverage.

Native acceptance verifies200-frame exact continuation after save/load, paused
inspection without random consumption, reset retaining seed, slot record/play
priority, import of actual training marks54/55, and four malformed guard-state
cases atomically restoring prior board/game bytes. New mark56 bounds seed,
quiet counter and boolean encodings. Synthetic tests independently cover
episode retention, deterministic continuation and malformed/truncated fields.

Default old/off/on900-frame hashes/WAV and GPU PNG remain identical. Two random
mode CLI runs reproduce all1,200 hashes and WAV; random-enabled300-frame rollback
stress matches the guarded control. Full CTest passes12/12. This change is scoped
to KOF98 game sources; no shared board/render change or other-game adapter is
introduced. See [aggregate receipt](random-guard-receipt.json). Human acceptance
of random guard and broader move coverage remain pending.

## Explicit random low guard — 21 November 2026

The user confirms the standing random-guard candidate works. Value5 now adds a
separate back+down posture using the same mark56 episode/seed; values0-4 remain
unchanged. Native tests preserve the decision/seed while switching posture inside
an active episode. In24 corner/reset trials, twelve low-guard choices accumulate0
damage and twelve pass choices accumulate36. Repeating the guarded trials from
the identical pre-attack states with standing guard accumulates36 damage. This
qualifies the sampled crouching attack, not automatic height detection.

The native suite also passes pause,200-frame continuation, actual54/55 migration,
recording/playback priority, four atomic malformed-state cases and online/viewer
input guards in low mode. Default900-frame hashes/WAV and GPU image are unchanged;
standing mode old/new and low-mode repeated1,200-frame hashes/WAV match. Low-enabled
300-frame rollback stress matches its guarded control. Full CTest passes12/12.
See [aggregate receipt](random-low-receipt.json). No shared board/render change,
new state schema, other-game training adapter or Internet acceptance is claimed.

## Recovery-signal research — 21 November 2026

The user confirms the explicit random low-guard candidate works. The next research
step uses real Iori/Brian idle, standing-hit, sweep and blocked-sweep inputs. Each
240-frame observation window retains identical hashes and audio samples with
object logging on/off. Measured damage is0/9/12/0 respectively. The byte used by
the installed reference as a hitstun proxy is nonzero for0/30/84/0 frames. These
are scene-specific signal observations, not first-actionable or reversal windows.

Single-frame A, down+A and down+C response experiments do not qualify timing:
the idle positive controls fail to register a qualifying body attack. Raw traces
and failed-calibration status are retained privately. In particular, the apparent
responses in some guarded trials must not be published as exact recovery timing.
An independent input/action/pose detector must pass the idle control before an
automatic reversal is exposed. No production source or candidate changed in this
research step. See [research receipt](recovery-research-receipt.json). The current
random-low-guard candidate remains the last delivered executable.

Independent idle controls establish that P2 inputs work: a four-frame jump moves
Brian from minimum Y213 to179, while A and C deal8 and12 damage to P1. Both the
player-body reader and full-scene reader report zero qualifying attack boxes in
these160-frame trials. This is a false-negative response detector, not evidence
of a broken controller or inability to act. Full-scene observations do not infer
ownership. Event-time or animation evidence must independently qualify a response
detector; neither the reader nor gameplay is changed on this evidence alone.

### Transient event follow-up — 21 November 2026

A private observer installed after the game's beforeFrame hook watches P2 status
writes at0x10837c. It sees one qualifying attack event for A and two for C, with
zero for neutral and jump. These sampled attacks expose boxes during the CPU
frame although the end-of-frame readers miss them. The initial diagnostic was
installed too early and overwritten by the production throw observer setup; its
zero-event result was invalid and rerun with the explicit hook order. Production
observer and CPU code remain unchanged.

All four160-frame controls retain identical observer on/off hashes, raw audio
samples and P2 object bytes. Four-frame A requests at eight sampled ticks all
produce an attack in idle controls; each no-request negative stays clear.
After standing hit, requests24/40 produce no observed response, while48 and later
sampled requests do. After sweep, sampled requests24..100 produce none, while
108/120 do. After blocked sweep,24/40 produce none despite a zero reference
counter, while48 and later sampled requests do. These are relative probe ticks
and a bounded response witness, not exact actionability, frame advantage or a
certified reversal window. Input buffering, startup, further characters and
independent wake-up semantics remain to be qualified. Task3.2b2b stays open;
the accepted executable is unchanged.

### Input duration and adjacent response probes — 21 November 2026

Thirty-two paired requests (eight ticks in each of four scenes) yield identical
response/onset/damage results with A held for one or four frames. All idle requests
pass. Eighteen additional single-frame requests delimit sampled transitions:
standing-hit requests40/41/42 fail and43/44 respond; sweep100 fails and101/102
respond; blocked-sweep40..43 fail and44 responds. All twelve no-request controls
remain negative. These ticks are relative to the private probe, not universal
recovery numbers. A transient response is possible while the reference counter
remains nonzero, and blocked recovery can reject an input with that counter zero.
The counter alone therefore cannot determine the desired trigger.

Matched private P2-object traces separately record the first divergence from the
no-request control. That byte-level divergence is not yet independently identified
as animation start or input acceptance. Nor does equal A1/A4 behaviour certify
command-motion buffering. See [duration/boundary receipt](recovery-buffering-receipt.json).
No new runtime option is exposed;3.2b2b remains open for semantic recovery and
buffer qualification before automatic reversal. The accepted candidate is unchanged.

### CPU sampling and conservative recovery signals — 22 November 2026

A private copy of the board implementation logs P2-register reads by the68000.
In idle, hit, sweep and blocked-sweep controls, A is sampled in the same host tick
that supplies it; matched no-request controls have no A samples. This separates
host input delivery from subsequent game processing without establishing the
precise point where the game accepts a move. The instrumented board is confined
to the private harness; no production board source/object or candidate is replaced.

The [willococa extension](https://github.com/willococa/fightcade-kof-training-mode-extension/blob/main/kof_training.lua)
provides field hypotheses: hit status at P2+0x72, movement at+0x73, guard flags
at+0xe3 and height at+0x21. Its mutable main URL was consulted on22 November;
pin its revision before any source-derived production implementation. Native
traces show ordinary jump also raises height. The swept player touches height0
at61 and71 while its movement continues until106; hit status already clears at81.
Neither height nor hit status alone qualifies wake-up. Guard flags0x20/0xa0 are
tested separately from attack-associated values.

Scenario-specific private predicates arm on the observed incident, then wait for
the sampled hit/movement/height/flag combination to clear. Ordinary single-frame
A requests then yield transient attacks after hit (request49, attack59), sweep
(107,116), and block (50,60). Idle never arms; all no-request controls stay clear.
These relative probe ticks describe a conservative post-recovery response,
later than the earliest successful requests, not a frame-perfect reversal. The
harness is not a general incident classifier. Further scenes, characters, input
history semantics and replay/state behaviour are required before a production
option. See [signal receipt](recovery-signal-receipt.json);3.2b2b remains open.

### Scenario-independent classifier — 22 November 2026

A private classifier now receives only the memory reader and battle-profile
fields, never the fixture/scenario identifier. A hit observation arms recovery;
height during that incident distinguishes the sampled knockdown. Guard flag
transitions arm a separate block response; hit observations take priority. The
conservative clear predicates retain the previous research scope.

Iori/Brian and the existing Kyo/Yuri fixture each yield one correctly classified
hit, knockdown and block incident in no-response controls, and one successful
ordinary-A response per incident when enabled: six positive responses total.
Kyo/Yuri request/attack ticks are49/57,104/112 and56/64 respectively, confirming
that the harness does not hard-code the first matchup's timings. Idle, jump,
own A/C attacks, crouch and back movement produce zero incidents/automatic input;
detector-enabled/disabled180-frame hashes match in these six negative scenarios.

The willococa reference is now pinned to revision
`4ecc81bb0e75522c8f5067641ec83f54c7d55530`; its private downloaded file hashes to
`d0d423c81d7c8a2538e7e36dd4edce73c5c46c5788adb68f2ded31a7a88d979c`.
Only RAM-field facts inform the independent classifier; no Lua is added to the
runtime. See [classifier receipt](recovery-classifier-receipt.json). Pending
qualification includes air hits, multi-hit/cross-up/throws, repeated incidents
and production state/rollback/recording-slot integration. No first-frame reversal
or complete-roster claim is made, and no executable/menu is updated.

### Repeated incidents and airborne contact — 22 November 2026

Iori/Brian660-frame trials use ordinary movement between three attacks, without
resetting either board or classifier between incidents. Hit, sweep and blocked
sweep each produce exactly three classifier events; enabled responses produce
exactly three ordinary-A attacks per trial. The blocked trials lose no HP.
Differences in subsequent damage between response/no-response trials are retained
as real gameplay effects, not presented as equal-state runs.

Eight200-frame recipes vary the timing of standing C against a P2 jump. Each
records actual HP loss with positive height before/after damage, one recovery
event and one automatic response. Three ordinary jumps in a660-frame no-contact
control produce zero incidents/responses. This supports the sampled airborne-hit
response, not universal air-hit or wake-up behaviour.

The private event2 name is narrowed from Knockdown to AirborneHit: positive
height during damage does not distinguish ground knockdown from an aerial-hit
recovery. Numeric value and detector behaviour are unchanged. Earlier sweep
receipts describe confirmed sweep scenes only. Multi-hit/cross-up/throw cases
and production state/rollback/slot integration remain pending. See
[incident receipt](recovery-incidents-receipt.json). The accepted executable
remains unchanged.

### Private checkpoint lifecycle — 22 November 2026

Seven checkpoints cover standing hit, early and late sweep recovery, block, and
post-response state for all three incidents. The harness stores detector and
response bookkeeping in a private textual sidecar alongside an actual port
board/game save. Independent cold-process restore and a deliberate board rewind
both reproduce the uninterrupted hash suffix, audio suffix, remaining events and
final response summary. The late-sweep checkpoint retains a pending elevated-hit
incident after the sampled hit byte has already cleared. Post-response checkpoints
do not duplicate the prior event or response.

Sixty paused presentation calls at each checkpoint (420 total) leave board hash
and detector fields unchanged. These are host lifecycle tests, not production
netplay rollback. The sidecar is not integrated into the game's state schema;
atomic cross-file failure, malformed input and old-version migration remain
unqualified. See [lifecycle receipt](recovery-lifecycle-receipt.json). No production
source, option, executable or installed package is updated by these probes.

### Production state foundation — 22 November 2026

`RecoveryDetector` now stores a bounded incident enum and explicit boolean byte
in KOF98 training mark57. A non-airborne incident cannot carry the elevated flag,
and an airborne incident must carry it. Fields are decoded into temporary bytes
and assigned only after validation. Marks54/55/56 import an idle detector while
preserving their existing recording/guard migrations. This is a dormant state
foundation: no before-frame detector observation or automatic input is enabled.

Actual port tests load all three earlier marks, round-trip each pending incident,
and reject eight malformed/truncated recovery payloads while preserving the
complete prior board hash and serialized game state. Sixty presentation calls
preserve state. Reset, exit, record ownership, online and viewer guards, and hard
reset clear pending state. The new component test also covers invalid field
combinations and invalid battle-profile clearing. CTest passes13/13 with the
existing UCRT toolchain directory on the child process PATH; an initial invocation
without that directory failed to load toolchain DLLs and was rerun correctly.

Final executable regression results and fingerprints are recorded separately in
[state foundation receipt](recovery-state-receipt.json). No accepted candidate or
installed package is replaced. New mark57 saves require this or a compatible
newer executable; old candidates remain available with their own user files.

### Conservative recorded recovery response — 22 November 2026

The opt-in Dummy > Recovery response setting now filters Off/block/hit/airborne
hit/any and starts the selected input-only slot in Once mode on the matching
conservative event. RESP identifies automatic ownership. Explicit manual play
and recording preempt it; Off stops only response-owned playback. A selected
empty slot cannot start a response. Observe/trigger never runs in paused
presentation, online or replay viewing.

Production-object native tests pass20 setting/incident combinations for each of
Iori/Brian and Kyo/Yuri (40 total), including first-input timing, forced Once,
manual/record priority, empty slot and online/viewer input preservation. A saved
active response reproduces120 frames of complete board hash, serialized game and
audio per matchup. Mid-response record/play commands preempt automatic ownership;
a manually started loop survives turning automatic response off.

Mark58 appends an explicit owned byte, requiring active Once playback when set;
actual54/55/56/57 states remain readable. Eleven malformed/truncated recovery or
ownership cases reject through atomic board/game rollback. CTest passes13/13.
Final CLI old/off/on900-frame hashes/WAV match, existing guard modes4/5 each
retain900 matching old/new hashes/WAV, and enabled response with empty slots
retains900 matching frames. Response-enabled300-frame guarded rollback stress
matches its disabled control; this does not claim active offline response under
netplay. Candidate dependencies also pass a direct two-frame headless startup
with no toolchain directory on PATH.

Native GPU images show RESP playback and the real Dummy menu. The local candidate
is separate from the accepted random-low build and installed beta; no user save
is bundled. See [response receipt](recovery-response-receipt.json). First-frame
reversal, complete roster, multi-hit/cross-up/throw coverage and new Internet
acceptance are not claimed.

### Original KOF2002 training — 22 November 2026

The user accepts KOF98 recorded responses and requests the original KOF2002 set.
An ordinary-input boot reaches a two-human battle at frame4043, with102 HP each.
Read-only idle/hit/sweep/block/jump controls confirm a distinct0x10b056 battle
profile. Damage in no-response controls is0/8/11/0/0. Recovery hypotheses produce
one incident each for hit/sweep/block and none for idle/jump. A request after the
swept recovery deals3 damage to P1. Later production tests use transient attack
observations as well as input/cursor checks rather than assuming damage is always
required for a response. Kyo/Iori identities are visible in the captured native HUD.

The independent original-set adapter offers session entry/reset,102-HP refill,
clock0x10a7d2, read-only power bar plus0x42-scaled stocks, bounded slots and the
same opt-in response contract. It exposes no hitbox/throw/projectile page: the
reference table differs (including hurt ID23), and the adapted reader is used
privately as a response witness only. Random guard episodes use attack buttons,
without the KOF98 collision-based extension.

Twenty production-object setting/incident cases pass, along with Once/first-input,
manual/record preemption, empty/off guards and online/viewer input preservation.
Active-response save/load reproduces120 frames of board hash, complete game state
and audio. Three pending detector round trips, eleven atomic invalid/truncated
loads, legacy board-only import, pause/reset/exit/record/online/viewer/hard-reset
guards pass. Actual damage followed by enabled resources holds both fighters at
102 HP and clock0x6000 for120 frames. In24 matched low-guard trials, eleven chosen
low guards take0 damage, thirteen pass choices sum26, and the matched standing
controls sum22. These are sampled Kyo/Iori outcomes.

Training-off900-frame hashes/audio match the previous original-set executable;
enabled-training300-frame guarded rollback matches disabled preferences. CTest
passes13/13. The first response harness incorrectly used the early battle-intro
snapshot as a corner fixture and failed its start-count control; preparation was
corrected using400 frames of ordinary forward movement, retaining the failed log.
No game-state forcing or ROM change was used. Evidence and final candidate
fingerprints are in [KOF2002 receipt](kof2002-training-receipt.json). Magic Plus II,
installed launcher/beta and the accepted KOF98 candidate remain unchanged.

### Magic Plus II training — 22 November 2026

After human acceptance of original KOF2002, the variant is probed independently
using its existing checksum-validated loader. Ordinary-input boot reaches a
two-human Kyo/Iori fixture at4043 with102 HP each. Idle/hit/sweep/block/jump
controls observe damage0/8/11/0/0 and no idle/jump incidents. The sampled recovery
signals match the parent in these scenes; this does not qualify every hack rule.
The native HUD shows the hack's330 power total without any meter writes.

The variant owns a distinct adapter namespace and state mark0x4b4d3201, with
legacy board-only import. Loading the original game's new training state is
rejected while preserving the prior board/game state. Twenty response-policy
cases, Once/first-input,120-frame full state/audio continuation, explicit-action
priority, off/empty and online/viewer guards pass. Three pending-state round trips,
eleven malformed/truncated payloads, lifecycle guards and120-frame102-HP/clock
resource tests pass. In24 low-guard trials, eleven guards take0 damage, thirteen
pass choices sum26, and matched standing controls sum22.

Training-off900-frame hashes/audio match the earlier variant executable;
enabled-training300-frame guarded rollback matches disabled preferences. CTest
passes13/13. The separate candidate passes direct two-frame headless startup
without the toolchain on PATH. No collisions, full roster, special hack mechanics,
first-frame reversal or new Internet acceptance are claimed. Accepted KOF98 and
original KOF2002 packages are unchanged. See
[variant receipt](kf2k2mp2-training-receipt.json).

### KOF2002-family player boxes — 22 November 2026

After accepting both training candidates, the user requests the missing player
hitboxes. Each set now has a player-only reader and Hitboxes page. The original
Lua's KOF2001/2002 profile classifies ID23 as vulnerability and derives the camera
from phase0x10b056+0x38 (0x10b08e). An initial inherited KOF98 camera address
caused an oracle mismatch; it was corrected before delivery. The first P1
wall-cross recipe also failed its side-change control; ordinary P2 movement via
the existing recording command produced the verified opposite-facing fixture.
Failed setup/mismatch data remain private; no character or position RAM forcing
was used.

Per set, all3,282 rectangles match the actual original Lua:1,890 vulnerability,
16 attack,176 guard and1,200 push boxes across600 snapshots. Both players cover
facings0/1, with62 camera positions and jumps. Lua is executed privately against
native RAM with writes rejected; it is not shipped in the runtime. Original
KOF2002 is selected directly and Magic Plus II resolves its parent profile.

Production inspection preserves board hash; paused load/reset recompute geometry,
while ended practice, online and viewer contexts clear it. A targeted component
test protects the distinct camera, ID23, correct battle phase and degenerate-box
handling in both profiles. CTest passes14/14 for each game. Accepted training
executables versus new boxes-off/on trajectories retain900 identical hashes/WAV;
300-frame guarded rollback stress matches disabled preferences. State formats
are unchanged. Final native oracle receipts are bound to the final executable
fingerprints after the test-target build relink.

Native GPU captures show actual player rectangles and the menu. Separate
candidates preserve accepted user packages; projectile/throw areas, throwability,
full roster, first-frame reversal and new Internet acceptance are not claimed.
See [original receipt](kof2002-hitboxes-receipt.json) and
[variant receipt](kf2k2mp2-hitboxes-receipt.json).

### KOF2002-family transient throw attempts — 22 November 2026

The user accepts both player-box candidates and requests grab boxes. The delivered
slice is explicitly event-time throw attempts, not opponent throwability. Each
set independently yields31 nondegenerate C/D attempt samples across both players
and both starting sides/facings. Production ReadThrow geometry matches the actual
original Lua callback for every sample, using watchpoint0x10e7b5 and camera0x10b08e.
Four600-frame observer-on/off trials per set preserve all hashes and PCM samples.

The native cache holds at most two optional boxes outside serialized game state,
with board owner, frame and inspection-epoch tags. Before-frame clear, conditional
watch attachment, callback/inspection context guards and toggle invalidation
prevent stale reuse. Native display tests observe7 P1 and8 P2 throw frames per
set and pass pause, expiry, saved-event load rejection, round/hard reset,
toggle/session non-resurrection, byte/word delivery, detach and online/viewer
guards. Yellow rendering and the real menu were inspected offscreen.

CTest passes14/14 for each set, including invalid actor/ID/degenerate range checks.
A new synthetic test initially placed the throw fields in the wrong fixture
offset; the fixture was corrected to base+0x188/+0x192 before the final passing
run. Final CLI900-frame C/D trajectories retain identical old/off/on hashes/WAV;
300-frame enabled/disabled guarded rollback remains equal. State schemas and
accepted candidates are unchanged. Command/air throws, full roster, throwability,
projectiles and new Internet acceptance are not claimed. See
[original attempt receipt](kof2002-throws-receipt.json) and
[variant attempt receipt](kf2k2mp2-throws-receipt.json).


### 2026-11-22 — KOF2002 and Magic Plus II defender areas

Each set independently matches the original Lua on 600 real native RAM snapshots:
1,012 nondegenerate rectangles (588 P1, 424 P2). Sixteen controls per set modify
copied snapshots only: restrictive status, invulnerability timer, signed eligibility,
proximity guard, each zero radius, and two permissive flag controls. No live guest
RAM was altered for these controls. Degenerate reference rectangles are filtered.

Native overlay checks preserve board hash and complete serialised game state,
including 60 paused presentations, load/reset, toggles, context exit/return and
Hide all boxes. Both builds pass 14/14 CTest checks, 900-frame accepted/off/on
state-hash and WAV comparisons, and 300-frame guarded rollback comparisons.
Separate candidate packages pass direct two-frame startup without toolchain PATH.
Screenshots and per-set throwability receipts record executable hashes and scope.
Save schemas and accepted prior packages are unchanged. Human acceptance of these
new grey areas is pending. These are reference-derived areas, not guarantees of
throw success or full-roster/command/air-throw coverage; projectiles remain pending.


### 2026-11-22 — KOF2002-family projectile candidates

The user accepted the previous grey defender areas. Projectile traversal is now
qualified separately: KOF2002 136 snapshots / 1,077 total rectangles including
130 projectile attacks and 136 projectile vulnerabilities; Magic Plus II 127
snapshots / 1,008 total rectangles including 123 attacks and 127 vulnerabilities.
Actual original Lua agrees with production readers, including body geometry.
Both probes use ordinary P2 Iori motion inputs; no ownership or full-roster claim.
The first generic active-object fixture expired immediately; the lifecycle fixture
was corrected to save a real projectile-attack frame. It demonstrates 5 visible
frames in the original and 4 in Magic, followed by disappearance.

Each native overlay check compares complete game serialisation and board hash,
current geometry, paused load, 60 paused presentations, independent toggle,
Hide all boxes, board reset and online/viewer/session guards. Both builds pass
14/14 CTest checks, including bounded traversal/duplicate/misaligned/out-of-RAM/
negative sentinel/cap-exhaustion controls. Regression compares accepted grey/off/on
hashes and WAV over 900 frames from the projectile fixture; 300 guarded rollback
frames also match. Final runs use projectile-accept-final after fixing the fixture.
Per-set receipts bind executable hashes, native GPU captures and separate packages.
No save schema change, Internet acceptance, installed-beta update or public commit.
Human acceptance of the new projectile overlay remains pending.


### 2026-11-24 — Accepted candidates and Terry expansion

The user confirmed both existing projectile candidates work. Their executable
hashes remain unchanged. New normal-input selection captures identify Terry P1
and Leona P2 independently in each set; an initially intended Athena selection
actually reached Leona, so no Athena coverage is claimed. No fighter RAM patch.

Per set, Terry qcf+A yields 65 snapshots and 512 total rectangles agreeing exactly
with the original Lua: 57 projectile attacks, 65 projectile vulnerabilities, 130
push and 260 hurt rectangles. Native overlays verify current geometry and preserve
complete game serialisation and board hash around each presentation. Each of A
and C receives 900 visibility-off and 900 visibility-on frames with equal hashes
and raw PCM. A has 182 projectile-visible frames in each run. C has zero: the
initial positive-projectile assertion failed, and the sequence is retained only
as an observed no-projectile control, not evidence for another move.

No production source/build change, new test-suite run, online acceptance or new
binary is claimed. Existing 14/14 build qualification remains in prior receipts.
Private recipes and raw data are under work/[set]-roster-*; public aggregate
receipts and two authorised captures per set contain no state/RAM/audio. Full
roster, move and facing matrices remain pending.


### 2026-11-24 — Athena ordinary-selection projectile expansion

Both sets independently reach Athena P1 versus Iori P2 using normal cursor and
team/order confirmation. Selection and native combat captures identify the fighter;
no character-ID RAM patch. An initial inherited forward-motion recipe produced no
projectile and failed its positive witness assertion; it remains private evidence
of a failed recipe. Corrected backward-motion samples produce offensive projectiles
for both A and C, without changing game source or accepted executables.

Per set: A117 snapshots/929 total rectangles (110 projectile attacks +117 projectile
vulnerabilities); C58 snapshots/462 total rectangles (56+58 projectile rectangles).
All geometry equals actual original Lua. Each input runs 900 frames with visibility
off and 900 on: hashes and PCM match exactly. A has351 projectile-visible frames,
C183. Each presentation preserves board hash and complete serialised game state,
with expected overlay geometry checked against the production reader. Captures
for A/C and selection are retained with hashes in each Athena receipt.

No game rebuild, new CTest run, online claim, save-schema change or installation
update. Accepted candidate hashes remain bound to their previous receipts. Other
moves, airborne projectiles, reversed-facing matrix and complete roster remain
pending. Private work/[set]-athena-* contains all raw states/RAM/PCM/Lua wrappers.


### 2026-11-24 — Opposite initial facing for Athena and Terry

Ordinary dummy-recorded jump/walk inputs cross P2 past P1. The private preparation
requires both P1 facing1->0 and reversed X ordering, then stops/clears the input
slot and settles before saving. Athena crosses after306 inputs in both sets;
Terry after268. These are normally reached states, not position/facing RAM patches.

Independently per set, left-facing Athena A has124 snapshots/239 projectile
rectangles, Athena C70/133 and Terry A66/129. All260 snapshots and501 projectile
rectangles agree with actual original Lua; body geometry agrees too. Each input
runs900 visibility-off and900 visibility-on frames: hashes/PCM equal, native
geometry checked and board hash plus complete game serialisation unchanged around
each presentation. Visible projectile frames are371,221,203 respectively.
Screenshots identify the fighters on the crossed side and the actual native overlay.

The accepted EXE hashes and save formats remain unchanged. No production rebuild,
new CTest run or Internet acceptance is claimed. This supplements prior initial
right-facing samples only for P1 Athena A/C and Terry A. P2 roles, dynamic repeated
crossings, other moves and full roster remain pending. Raw private data stays under
work/[set]-projectile-cross and work/[set]-opposite-projectiles.


### 2026-11-24 — Independently selected P2 projectile samples

Both sets select Kyo P1 versus Athena or Terry P2 through normal mode/cursor/team/
order inputs. Captures identify the actual P2 fighter. Existing dummy recording
routes the scripted motion to P2; every frame checks P1 input remains neutral and
each run observes positive nonzero P2 input. No character-ID RAM patch.

Three P2 sequences (Athena A/C, Terry A) each run900 frames with projectile boxes
off and900 on. Native overlays preserve board hash and complete game state around
every presentation; geometry/count checks pass, hashes and PCM match per input.
Actual original Lua agrees on all collected snapshots/rectangles; exact counts
are recorded in the two projectile-p2 receipts, with capture hashes and unchanged
accepted EXE hashes. This is P2 from the initial right-side/left-facing state only.

No production game source/build or save schema change, no CTest rerun, physical
controller or Internet acceptance. P2 opposite-facing states, continuous crossing
sequences, other moves and full roster remain pending. Raw data stays privately
under work/[set]-p2-battle and work/[set]-p2-projectiles.


### 2026-11-24 — Crossed P2 facing closes the sampled role/facing matrix

P1 ordinary jump/walk inputs cross P2 to the left side facing right. Preparation
asserts P2 facing0->1 and reversed X order after settling. Athena needs305 inputs,
Terry267 in each set. No position/facing patches. Existing dummy recording then
drives P2 Athena A/C or Terry A while checking P1 remains neutral every frame.
Each input runs900 visibility-off and900 visibility-on frames with equal hashes
and PCM, positive P2 input/projectile witnesses and read-only native presentation.
Actual original Lua geometry agrees on every collected snapshot; exact independent
set counts and image hashes are in projectile-p2-opposite receipts.

Together with the earlier P1 right/left and P2 left-facing receipts, these samples
close only the three-input initial role/facing matrix. Full roster, more moves,
continuous/repeated crossings, collision outcomes, physical controllers, Internet
and frame advantage are not certified. Production code, accepted EXEs and save
schemas remain unchanged. No new CTest run or installation update. Raw RAM/saves/
PCM/Lua wrappers remain private under work/[set]-p2-cross and p2-opposite-projectiles.


### 2026-11-24 — Kensou and Andy first-orientation samples

Normal selection and combat captures independently identify Kensou or Andy P1
versus Iori P2 in both sets. No character-ID patch. Ordinary quarter-circle-back
+A/C sequences each run900 visibility-off and900 visibility-on frames, with
positive offensive-projectile witnesses. Per-input hashes and PCM match. Every
presentation checks expected geometry, board hash and complete game serialisation.
Actual original Lua agrees on all snapshots/rectangles; per-set exact totals and
capture hashes are in the extra-roster receipts.

The evidence is P1's initial left-side/right-facing setup only for these fighters.
No extension to P2, opposite-facing states, every move, collision outcome, hardware,
Internet or frame advantage. Production code/accepted EXEs/save schemas remain
unchanged; no CTest rerun. Private raw inputs/states/RAM/PCM/Lua wrappers stay under
work/[set]-extra-battle and work/[set]-extra-projectiles.


### 2026-11-24 — Kensou/Andy remaining sampled role/facing cells

Normal P2 selection is verified in native selection/combat captures. Ordinary
jump/walk crossings reach the opposite facings with facing/X-order assertions;
Kensou needs349 preparation inputs for crossed P1 and347 for P2, so the private
preparation cap is640, within the existing recording bound. Andy crossings retain
per-set observations in receipts. No fighter/position/facing RAM patches.

Each set independently completes twelve new input exercises: Kensou/Andy A/C in
P1-left, P2-left and P2-right initial-facing cells. Every exercise runs900 frames
per visibility setting with equal board hashes and PCM, positive input/projectile
witnesses and inactive-player neutral-input assertions. Native presentation preserves
complete board/game state and expected geometry. Actual original Lua agrees on
every collected snapshot. Exact independent counts, crossing observations, EXE
hashes and native capture hashes are in the extra-matrix receipts.

Together with prior P1-right samples, this closes the sampled four-input initial
role/facing matrix only. No full-roster/move/dynamic-crossing/collision outcome,
physical-controller, Internet or frame-advantage certification. No production
source/build/schema changes or new CTest run; accepted binaries remain intact.
Private raw work/[set]-extra-matrix contains RAM/saves/PCM/Lua wrappers and must
not be published.


### 2026-11-24 — Documentation consolidation review

The training development report consolidates the local changes since published
main1e41731143f892d55cd0ee98d92734ddf0780cb3, independently checked against the
remote. README/per-game notices distinguish local implementation from a separate
documentation-only publication. The prepared documentation projection includes
Markdown, aggregate JSON receipts and authorised native PNGs, excluding source/test
changes, the unused input-art prototype and private work payloads. Local-link checks
use the projected published file set rather than accepting untracked source links.
No new executable, CTest, gameplay, peer or Internet validation is claimed by this
documentation pass. Implementation publication and clean-clone qualification remain
separate work.

## KOF98: combo counter, refills, meter and input history — 27 November 2026 (Claude)

Branch `claude/kof98-training` (worktree `D:/CPS2/neogeo-kof98-training`) from `claude/kof95-base` 3af7ab7, with
Codex's uncommitted training work copied from `D:/CPS2/neogeo-recomp` unchanged: all 25 files equal to
`work/documentation-excluded-source-hashes.json`; `port/input_art_data.h` left out as Codex asked.

Added, compared with peon2's fbneo-training-mode kof98 script and its framework (address facts only, no Lua code):
- `games/kof98/src/combo.h`: the dummy's combo, counted while the game's combo counter on it (0x1084b0) is not 0;
  the last hit's damage and the combo's total from its health (0x108439); the last combo kept after it ends. Saved
  with the training state (mark 0x4b393859; earlier marks still load, the combo then empty).
- Resources: Refill health is Off / Always full / After combo (a side out of hitstun, its attacker's counter on it at
  0, refills 4 points a frame); Meter always full (both gauges 0x1081e8/0x1083e8 at 0x80).
- HUD: Combo counter (COMBO n, DAMAGE d TOTAL t, shown by default) and Input history (Hide / P1 / P2 / Both: the last
  six inputs in fighting notation from each side's facing, 0x108131/0x108331, with frames held).

Checks: CTest 15/15 (the new `kof98_combo` included); a training fight from Codex's `kof98-hitboxes/attack.state`
showed COMBO 1 / DAMAGE 10 TOTAL 10, POW 128, the dummy back at HP 103 and both histories (`build/demo`,
`build/demo2`: 3-20, 5A-2, 5-38, 4-30 for the scripted inputs); offline 6000 scripted frames with training off: state
hashes and sound byte for byte the same as Codex's accepted `work/kof98-response-ready/kof98.exe`.

Not done: recording P1 or both sides, the replay editor, refill speed and maximum settings, stun, input delay and
missions; KOF2002 and Magic Plus II do not have these additions. No window or pad test yet.
