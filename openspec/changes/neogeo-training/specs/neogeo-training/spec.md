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
