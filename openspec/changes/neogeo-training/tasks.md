## 1. Exploration
- [x] 1.1 Read the user references and existing runtime/per-game training hooks.
- [x] 1.2 Record native integration, per-set gates, staged HUD/dummy scope and attribution requirements.
- [ ] 1.3 Pin reference revisions and inspect complete inherited licences before code reuse.
- [x] 1.4a Verify KOF98 battle phases, health/clock, facing and positions through native traces; meter read-only.
- [ ] 1.4b Independently scope KOF95, KOF97, KOF2002 and Magic Plus II.
- [x] 1.4b1 Qualify the original KOF2002 set's battle/resource/recovery fields with ordinary-input controls before implementing its adapter; Magic Plus II remains separate.
- [x] 1.4b2 Implement and qualify original KOF2002 session/resources, recorded slots, guard intentions and conservative responses without enabling unverified collision views.
- [x] 1.4b3 Independently qualify Magic Plus II ordinary-input resource/recovery behaviour and a distinct adapter/state mark before exposing its training candidate.
- [x] 1.4c Qualify original KOF2002 and Magic Plus II player-box profiles against actual original Lua, add guarded menus/axes and test read-only/load/reset/regression behaviour separately per set.
- [x] 1.4d Qualify and expose transient player throw-attempt areas for both sets, including actual-callback geometry, bounded capture, expiry/epoch guards and read-only regression; throwability remains separate.
- [x] 1.4e Qualify reference-derived defender throw-vulnerability areas separately for both sets, including original-Lua comparison, suppression controls, read-only lifecycle and regression evidence.
- [x] 1.4f Qualify bounded projectile traversal separately for KOF2002 and Magic Plus II against original Lua and native ordinary-input snapshots before exposing a display control.
- [x] 1.4g1 Extend projectile evidence to normally selected Terry qcf+A independently per set, with native Lua geometry and visibility on/off state/audio checks; preserve accepted binaries.
- [x] 1.4g2a Qualify normally selected Athena projectile inputs independently per set, preserving the accepted executables.
- [x] 1.4g2b Qualify opposite-facing Athena/Terry projectile samples after ordinary jump crossings, with independent Lua and visibility regression per set.
- [x] 1.4g2c Qualify normally selected P2 Athena A/C and Terry A projectile samples independently per set, including actual input routing, Lua and visibility regression.
- [x] 1.4g2d Qualify crossed initial P2 facing for Athena A/C and Terry A independently per set, closing only this sampled role/facing matrix.
- [x] 1.4g2e Independently select and qualify additional Kensou/Andy projectile inputs per set; do not assume their motion recipes or extend the role/facing matrix without evidence.
- [x] 1.4g2f Complete sampled Kensou/Andy A/C role/facing cases through ordinary selection/crossings and per-set Lua plus visibility regression.
- [ ] 1.4g2 Extend additional fighters, moves and facing combinations without assuming that all attack-button variants create projectiles.
- [x] 1.5 Consolidate the local development work, actual captures, evidence matrix, credits, state compatibility and publication limits in a public-safe documentation report; keep implementation publication separate.
## 2. Native first slice
- [x] 2.1 Add explicit session entry/exit/reset and practice mutation guards.
- [x] 2.2 Add verified resources, basic dummy and input-history HUD.
- [x] 2.3 Verify off-state regression, state round trips and online/replay blocking.
## 3. Advanced drills
- [x] 3.1 Add bounded dummy slots and deterministic side-relative/random playback.
- [x] 3.2a Add deterministic KOF98 random standing guard with saved decisions, migration, priority and native tests.
- [x] 3.2b1 Qualify explicit random low guard with matched standing/pass damage controls, saved-decision posture switching and unchanged existing modes/state layout.
- [x] 3.2b2a Collect read-only native idle/hit/sweep/blocked-sweep recovery signals and reject response-timing inference when idle positive controls fail.
- [x] 3.2b2a1 Establish a transient response witness with idle positive/negative controls and non-mutating event observation; retain sampled recovery results without exact-timing claims.
- [x] 3.2b2a2 Compare one/four-frame A requests and adjacent single-frame response boundaries against matched no-request controls; distinguish RAM divergence and attack onset from input acceptance.
- [x] 3.2b2a3 Verify immediate CPU input sampling, contrast reference field hypotheses with jump/knockdown controls, and test scenario-specific conservative recovery responses privately.
- [x] 3.2b2a4 Test a scenario-independent private incident classifier across two matchups, six ordinary-action negative controls and a pinned RAM-fact reference.
- [x] 3.2b2a5 Qualify repeated hit/sweep/block incidents and actual airborne contacts versus repeated ordinary jumps; avoid classifying all elevated damage as ground knockdown.
- [x] 3.2b2a6 Verify private detector checkpoint continuity through actual board/game save/load and deliberate rewind, including post-response checkpoints and paused presentation; keep production state integration separate.
- [x] 3.2b2a7 Integrate bounded detector fields into mark57, preserve actual54/55/56 imports, qualify atomic invalid loads and ownership/lifecycle clearing without enabling automatic response inputs.
- [ ] 3.2b2b Independently qualify an actual response detector and wake-up/recovery hooks before adding automatic reversal; other sets remain separate.
- [x] 3.2b2c Add opt-in conservative selected-slot response with Once ownership, explicit-action priority, mark58 migration/atomic validation and offline/viewer guards; first-frame reversal remains outside this slice.
- [ ] 3.3 Research hitboxes/frame data/combo/stun; publish only measured capabilities.
- [ ] 3.4 Human acceptance, screenshots, credits and catalogue documentation.

