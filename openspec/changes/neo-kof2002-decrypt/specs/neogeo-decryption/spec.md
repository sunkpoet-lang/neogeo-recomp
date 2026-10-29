## ADDED Requirements

### Requirement: Original cartridge decryption
The loader SHALL decrypt only the original kof2002 set after ROM CRC verification, applying P2 permutation, CMC50 graphics with key 0xEC, SFIX extraction, CMC50 M1 and PCM2 type 0 before board construction.

#### Scenario: Valid original set
- **WHEN** the original set is loaded
- **THEN** program, sprites, FIX, linear M1 and samples SHALL match MAME's decoded data

### Requirement: M1 layout and banking
The loader SHALL zero-extend the 128 KB encrypted M1 to 512 KB and calculate the key as a byte sum modulo 65536 over its first 64 KB. It SHALL expose linear decrypted bytes without MAME's 64 KB audio-region prefix.

#### Scenario: Banked sound reads
- **WHEN** the Z80 selects any of its four cartridge bank windows
- **THEN** reads SHALL address the corresponding linear decrypted M1 bytes, using MAME's (linear_size - 1) & 0x3ffff bank mask

### Requirement: Validation and isolation
Unsupported KOF2002 region sizes SHALL fail with a useful error before any decryption mutation. Other sets SHALL retain their existing loading behaviour.

#### Scenario: Invalid cartridge layout
- **WHEN** an original KOF2002 region has an unexpected size
- **THEN** decryption SHALL return failure without modifying any region

#### Scenario: KOF97 regression
- **WHEN** KOF97 runs with the same ROMs, inputs and initial state before and after integration
- **THEN** frame hashes and generated audio SHALL remain identical

### Requirement: Boot verification
The implementation SHALL include reproducible algorithm checks and real-set checks where local ROMs are available.

#### Scenario: Title screen
- **WHEN** the original KOF2002 set runs through startup
- **THEN** it SHALL reach the title or attract screen with decoded graphics and working sound
