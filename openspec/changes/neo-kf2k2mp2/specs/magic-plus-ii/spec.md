## ADDED Requirements

### Requirement: Exact variant loading
The runtime SHALL recognise kf2k2mp2 as a distinct cartridge and resolve missing shared members from its adjacent parent and BIOS archives, validating existing CRC rules.

#### Scenario: Split variant
- **WHEN** the player supplies kf2k2mp2.zip, kof2002.zip and neogeo.zip
- **THEN** the variant's own P and S1 and the required shared C, M1 and V regions are loaded without modifying the archives

### Requirement: Variant-specific transformation
The runtime SHALL validate all region sizes before mutation and perform Magic Plus II relocation, S1 swap, CMC50 and PCM2 transformations matching the pinned MAME reference.

#### Scenario: Valid layout
- **WHEN** the exact variant layout is loaded
- **THEN** its transformed region checksums match the independent upstream-body oracle on synthetic input and M1 uses the existing linear Z80 representation

#### Scenario: Invalid layout
- **WHEN** any required region has an unsupported size
- **THEN** decryption fails with the variant identified and no region modified

### Requirement: Isolation and acceptance
The implementation SHALL retain original KOF2002 and KOF97 behaviour, separate synthetic checks from real gameplay evidence and preserve the deployed beta.

#### Scenario: Regression check
- **WHEN** the same original sets and scripted inputs run before and after the shared changes
- **THEN** frame hashes and audio match their recorded baselines

#### Scenario: Variant executable
- **WHEN** the variant is delivered for local testing
- **THEN** boot, graphics and sound evidence and remaining limits are recorded and its own title-based icon is embedded