## 5. FBNeo training-suite adaptation
- [x] 5.1 Audit installed hitboxes/guipages/per-game/addon sources and fingerprint the reviewed copy; record authorisation and unsafe throw-write fallback.
- [x] 5.2a Gather a read-only KOF98 native battle probe for candidate object/box fields (900frames; not hitbox correctness acceptance).
- [x] 5.2b Validate KOF98 player-box geometry against original Lua over native battle snapshots, facing/jumps/camera.
- [ ] 5.2c Resolve inherited licences before publishing literal reference-source adaptations; current production reader is independent.
- [ ] 5.2d Expand roster/move coverage and human visual acceptance.
- [x] 5.3 Implement the KOF98 read-only body hurt/attack/push/axes capability and offline-only display switches.
- [x] 5.4a Validate bounded projectile traversal against original Lua, native presentation and off/on gameplay/audio; expose independent display control.
- [x] 5.4b1 Investigate post-write capture privately: twenty events across P1/P2 match original Lua geometry and preserve 1,200 frame hashes per run.
- [x] 5.4b2 Add the optional read-only production observer and partial throw-attempt switch; verify sampled P1/P2 presentation, expiry/pause/load/reset, online/viewer guards, rollback stress, audio and four-game regression.
- [x] 5.4b3a Qualify sampled ordinary throw geometry in both player facings using an opposite-side state reached by real crossing inputs; compare production observer/reader with original Lua and on/off hashes/audio samples.
- [x] 5.4b3b1 Qualify a confirmed Iori A-only command-throw sample: preserve user F5, compare two input variants against forward+A control, validate 20 event rectangles with original Lua, both facings and on/off hashes/audio/motion; verify production presentation.
- [ ] 5.4b3b2 Extend aerial/other command-throw and roster coverage and continuous crossing sequences; obtain broader human acceptance. Throwability areas remain separate.
- [x] 5.5a Add native Hitboxes page with independent controls and explicit partial coverage.
- [x] 5.5b1 Organise existing native controls under Session/Resources/Dummy/Recordings/Hitboxes/HUD; preserve option IDs, command pairing, saved state and gameplay/audio.
- [x] 5.5b2 Add qualified offline HUD visibility, fixed anchors, text/resources choices and restore defaults; preserve accepted default appearance and independent gameplay preferences.
- [x] 5.6a Qualify KOF98 random standing guard as a separate input exercise with deterministic state tests and measured guard/pass trials.
- [x] 5.6c KOF98: combo counter with last-hit and total damage, after-combo health refill, meter always full and a facing-relative input history (Claude, 26 Nov).
- [ ] 5.6b Extend stun, refill speed/maximum settings, recording P1 or both sides, additional guard/reversal/recovery and optional missions only with native evidence and deterministic state tests.
- [ ] 5.7 Requalify KOF95/97/2002/Magic Plus II separately; keep CPS/SF integration a separate adapter task.
- [x] 5.8a Automated player-box acceptance: old/off/on hashes/audio, paused load/reset freshness, online/replay inspection guards, screenshots/credits and isolated candidate.
- [ ] 5.8b Human visual/controller/roster acceptance; full projectile/throw and product promotion gates remain open.

## 4. Shared current-input presentation
- [x] 4.1 Pin original InputDisplay PNGs, verify blobs and include attribution/licence.
- [x] 4.2 Render current-only P1/P2 panels from effective board inputs with explicit visibility controls.
- [x] 4.3 Build and verify KOF95/97/98/2002/Magic Plus II; preserve core hashes/audio and game icons.
- [x] 4.4 Human review of the new panel and dummy slot controls (user confirms functionality).
- [ ] 4.5 Separate Street Fighter six-button implementation (deferred).

- [x] 4.6 Halve input boxes/icons and move verified HP/POW into compact upper portrait badges.
- [ ] 4.7 Human acceptance of crisp native controls, reserve-name placement and portrait resources.

- [x] 4.8 Replace resampled art with native geometry/letters, remove player labels and raise training controls over reserve-team names; supersedes4.6 sizing.

- [x] 4.9 Add arcade cap shading/reflections using the existing mock as visual reference, preserving panel behaviour.

- [x] 4.10 Reuse the existing recorder MVS arc layout; document SF Astro City layout for its separate integration.

- [x] 4.11 Reuse original recorder assets and compose at3x presentation resolution; verify bounds, GPU menu stacking and gameplay regression.
- [ ] 4.12 Human window review of exact recorder controls/performance (GPU and SDL).

- [x] 4.13 Reduce recorder panels25% and blend the full panel at88% opacity, once over the game picture.

- [x] 4.14 Move controls above the super meters using a bottom anchor; retain approved size/transparency.
