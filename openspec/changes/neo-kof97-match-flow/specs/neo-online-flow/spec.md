## ADDED Requirements

### Requirement: Enable KOF97 match flow explicitly
The port SHALL provide explicit KOF97-only ForkAde flow without changing
normal offline execution, default peer wire or accepting unverified games.

#### Scenario: Existing player starts normally
- **WHEN** no ForkAde opt-in is supplied
- **THEN** offline state/audio and default netplay retain their existing behaviour

#### Scenario: Another Neo Geo set requests this slice
- **WHEN** the opt-in is requested for a game other than KOF97
- **THEN** it is refused without claiming that game's support

### Requirement: Host and admit a challenger safely
KOF97 SHALL own one native hosted session, expose its code, permit solo arcade
while waiting and restore the same clean initial state for both admitted peers.
Names and independent Start prompts SHALL be presentation-only. Player backup
and settings SHALL be retained across Stop and online state SHALL not persist.

#### Scenario: Challenger arrives after solo play
- **WHEN** a compatible peer joins the waiting host
- **THEN** solo progress is discarded for the match and both begin from the initial state

### Requirement: Present only verified team-match wins
Streaks SHALL count verified completed human team matches once, restore across
rollback and exclude attract, CPU-only play and individual character defeats.

#### Scenario: One teammate loses
- **WHEN** another teammate remains in the same match
- **THEN** no completed-match streak is awarded

### Requirement: Recover the same session with bounded retries
An opted-in established pair SHALL keep its peer, native owner and reliable
chat while attempting recovery for at most60 seconds. Both SHALL reset through
a versioned epoch barrier; stale data SHALL be ignored and desync SHALL end play.

#### Scenario: A brief interruption ends
- **WHEN** the same peer returns within the retry window
- **THEN** both restart the initial match without accepting previous-epoch packets
- **AND** at least3600 recovered frames have matching board and presentation checkpoints
