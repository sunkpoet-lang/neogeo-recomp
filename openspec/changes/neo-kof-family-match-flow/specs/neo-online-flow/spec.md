## ADDED Requirements

### Requirement: Independently accept each additional KOF variant
ForkAde flow SHALL be enabled explicitly for each verified kof2002, kf2k2mp2
and kof98 adapter without changing normal execution or cartridge transforms.
Each adapter SHALL read native completed-human-team counters, with its own
runtime evidence and rejection of attract, CPU play and individual KOs.

#### Scenario: A variant resembles its parent
- **WHEN** its parent already supports match flow
- **THEN** the variant still requires its own result and regression evidence before acceptance

### Requirement: Preserve session behaviour and previous games
Each accepted variant SHALL retain native owned hosting, clean admission,
independent Start, names/Hide, rollback-safe streaks and bounded same-peer
recovery. Backup/settings and default legacy netplay SHALL remain intact.

#### Scenario: A returning player reconnects
- **WHEN** an established peer returns inside the fixed recovery window
- **THEN** both restart the initial board and pass3600 recovered frames with matching saved checkpoints

### Requirement: Bind acceptance to actual builds
Every game SHALL have identified pre-build references, source/recipe/binary
fingerprints and isolated retained outputs. Missing or failed evidence SHALL
remain pending rather than being inherited from a sibling game.

#### Scenario: The new game passes while another does not
- **WHEN** only one adapter satisfies all its checks
- **THEN** only that game's task is marked complete and the other remains pending
