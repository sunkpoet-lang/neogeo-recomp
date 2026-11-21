## 1. Exploration
- [x] 1.1 Read the user references and existing runtime/per-game training hooks.
- [x] 1.2 Record native integration, per-set gates, staged HUD/dummy scope and attribution requirements.
- [ ] 1.3 Pin reference revisions and inspect complete inherited licences before code reuse.
- [x] 1.4a Verify KOF98 battle phases, health/clock, facing and positions through native traces; meter read-only.
- [ ] 1.4b Independently scope KOF95, KOF97, KOF2002 and Magic Plus II.
## 2. Native first slice
- [x] 2.1 Add explicit session entry/exit/reset and practice mutation guards.
- [x] 2.2 Add verified resources, basic dummy and input-history HUD.
- [x] 2.3 Verify off-state regression, state round trips and online/replay blocking.
## 3. Advanced drills
- [x] 3.1 Add bounded dummy slots and deterministic side-relative/random playback.
- [ ] 3.2 Add verified guard/reversal/recovery capabilities per set.
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
- [ ] 5.4b Add non-mutating temporary throw capture and validate throw coverage separately.
- [x] 5.5a Add native Hitboxes page with independent controls and explicit partial coverage.
- [ ] 5.5b Complete the broader Session/Resources/Dummy/Recordings/HUD organisation and capabilities.
- [ ] 5.6 Extend combo/stun, refill policies, random guard/reversal/recovery and optional missions only with native evidence and deterministic state tests.
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
