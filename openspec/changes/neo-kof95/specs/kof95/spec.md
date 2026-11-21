## ADDED Requirements
### Requirement: Exact original cartridge layout
The loader SHALL validate the original kof95 MVS set and assemble its continued program and sparse sample/sprite regions without modifying source archives or changing other sets.
#### Scenario: Original set
- **WHEN** matching files are supplied
- **THEN** P1 halves map to MAME addresses and V/C gaps are preserved
#### Scenario: Malformed layout
- **WHEN** a required region has an unexpected size
- **THEN** assembly fails before mutating cartridge data
### Requirement: Shared game baseline
The game SHALL remain incomplete until exact states, rollback, menu, format-2 replay/viewer, names, volume, applicable unlock, online spectators/chat and launcher gates are demonstrated.
#### Scenario: Offline image only
- **WHEN** title and gameplay are captured
- **THEN** evidence records offline bring-up without claiming online or full baseline completion
#### Scenario: Release review
- **WHEN** a release is proposed
- **THEN** deterministic state/replay and peer/spectator checks and launcher discovery are documented with source and executable identity

### Requirement: Local format-2 recordings
The game SHALL automatically record verified two-human local versus segments and SHALL offer manual offline recording. Files SHALL identify the game, backend and compatible state/gameplay source. Recording SHALL use a separate writable folder without changing ROM archives.
#### Scenario: CPU-only game
- **WHEN** a player plays against the CPU or attract mode runs
- **THEN** no automatic versus replay is created
#### Scenario: Human versus ends
- **WHEN** a two-human versus segment ends normally
- **THEN** its recording is finalised with an initial state, input changes and periodic/final checksums
#### Scenario: Online session starts
- **WHEN** the current implementation starts netplay
- **THEN** offline recording stops and online capture uses only the confirmed-input prefix whose state-hash chain matches simulation
#### Scenario: Disconnect with an unresolved prediction
- **WHEN** the latest simulation does not match a confirmed input or its preceding state
- **THEN** finalisation truncates before that frame and writes the checksum of the last accepted frame
#### Scenario: Session or recovery boundary
- **WHEN** an active recorded session ends or is replaced
- **THEN** the bounded confirmed segment is finalised before the SDK recording is destroyed, without claiming a verified match result

### Requirement: Compatible replay viewer
The viewer SHALL expose pause, steps, forward/rewind speeds, timeline jumps, round markers and A–B loops. Playback SHALL restore the prior local game state when stopped and SHALL refuse incompatible or structurally invalid files before restoring their state.
#### Scenario: Navigation
- **WHEN** a scripted viewer seeks, steps or loops within a valid replay
- **THEN** each visited game state matches the corresponding linear playback state
#### Scenario: Damaged or incompatible file
- **WHEN** the identity, state layout, input bounds or checksum is invalid
- **THEN** playback reports failure rather than silently accepting a divergent game
