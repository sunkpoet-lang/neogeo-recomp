# Validation report

Date: 2026-10-29 (America/Mexico_City). OpenSpec configuration was preserved; the change was specified and passed
`openspec validate neo-kof2002-decrypt --strict` before implementation. No commit or push was made during that
implementation run; publication was subsequently authorised separately.

## Reference and algorithm checks

Reference: MAME revision `8c285468743535fbbde018fef23b3cfb8c9a3ec8`, BSD-3-Clause, S. Smith, David Haywood,
Fabio Priuli. Files: `src/devices/bus/neogeo/prot_cmc.cpp`, `prot_kof2k2.cpp`, `prot_pcm2.cpp`, `kof2k2.cpp`, and
`src/mame/snk/neogeo.cpp`. The ROM definition confirms the encrypted M1 is 0x20000 bytes in a zero-initialised
0x80000 region without repetition. The M1 key is a byte sum, reduced modulo 65536. Audio bank setup skips MAME's
0x10000 prefix and applies an address mask of `(linear_size - 1) & 0x3ffff`.

`tests/mame_decrypt_reference.py` compiled the upstream algorithm bodies separately from the port and ran two
xorshift-generated artificial cartridges. Expected CRC32 values, in P/C/S/M/V order:

| Seed | P | C | S | M | V |
| --- | --- | --- | --- | --- | --- |
| 12345678 | f6e98944 | 99c72e26 | f7903932 | 7a861d99 | 3f25635a |
| deadbeef | 95812652 | e656e0b8 | e5cea0c6 | 4e79da13 | f187d379 |

The integrated implementation matches all ten CRCs. Fixtures contain no ROM data. Reference source SHA256:

- prot_cmc.cpp: `f6b618e70a1e6dcdcbbed1ee9c3fbc352dd195a895baf2fbb2e22ee413e3bf84`
- prot_kof2k2.cpp: `2805e6b2e3fb76ff8ae9ef28d897670613d134941afbeec4476d8d652c0a6ed0`
- prot_pcm2.cpp: `c9b0aa2e24d39476cc9c860455dfaec8795741338d0964fb88e41807cd5804bc`

## Automated integration tests

Built KOF2002 and KOF97 with MSYS2 UCRT64 GCC 16.2.0, CMake and Ninja. Existing warnings in frontend and
GekkoNet code remain; the new decryption/test files compile without warnings under the configured flags.

`ctest --test-dir build/kof2002 --output-on-failure`: 1/1 passed (6.47 seconds). This executable covers:

- Both full-size synthetic cartridges against upstream CRCs for all five transformed regions.
- M1 output size, zero tail, and absence of a duplicated audio prefix.
- Unexpected sizes for each input region, including expanded M1 and a supplied S1, rejected before mutation.
- Actual loader word swapping and C-pair interleaving before decryption, with synthetic file CRC verification.
- Another set ID bypassing all transforms and a corrupt M1 rejected by the loader.
- 256 selections for each of the four Z80 bank windows, on both 128 KiB and 512 KiB linear M1 data, including
  window boundaries, high banks and switching the fixed window between BIOS and cartridge.

## KOF97 regression

Before implementation, the existing executable ran the local original set for 3000 frames without scripted input.
After rebuilding, the same command and initial state produced byte-identical frame hashes, WAV output and all
three screenshots. Both executions logged the same startup watchdog reset at frame 247 and ended at state hash
`e61c6f3c`.

- Complete 3000-frame hash log SHA256: `ff1a51b18fa1112e16fffd7215ffb351cb0ef5f3531893ba8e1624a19d34ab1c`.
- Complete WAV SHA256: `ca16b20791c08f664fcf0fa66d50c3e163b09a832e72adc6d33a5168ec465bd8`.
- Screenshots at frames 1000, 2000 and 3000 match byte for byte.

Generated evidence: ignored `work/decrypt-check/baseline/` and `work/decrypt-check/after/`.

## KOF2002 real-set smoke checks

The existing user-supplied `games/kof2002/kof2002.zip` and adjacent BIOS passed the configured CRC checks.
The generated game header recognises kof2002, and both loading and runtime decryption work with that zip.

- 3000-frame attract run: final state `eb3961f6`, zero watchdog resets. Visually inspected decoded attract graphics
  and the title screen at frame 3000.
- 6000-frame run with coin at 3100 and Start at 3250: final state `e7e12348`, zero watchdog resets. Visually inspected
  the tutorial and character selection.
- 9000-frame run with additional A presses at 6100, 6150 and 6200: final state `2a27d18b`, zero watchdog resets.
  Visually inspected an active fight with decoded stage, sprites, portraits, FIX text and HUD at frame 9000.
- The 3000-frame WAV contains stereo 55555 Hz output, peak 16258, RMS 1897.25 and 4,468,116 nonzero samples.
  This verifies sound generation; no subjective listening assessment was performed.

Reproduce with the commands in `games/kof2002/README.md`; the 9000-frame extension uses
`--script "3100:coin1:1,3250:start1:8,6100:a1:1,6150:a1:1,6200:a1:1"`.
Evidence lives under ignored `work/decrypt-check/kof2002/`, `kof2002-play/` and `kof2002-fight/`.

## Limits and preserved local state

Two unhandled startup write addresses are logged by the board: `25fff0` (PC `008c02`) and `00000c` (PC `00aa2a`).
Neither prevents title, selection or fighting in these runs; this change does not claim complete board accuracy.
No full playthrough, subjective audio audition or online interoperability test was performed. No performance
claim is inferred from the local headless timing.

The initial untracked `neogeo-recomp.rar` and the player's ROM zips remain untouched. The original game.cfg was
retained. No OpenSpec installation/reinitialisation or global skill installation was performed. Only the original
kof2002 set is supported; bootlegs and predecrypted variants are outside this change.

## Publication preflight (2026-10-29)

After the user separately authorised committing and publishing, both games were rebuilt. CTest passed both
`neogeo_decrypt` and `neogeo_startup` (2/2). A fresh 3000-frame KOF97 run matched the pre-change hash log and WAV
byte for byte, ending at `e61c6f3c`. KOF2002 completed another 3000 frames with exit 0 and state `eb3961f6`.
Both OpenSpec changes passed strict validation. These checks do not establish Internet netplay behaviour.
