## ADDED Requirements
### Requirement: Explicit offline training session
The runtime SHALL expose a distinct training session using the shared menu and
SHALL apply practice mutations only while the verified session is active.
#### Scenario: Online or replay entry
- **WHEN** online play or replay playback begins with training preferences enabled
- **THEN** training mutations and dummy input injection are disabled
#### Scenario: Training is off
- **WHEN** the same game input sequence runs without a training session
- **THEN** training UI/readers do not change game state or audio

### Requirement: Verified per-set capabilities
Every supported set SHALL supply independently verified readers/writers and SHALL
hide unsupported training capabilities rather than applying another game's map.
#### Scenario: Bootleg or unverified set
- **WHEN** a variant has no verified training adapter
- **THEN** the UI reports training unavailable for that set
#### Scenario: Unverified frame data
- **WHEN** startup/recovery/hitbox readers are not validated
- **THEN** the HUD does not display invented advantage or collision measurements

### Requirement: Read-only collision inspection
Hitbox visualisation SHALL read verified collision state without writing guest
memory or changing inputs. Capability records SHALL distinguish body, projectile,
throw and throwable coverage per set; missing hook coverage SHALL remain explicit.
#### Scenario: Temporary throw metadata
- **WHEN** a reference implementation clears a memory field or relies on a write/execute callback
- **THEN** the native visualiser uses a verified non-mutating capture hook or marks that coverage unavailable, without copying the clearing write
#### Scenario: Invalid object or reset
- **WHEN** an object address/ID is invalid, a pointer list cycles, the round resets or a state is loaded
- **THEN** bounded readers stop safely and stale rectangles are discarded
#### Scenario: First offline KOF98 slice
- **WHEN** online play or replay playback begins
- **THEN** this training-only visualiser is disabled, with no board-state or input change

### Requirement: Capability-driven native training pages
The native training UI SHALL expose Session, Resources, Dummy, Recordings,
Hitboxes and HUD sections only for verified capabilities, using the existing
menu/controller system rather than requiring a separate Lua installation.
#### Scenario: Unverified resource writer
- **WHEN** a reference acknowledges incomplete MAX/refill support
- **THEN** the native menu does not advertise that writer as verified merely because the reference contains it

### Requirement: Reproducible dummy and reset
Dummy recordings and random practice state SHALL be bounded and deterministic,
and session reset/save/load SHALL restore or clear their counters consistently.
#### Scenario: Side switch
- **WHEN** a verified side-relative dummy sequence is played after the players swap
- **THEN** forward/backward inputs follow the dummy's current facing
#### Scenario: State reload
- **WHEN** a training state is restored
- **THEN** the dummy, random seed and HUD history match the documented restore policy

### Requirement: Safe first-slice resource and checkpoint handling
KOF98 health/clock writers SHALL require verified two-human battle and initialized
living-player resource values. Meter SHALL remain read-only until its write path
is independently verified. Optional training checkpoint data SHALL be bounded
without weakening fixed chip-memory layout checks.
#### Scenario: BIOS, selection or KO
- **WHEN** the board is outside a verified initialized living battle
- **THEN** resource helpers do not write health or clock memory
#### Scenario: Truncated application state
- **WHEN** a state load fails after partially reading training data
- **THEN** both the prior board state and the prior training state are restored

### Requirement: Bounded dummy input slots
The native training controller SHALL provide three input-only slots, each bounded
to1,800 simulated frames. Recording SHALL route P1 gameplay controls to the dummy,
leave P1 neutral and omit operator/coin/Start inputs.
#### Scenario: Recording limit or overwrite
- **WHEN** recording reaches the cap or a populated slot is selected for recording
- **THEN** capture stops at the cap, or replaces only that selected slot respectively
#### Scenario: Side switch during playback
- **WHEN** the dummy changes facing
- **THEN** recorded forward/backward resolve against its current facing on each frame
#### Scenario: Random slot loop
- **WHEN** random playback reaches a slot boundary
- **THEN** only populated slots are eligible and the saved PRNG state determines the next choice without an inserted neutral frame
#### Scenario: Reset and session exit
- **WHEN** the round resets or training ends
- **THEN** recording/playback stop but slot contents remain available in the current process and in explicit save states

