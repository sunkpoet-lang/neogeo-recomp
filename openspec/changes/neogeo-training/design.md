# Context
Inspected on 20 November 2026:
- [Tommy's training contract](https://github.com/tmcleroy/cps2recomp/blob/main/docs/TRAINING.md).
- [willococa's extension](https://github.com/willococa/fightcade-kof-training-mode-extension),
  its README, `kof_training.lua` and `kof_training_guipages.lua`.
- Existing local runtime hooks and KOF per-game registration sources.

The extension primarily targets KOF98; its README explicitly limits assurances
about other KOF games. It depends on FBNeo Lua memory/joypad/GUI APIs and the base
fbneo-training-mode project. It cannot be dropped into this native SDL/C++ runtime.
The configuration/code offers useful guard, reversal, recovery and practice-preset
ideas. Treat all proposed memory addresses as hypotheses per set.

# Decisions
1. Build one native training controller and HUD, with a small per-set adapter for
   phase, health, meter, facing, position and verified observations. KOF98 is the
   first research reference; it does not inherit KOF95 addresses or vice versa.
2. Use `Game::inTraining` and `kFeatureTraining`, already present locally. Practice
   mutation and injected dummy input must be disabled at their execution boundary
   online and during replay playback, even when stored preferences remain enabled.
3. Keep the familiar top bar. Features > Training opens a clearly labelled session;
   controller-friendly groups cover Session, Dummy, Resources, Display and Drills.
   A compact optional HUD shows input history, validated health/meter and combo
   information; unsupported fields are hidden or labelled unavailable. Hitboxes
   and frame advantage require independent verified readers, not inference from
   animation frames or the extension's menu appearance.
4. Phase 1: session entry/exit, refill/time, reset and basic dummy, input history.
   Phase 2: bounded dummy recording slots, side-relative playback, random selection
   with serialised deterministic seed, guard-after-hit and wake-up reversal.
   Phase 3: verified hitboxes/frame meter, combo/stun and practice presets (safe
   jumps, confirms, pressure). Expose only moves/recovery that that exact game has.
5. Dummy recordings are a separate training feature, not online `.fareplay`
   recordings. Training state must join save/load and replay identity when added;
   reset must clear stale HUD/input/dummy counters. Do not silently change games
   already released or their online behaviour.

# Provenance and limits
The extension repository advertises BSD-2-Clause but its README acknowledges
inherited code and uncertainty about licensing. Before copying any implementation,
pin exact revisions, read the actual LICENSE and inherited-file notices and retain
credits for willococa, peon2/base training mode and Tommy as applicable. Current
work records ideas only; no code or artwork was imported. Use our own consistent
HUD layout rather than assuming a screenshot grants asset reuse.

# Validation
Per-set native memory traces must cover character selection, both facing
directions, hit/block/KO/continue and round transitions. Verify HUD reads do not
alter baseline hashes/audio when training is off. Exercise session enter/exit,
reset/save/load, dummy slots, determinism and the online/replay mutation guards.
Human acceptance is required for gamepad navigation and each published drill.

## First implementation slice
KOF98 uses native coin/start input for entry (no forced phase or character writes).
P1 can control both character selections. Practice effects require the observed
two-human battle phase. First writers are health and clock; meter stays read-only
because the reference explicitly warns about partial MAX support. A first-round
board checkpoint supports reset; training counters/history/checkpoint are saved
through `Game::state`. Standard states require the same training feature choices.
Training recordings are refused for now; viewer playback has an explicit mutation
guard. No random behaviour, copied Lua or advanced measurement code in this slice.

## Dummy recording slice
Three process-local input slots hold at most1,800 simulated frames each. During
recording P1 controls the dummy (P2) and P1's fighter stays neutral; operator,
coin and Start inputs are never stored. Store horizontal inputs relative to the
dummy's facing on each frame, then resolve them against its facing on playback.
Recording is armed until initialized battle; it stops on limit, explicit Stop or
leaving battle after samples exist. Playback supports Once, Loop selected and
Random populated slots. Random choice uses an explicit saved non-zero PRNG seed,
never wall time, and selects only populated slots with no extra neutral frame.

Record replaces the selected slot; Clear explicitly empties it. Reset round,
Start and End training stop actions/reset playback seed but retain slots for the
current process. Standard save states persist slots, cursor and PRNG; there is no
separate disk library yet. Online and replay guards apply before sampling,
playback injection or PRNG advancement. Entering those modes cancels actions but
retains slot contents. These input-only slots are separate from match fareplays.
Add a nested Dummy recordings page and HUD action/length feedback. This slice does
not implement random guard or reversals. Validate direction changes, empty slots,
overwrite/cap, exact loop boundaries, random restore, malformed bounds and native
save/load/online/viewer regressions before refreshing the user-tested candidate.

## Shared Neo Geo current-input panel
User steering replaces the on-screen history with CURRENT inputs only. Reuse
InputDisplay's exact `btn/neogeo` A/B/C/D artwork (red/yellow/green/blue) and
`dir/stick/1..9` (white arrows on red discs, N for neutral), pinned to
`3a60d387a07f5d472ed6934f1f8c0508fea71db6`, retaining GPL3 licence and attribution.
No external overlay process or input polling: read the board's effective pads,
including dummy and replay inputs. A shared frontend renderer makes the same
theme available to every Neo Geo set. Default Automatic shows it with training;
Off and On are explicit preferences. No historical rows are rendered. Opposing
directions cancel visually per axis without changing game input. Embed exact
decoded RGBA assets with zlib already present, keeping original PNGs/source hashes.
Place bordered P1/P2 boxes above the bottom meter area; inactive buttons dim and
pressed buttons use full colour. No CPS/Street Fighter changes in this slice.
UTM remains a later CE/Hyper/ST research reference; no UTM code/assets are
imported by this Neo Geo slice.

User size revision: halve both panel dimensions (116x36 to58x18 native pixels),
including their resource headers/icons. Preserve their top/outer-edge anchors,
restore the uncovered game picture, and leave other HUD elements unchanged.

Further user steering: resource readouts belong at the upper character portraits,
not inside input boxes. Use compact blue/red-accented HP/POW badges above the
portraits without covering the life bars. This is the fighting-game presentation
convention; enable numeric resources only where that set's readers are verified.

Final visual revision after hands-on review: independently drawn native pixel
circles, high-contrast ABCD letters and neutral/eight-direction arrow geometry
replace resampled glossy sprites. Keep Neo Geo colours, omit P1/P2 text, and
use90x22 panels. During training cover the reserve-team names (19% height),
outside life bars and central practice readouts. Explicit On in normal play
retains lower positioning. HP/POW remain above portraits. Imported artwork and
provenance are historical; the active renderer no longer decodes/draws it.
This supersedes the earlier exact half-size requirement.

Arcade finish revision: use the user's existing mock_inputs CSS as visual
reference (raised lip, dark shadow, upper-left reflection and brighter pressed
cap). Implement native integer gradients, preserving ABCD colours/letters,
arrows, panel size/position and current-only behaviour. No external mock code,
fonts, player metadata, history or side panels are imported.

Reuse the actual per-system layout in the user's existing input display:
Neo Geo MVS has A low-left and B/C/D along the upper arc (source design centres
42,452 /92,414 /146,402 /198,410). At native resolution use relative centres
31,22 /47,11 /64,8 /80,10 in90x32 boxes. Retain requested red/yellow/green/blue,
white-arrow directions, no player labels/history and upper training placement.
Street Fighter uses Astro City two rows of three: LP/MP/HP above LK/MK/HK,
with the lower row offset left. Record this as the separate SF integration design;
do not enable a six-button map inside the four-button Neo Geo adapter.

Exact recorder finish: generate assets using the actual recorder Pintor._boton
and Pintor._bola methods and original font/palette, with source/output hashes.
Compose the controls on a3x presentation picture, after nearest expansion of the
native game/HUD, so the artwork is not reduced to224-line resolution first.
GPU/SDL and headless presentation must use the same path; menu layers stay above
it. Keep current-only MVS layout and portrait resources. Python/Pillow/font files
are development inputs, not runtime dependencies. Frame hashes/audio stay native.

Approved visual refinement: reduce the complete control panels25% in each
dimension, keep their outer/top anchors, and apply12% transparency (88% opacity)
to the whole composed panel. Blend once over the actual game picture, not over
a previously composited native input panel. Keep portrait HP/POW unchanged.

User accepts the size/transparency and moves controls immediately above the
super-meter area. Use a shared bottom anchor:24 native lines for the controls,
26 lines reserved below them (top174 at224 lines), keeping portrait HP/POW intact.
Use this fighting-game layout convention in normal/training display; individual
game HUD variations still require visual review before assuming universal fit.

## FBNeo training-suite reuse
See fbneo-reuse-audit.md and fbneo-source-fingerprints.json. The user authorises
useful adaptation from the installed suite. First implement independently checked
KOF98 read-only body-box decoding and native capability-driven GUI sections.
Do not transplant the reference's throw-field clearing write, unrestricted
projectile traversal or presumed parent/hack compatibility. Temporary throw
capture is a separately validated hook; online/replay guard remains in force.
Preserve author/source evidence and resolve inherited terms before publishing
literal source adaptations. An installed file hash is not an upstream commit ID.

Projectile slice: enumerate the KOF98 object-pointer array at0x10BF24 with a
64-object cap, valid even RAM offsets, sufficient object-span checks, repeated
pointer detection and the reference's inactive descriptor terminator. Keep unknown
ownership explicit. Separate projectile visibility from body filters; use original
projectile attack/vulnerability classifications. On malformed/capped traversal,
discard incomplete projectile rectangles while preserving valid player boxes.
Validate against the actual original Lua using native projectile snapshots;
effects without collision entries do not count as projectile-box evidence.

## Throw-capture investigation

Before exposing throws, observe the original KOF98 write at `0x10EBDF`, including
the low byte of a word write to `0x10EBDE`. Capture after the real bus write,
using the contemporaneous A4 player pointer, not an inferred last attacker.
The private probe may copy RAM and CPU context, but MUST NOT alter registers,
RAM, inputs or timing. Compare an instrumented run with an uninstrumented run
using identical state and inputs. No temporary throw-field clearing is allowed.
Probe output remains private; end-of-frame data alone is insufficient evidence.
Only the two verified player bases are accepted initially. Future production
events need explicit lifetime, pause/load/reset/rollback clearing and offline
guards before any menu capability is advertised. This investigation does not
enable a production hook or claim throw coverage.

Production slice: a board-local optional const observer watches one byte address,
including the low byte of a word store, after the real RAM store. It receives A4
without modifying CPU registers. Pointer/address and an invalidation epoch are
host-only diagnostics, excluded from state bytes/hashes. Reset and state reads
invalidate captures. KOF98 attaches it only for offline training with throw
display enabled, clears two last-event slots before each frame, and shows them
only on the immediately completed frame with matching epoch. Paused presentation
may retain that frame; load/reset cannot resurrect an event. Online/viewer/session
checks also apply inside the callback. No throwability or full-roster claim.

## Native training navigation

Keep Training as the entry page and group existing capabilities under Session,
Resources, Dummy, Recordings, Hitboxes and HUD, in that order. Session contains
start/reset/end and the session toggle; Resources contains health/time; Dummy
contains the verified stance/guard choices; Recordings contains the three-slot
controls; Hitboxes retains its independently qualified views; HUD contains its
display switch. Preserve feature/command IDs, command registration order, input
bindings, saved preferences and all runtime algorithms. Do not show unsupported
MAX refill, reversals, frame data or other-game adapters. This navigation change
does not add draggable HUD placement or a Lua GUI host.

## Optional HUD controls

Within verified offline KOF98 practice, add persistent presentation-only choices:
input visibility (follow global/show/hide), input position (above supers/higher/
upper sides), status text (full/session only/hidden) and portrait resources
(show/hide). Zero values preserve the accepted defaults. Apply visibility and
position consistently in native and high-resolution presentation; clamp panel
bounds and preserve size/alpha. Restore HUD defaults resets only these choices
and enables the existing HUD switch, leaving global input preferences, recordings,
resources, dummy behaviour and hitbox toggles untouched. Recompute display on
paused presentation without running gameplay. Outside offline training, the
global input preference and normal panel anchor remain authoritative. No mouse
dragging or arbitrary coordinates are introduced.

## Throw side-switch qualification

Use the existing native state reached by ordinary jump/crossing inputs (no RAM
pokes) to test the opposite player facings. Run each player separately from the
same state with forward-relative C/D attempts. Compare production-observer on/off
frame hashes and captured audio samples. Capture event RAM privately and compare
production `ReadThrow` with the actual original Lua callback, recording player,
facing, camera and rectangle coverage. Require real events in both runs; a run
with no event is inconclusive. This extends sampled ordinary-throw coverage only,
not aerial/command throws, throwability or a full roster acceptance claim.

## Command-throw investigation

For a concrete Iori case, compare candidate directional command sequences ending
in A with a forward-plus-A control; exclude C/D so an ordinary throw fallback
cannot qualify the test. Treat published move lists as input hypotheses, not
native acceptance. Record actual pads, event RAM, HP and positions privately.
Require real event geometry, production/original-Lua agreement, on/off hash/audio
equality and behavioural evidence distinguishing the move from the A control.
Do not label an event as a named move or general command-throw support solely
because the watched address was written. Retain every failed/inconclusive run.

## Deterministic random standing guard

Append a Random guard choice to KOF98's existing dummy modes, preserving values
0-3. An episode begins on a P1 attack-button press (before the CPU runs), or an
already verified P1 body attack. Choose stand/pass or hold relative back once
using a nonzero xorshift32 seed. Hold that decision through presses/active boxes
until30 quiet frames, avoiding per-frame random flicker. This is a standing-guard
input exercise, not automatic low/projectile/reversal coverage or a success
prediction. Input-slot recording/playback has priority and resets the episode.

Serialise seed, episode, decision and bounded quiet counter in training mark
0x4B393856; migrate 0x4B393854/55 with the initial guard state. Paused inspection
never advances the seed. Round reset stops an episode but retains the seed;
session start/end creates a fresh initial seed. Invalid state reads must use the
existing atomic game/board rollback path. Offline/session/battle gates remain.

## Random low-guard option

Append dummy value5 for Random low guard, preserving values0-4. Reuse the saved
episode/seed without changing state mark56. A guard decision holds relative back
plus down; a pass decision stays neutral. Switching between random standing and
low guard changes the posture without rerolling the current episode. Recorded
slots still have priority. Verify a real crouching attack with positive damage
under pass and standing-guard controls, and no damage under low guard in matched
reset trials. This is an explicit low-guard exercise, not automatic high/low,
overhead, projectile or reversal detection. Preserve existing standing-mode
hashes/audio and qualify low-mode continuation with identical settings.

## Hit/knockdown/recovery research

Treat the installed reference's combo-byte-as-hitstun functions as hypotheses,
not an authoritative wake-up clock. Capture native idle, standing-hit, sweep and
blocked-sweep cases with real inputs. Record HP, combo bytes, positions and player
object data privately, comparing read-only logging on/off hashes/audio. Distinguish
hit response, knockdown, guard recovery and first accepted input through separate
measurements. Do not expose automatic reversal or frame-advantage values from a
nonzero counter alone. No new RAM writes, character forcing or production adapter
is permitted by this research step.

Use matched no-request controls when varying a requested input. Separate host
request tick, first player-object divergence and transient attack onset: none is
automatically a game-level input-acceptance or wake-up signal. Compare short and
held requests before inferring buffering. Preserve raw private traces, publish
aggregate observations only, and retain the production reversal gate until an
independent semantic recovery signal is qualified.

## Recovery-response state integration gate

The private research sidecar is not a deployable state format. A future production
response adapter must include pending incident, elevated-hit flag and response
playback state inside the game's atomic serialization path, with explicit range
and boolean validation. Older state versions must start with no pending response;
malformed reads must preserve both board and game state. Offline/session/battle
guards, reset/session exit and input-slot ownership must clear or transfer pending
work explicitly. Presentation must not advance or consume a recovery event.
Cold restore, deliberate rewind and actual rollback stress need separate evidence;
private sidecar equality does not satisfy the production migration/rollback gate.

## Magic Plus II qualification

After user acceptance of original KOF2002, test the kf2k2mp2 bootleg independently.
Use its existing checksum-validated variant loader and parent assets unchanged.
Collect ordinary-input battle/resource/recovery controls before adapting the
workflow. If qualified, use a distinct variant-owned state mark, preserve legacy
board-only imports and repeat response/guard/state/regression checks. Do not
enable collision views or assume every parent mechanic applies to the hack.
Accepted KOF98/KOF2002 candidates and installed packages remain untouched.

## Original KOF2002 qualification

The user requests applying the accepted recorded-response workflow to original
KOF2002. Treat KOF98 as a behavioural reference only. Verify phase0x10b056,
health bounds, clock0x10a7d2 and distinct power-stock fields against the installed
KOF2002 reference and native ordinary-input traces. Do not transfer KOF98's
phase0x10b094, clock0x10a83a, HP ceiling or collision-ID table. Recovery-field
and response hypotheses require idle/hit/sweep/block controls and negative jump
controls before an adapter is exposed. Do not enable Magic Plus II by parent
association. Existing KOF98 candidate and installed packages stay untouched.

## Conservative selected-slot response

Append `training_response` on the Dummy page:0 Off,1 block,2 hit,3 airborne hit,
4 any observed incident. Observe only during ready offline practice when no
recording/playback owns the dummy. On a matching conservative event, start the
selected populated slot in Once mode and consume its first input that frame.
Explicit record/play commands preempt automatic playback. Do not reset recorded
data or its random seed when skipping an empty response slot.

Training mark58 appends an explicit response-owned byte to mark57's detector
fields. Validate it as0/1 and require active Once playback when owned. Import
marks54–57 without ownership; preserve mark57 detector fields. Stop response-owned
playback when switching the response off or leaving a ready offline session;
manual playback retains its existing lifecycle. Paused HUD inspection only reads
state. Show RESP rather than PLAY for owned playback. No first-frame reversal,
general knockdown classifier or unqualified roster coverage is implied.


KOF2002/Magic projectile qualification: independently inspect each set's object
pointer list at phase base + 0xe90 (0x10bee6), with camera 0x10b08e and hurt ID23.
Reuse only bounded traversal policy (64 objects, RAM bounds, duplicate termination,
fail closed on malformed/capped lists); do not infer object ownership from order.
First compare private native ordinary-input snapshots against the actual original
Lua. Production exposure requires per-set geometry, lifecycle and state/audio
regression evidence. Preserve accepted grey/yellow candidates throughout.


Roster expansion uses normal character-selection and motion inputs only. Capture
selection and combat evidence to identify each fighter independently for each set;
no character-ID RAM patching. Compare production geometry with the original Lua
and visibility on/off trajectories, keeping private snapshots/states out of source.
An unchanged accepted executable needs additional evidence, not a replacement build.


Opposite-facing projectile qualification reaches crossed positions using normal
recorded dummy jump/walk inputs, then stops and clears that private input slot.
Require both changed facing and reversed X ordering before saving the fixture.
Compare native ordinary-motion samples to original Lua and visibility on/off
hashes/PCM, retaining existing approved executables. This qualifies sampled
initial opposite-facing cases, not an entire player-role or dynamic-crossing matrix.


P2 projectile qualification uses normal independent fighter selection, verifies
identity in selection/combat, then drives P2 through the existing bounded dummy
recording input route while keeping P1 neutral. This is a native training-input
exercise, not physical controller or online acceptance. Compare original Lua and
visibility-on/off hashes/PCM without changing accepted game binaries or user saves.
