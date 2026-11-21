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

Independent bounded native traversal matches the original Lua on37 Iori-scene
RAM snapshots (291 rectangles, including73 projectile rectangles). Native
inspection remains read-only across160 frames, paused load and display toggles.
Old/off/on900-frame hashes and WAV match;300-frame rollback stress matches.
Full CTest11/11 passed. See [aggregate receipt](projectiles-receipt.json).
No new Internet acceptance, throw capture or full roster coverage is claimed.
