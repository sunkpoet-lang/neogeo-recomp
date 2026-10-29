## Why
The original KOF 2002 cartridge cannot boot from encrypted raw ROMs. Add its five protection transforms while preserving existing KOF97 loading and sound banking.

## What Changes
- Recognise the original kof2002 set and decrypt validated cartridge regions after loading.
- Port MAME's P2 block permutation, CMC50 graphics (key EC), SFIX extraction, M1 address cipher and PCM2 type 0.
- Keep the board's linear M1 representation; do not insert MAME's audio-region prefix.
- Add synthetic algorithm, loader, Z80 banking and real-set boot/regression checks.

## Capabilities
### New Capabilities
- `neogeo-decryption`: validated runtime KOF 2002 cartridge decryption.
### Modified Capabilities
None.

## Impact
machine/romset.cpp, new machine/neogeo_decrypt.*, CMake, tests, game documentation and third-party notices. Existing OpenSpec configuration remains unchanged. No ROM data is versioned.