### Requirement: Bounded read-only projectile inspection
The verified KOF98 adapter SHALL expose projectile attack and vulnerability
rectangles through an independent offline-only display switch. It SHALL bound
object traversal to 64 distinct entries and discard partial projectile results
on invalid addresses or an unterminated list. It SHALL NOT infer ownership or
claim throw coverage from projectile inspection.
#### Scenario: Projectile expires or a paused state is loaded
- **WHEN** a projectile disappears or the user loads a different battle state while paused
- **THEN** the next presentation reads fresh data without leaving stale rectangles or modifying board state
#### Scenario: Invalid projectile list
- **WHEN** an object pointer is unaligned, outside the verified work-RAM range, or exceeds the bounded traversal
- **THEN** projectile rectangles are omitted while valid player rectangles remain available
#### Scenario: Online or replay viewing
- **WHEN** the session is online or displaying a replay
- **THEN** training collision overlays remain disabled regardless of saved switches

### Requirement: Temporary throw-attempt inspection
The original KOF98 adapter SHALL offer an offline-only throw-attempt switch
separate from body/projectile views. It SHALL capture current player geometry
at the verified write event without clearing game fields or altering the CPU.
It SHALL retain at most one last event per player for the completed frame and
SHALL NOT advertise throwable areas or complete roster coverage.
#### Scenario: Pause and expiration
- **WHEN** a verified event frame is paused or a later frame has no event
- **THEN** the paused frame may retain its boxes, but the later frame has none
#### Scenario: Load, reset or rollback
- **WHEN** state is read, the board resets, or a frame is rerun
- **THEN** stale captures are invalidated and only newly observed valid events may appear
#### Scenario: Training disabled or network/viewer active
- **WHEN** training ends, the throw switch is disabled, or online/replay viewing starts
- **THEN** transient boxes are cleared and no new throw display is produced

### Requirement: Native training navigation
Training SHALL group verified existing controls under Session, Resources, Dummy,
Recordings, Hitboxes and HUD. Navigation changes SHALL preserve stable option
and command IDs, keyboard bindings and runtime behaviour.
#### Scenario: Select a training capability
- **WHEN** the user opens Training
- **THEN** the six sections lead to the existing qualified controls without advertising unsupported features
#### Scenario: Existing settings or dummy slots
- **WHEN** a player uses prior preferences or an existing compatible state
- **THEN** values and recorded inputs remain usable despite the changed menu categories

### Requirement: Optional offline HUD layout
The KOF98 HUD section SHALL provide input visibility/position, status density and
portrait-resource visibility choices with zero values preserving the accepted
defaults. Both native and detailed presentation SHALL use the same panel anchor.
#### Scenario: Paused display adjustment
- **WHEN** the player changes a HUD choice while paused
- **THEN** the next presentation reflects it without advancing gameplay or changing recorded inputs
#### Scenario: Restore defaults
- **WHEN** Restore HUD defaults is selected
- **THEN** local HUD choices reset without modifying the global input preference, dummy, resource aids, slots or hitbox switches
#### Scenario: Leave practice
- **WHEN** offline training ends or online/replay viewing starts
- **THEN** local input overrides and anchors no longer affect the ordinary display

### Requirement: Deterministic random standing guard
The original KOF98 dummy SHALL append a Random guard mode without renumbering
existing modes. It SHALL choose relative back or neutral once per episode started
by P1 attack input or verified body attack, retaining that decision until thirty
quiet frames. Seed and episode SHALL survive compatible state save/load.
#### Scenario: Pause or repeat a state
- **WHEN** presentation is repeated while paused or gameplay resumes from a saved state
- **THEN** presentation does not consume random decisions and replayed inputs reproduce the same guard decisions
#### Scenario: Recorded dummy controls
- **WHEN** dummy recording or playback owns P2 controls
- **THEN** those recorded controls take priority over random guard and the guard episode stops
#### Scenario: Legacy or malformed state
- **WHEN** an earlier supported training state is loaded or guard fields are invalid
- **THEN** the legacy state starts with the default guard seed, while an invalid state fails through the existing atomic rollback path
#### Scenario: Unsupported attack coverage
- **WHEN** a low, delayed projectile or reversal needs a distinct training adapter
- **THEN** the random standing-guard option does not claim automatic coverage for it

