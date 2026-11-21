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
