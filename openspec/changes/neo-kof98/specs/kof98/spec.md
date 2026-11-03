## ADDED Requirements

### Requirement: Exact original cartridge
The loader SHALL validate and decrypt the original KOF98 layout independently of KOF2002 and its bootlegs.

#### Scenario: Original set
- **WHEN** kof98.zip and neogeo.zip contain all declared members
- **THEN** program bytes match the pinned MAME transform and unencrypted regions are unchanged

#### Scenario: Invalid regions
- **WHEN** a required region has the wrong size
- **THEN** the loader rejects it without partial decryption

### Requirement: Stateful header protection
The KOF98 board SHALL implement the two header overlay states, original-word fallback and deterministic save/load while leaving other sets unaffected.

#### Scenario: Protection command and rollback
- **WHEN** 0x0090 or 0x00f0 is written at 0x20aaaa and a state is saved/restored
- **THEN** subsequent 0x100/0x102 reads reflect the restored protection state and byte lanes

### Requirement: Complete isolated ForkAde candidate
The candidate SHALL include KOF98 and Magic Plus II executables, own embedded title icons, launcher entries, GekkoNet online controls and dependencies alongside existing games, without ROMs/BIOS/player data.

#### Scenario: Local and online entry
- **WHEN** either new game is selected in the candidate launcher
- **THEN** its executable, ROM/BIOS requirements and online arguments are resolved consistently and local deterministic/peer tests are documented separately from Internet acceptance