### Requirement: Explicit random low guard
The KOF98 dummy SHALL append a random low-guard choice using the same saved
episode and seed. A chosen guard SHALL send relative back plus down; a pass
SHALL send neutral. It SHALL NOT infer attack height automatically.
#### Scenario: Change random posture
- **WHEN** the user changes from random standing guard to random low guard within an episode
- **THEN** the chosen decision and seed remain unchanged while the requested posture changes
#### Scenario: Low attack qualification
- **WHEN** the measured crouching attack is tested under matched reset trials
- **THEN** pass and standing controls retain measured damage while low guard prevents it in the sampled case
#### Scenario: Existing behaviour and state
- **WHEN** mode0-4 or an existing supported state is used
- **THEN** mode IDs, state layout, existing standing decisions and recorded-slot priority remain unchanged

### Requirement: KOF2002-family defender throw-vulnerability view
Both sets SHALL offer a separate read-only reference-derived defender throw
vulnerability area, distinct from transient attacker attempts. Visibility SHALL
respect the reference's status, timer, signed eligibility and proximity predicates.
#### Scenario: Eligibility suppression
- **WHEN** any reference suppression predicate is true or its area is degenerate
- **THEN** the defender area is absent; no successful-throw prediction is fabricated
#### Scenario: Fresh paused inspection
- **WHEN** a state loads, resets or is inspected while paused
- **THEN** the defender area is recomputed from current memory without retaining an event cache or advancing game state

### Requirement: KOF2002-family transient throw-attempt views
Original KOF2002 and Magic Plus II SHALL qualify event-time throw attempts
independently using their reference watchpoint and camera profile. A yellow
attempt area SHALL NOT be described as the opponent's throwability area.
#### Scenario: Transient capture
- **WHEN** a validated player-owned throw write is observed during offline practice
- **THEN** at most one current-frame area per player is retained read-only and compared with the reference's original callback
#### Scenario: Expiry and discontinuity
- **WHEN** a frame expires, a state loads/resets, a toggle/session ends or online/viewer context is active
- **THEN** stale throw events are cleared and cannot reappear merely by re-enabling the option

### Requirement: KOF2002-family player collision views
Original KOF2002 and Magic Plus II SHALL expose read-only player hurt, attack,
guard, push boxes and axes only after each set's native snapshots agree with
the original reference's own profile. Hurt ID23 and phase0x10b056 SHALL be
handled independently of the KOF98 profile.
#### Scenario: Presentation lifecycle
- **WHEN** a user pauses, loads a state or resets during offline practice
- **THEN** the current player geometry is recomputed without advancing game state or retaining stale boxes
#### Scenario: Disabled context or unqualified objects
- **WHEN** practice ends, online/viewer mode is active, or projectile/throw data is unqualified
- **THEN** the player overlay is cleared and no unqualified object view is exposed

### Requirement: Magic Plus II practice adapter
The kf2k2mp2 variant SHALL qualify its own ordinary-input battle/resource/recovery
behaviour before offering the recorded-response workflow. It SHALL use a distinct
state mark, not load the parent's training tail as its own, and retain the same
offline/viewer and explicit-action ownership guards.
#### Scenario: Qualified variant response
- **WHEN** a recorded response runs in the measured variant battle fixture
- **THEN** it uses conservative recovery and Once playback without inferring untested hack mechanics or first-frame reversal timing
#### Scenario: Candidate isolation
- **WHEN** the variant candidate is prepared
- **THEN** accepted original KOF2002/KOF98 candidates and installed packages remain unchanged, and no unqualified collision views are enabled

### Requirement: Original KOF2002 practice adapter
The original kof2002 set SHALL use its own phase, clock, HP, meter-stock and
recovery profile for offline practice, bounded recordings and opt-in conservative
slot responses. It SHALL NOT enable KOF98 collision tables or Magic Plus II.
#### Scenario: Session resources
- **WHEN** verified living human fighters are in the original KOF2002 battle
- **THEN** optional health/time aids use the qualified KOF2002 addresses and limits while meter remains read-only
#### Scenario: State compatibility
- **WHEN** a legacy board-only KOF2002 state or new adapter state is loaded
- **THEN** legacy training data starts empty and new bounded training data restores atomically; another adapter's trailing mark is rejected
#### Scenario: Native response qualification
- **WHEN** recorded recovery responses are exercised in the qualified original-set scenes
- **THEN** selected-slot Once ownership, explicit-action priority and offline/viewer guards follow the existing behavioural contract without claiming first-frame reversal or full-roster coverage

### Requirement: Conservative recovery slot response
Offline KOF98 practice SHALL offer an opt-in selected-slot response after observed
block, hit, airborne-hit recovery or any of those incidents. It SHALL play once,
relative to the dummy's facing, without claiming first-frame reversal timing.
#### Scenario: Recovery matches selected response
- **WHEN** a monitored recovery completes and the selected slot contains inputs
- **THEN** the slot starts in Once mode with its first input on that frame
#### Scenario: Explicit action owns the dummy
- **WHEN** a user recording or explicit playback is running
- **THEN** it has priority and recovery observation is reset rather than interrupting it
#### Scenario: Empty slot or disabled context
- **WHEN** the selected slot is empty, the response is off, or practice is online/in a replay viewer
- **THEN** no new automatic response is started; switching off stops only response-owned playback
#### Scenario: Saved response cursor
- **WHEN** a compatible state restores an active automatic response
- **THEN** playback cursor and bounded response ownership restore through the atomic game state path; earlier marks import no ownership

### Requirement: Recovery detector state foundation
The KOF98 game SHALL serialize the bounded pending recovery incident and elevated-hit
flag in its atomic board/game state path. This foundation SHALL NOT expose an
automatic input or menu option by itself.
#### Scenario: Import earlier training state
- **WHEN** a supported mark54,55 or56 state is loaded
- **THEN** recordings and guard data retain their existing migrations and the recovery detector starts idle
#### Scenario: Invalid recovery fields
- **WHEN** incident enum, boolean encoding, field combination or length is invalid
- **THEN** loading fails and the existing board/game state is restored
#### Scenario: Interaction ends
- **WHEN** training exits, resets, loses battle readiness or yields to explicit slot recording/playback
- **THEN** pending recovery work is cleared without sending an input

### Requirement: Shared Neo Geo current-input artwork
All native Neo Geo frontends SHALL offer one current-input panel per player using
the requested A/B/C/D colours and the recorder's original current-direction
joystick artwork, centred for neutral and displaced for the eight directions.
The panels SHALL read effective board input without polling or modifying controls.
#### Scenario: Release and simultaneous buttons
- **WHEN** a player presses a diagonal and multiple buttons, then releases them
- **THEN** the ball moves to the matching direction and the matching buttons illuminate, returning to centred/dimmed on release without a history list
#### Scenario: Input source or game changes
- **WHEN** keyboard, gamepad, dummy or compatible replay drives a Neo Geo game
- **THEN** the same four-button theme reflects the effective inputs received by that game

### Requirement: Compact fighting-game HUD layout
Input boxes SHALL use the original recorder-rendered artwork without redundant
P1/P2 labels, composed above native game resolution to preserve its finish.
Training panels SHALL sit immediately above the super-meter area, outside the
life bars and central practice status. Verified numeric
HP/POW readouts SHALL appear in compact player-coloured badges above the
character portraits rather than inside the input boxes.
#### Scenario: Training resources are available
- **WHEN** a verified adapter supplies HP/POW values
- **THEN** resource badges appear at the upper portraits without covering the life bars, and current-input boxes above the super meters contain only the joystick and ABCD controls
#### Scenario: Other fighting-game adapter is unverified
- **WHEN** a game has no verified resource readers
- **THEN** the shared visual layout does not fabricate HP/POW values or reuse another set's memory map


### Requirement: Independently qualified KOF2002-family projectiles
Original KOF2002 and Magic Plus II SHALL qualify their projectile views separately,
using bounded read-only traversal and an independent visibility control. Object
list order SHALL NOT be presented as player ownership.
#### Scenario: Live projectile or paused load
- **WHEN** a qualified projectile moves, expires or a paused battle state is loaded
- **THEN** its current reference-derived rectangles refresh without changing board or training state
#### Scenario: Malformed or capped list
- **WHEN** an object pointer is misaligned, outside supported RAM, or traversal reaches its safety cap
- **THEN** incomplete projectile results are discarded while valid player boxes remain
#### Scenario: Display leaves practice
- **WHEN** online, replay viewing, session exit or Hide all boxes applies
- **THEN** projectile overlays are hidden and no inspection writes or automatic inputs occur
