# Sampled projectile coverage

Updated 24 November 2026. The table applies independently to original KOF2002
and Magic Plus II; each set was tested rather than inheriting the other's results.

| Normally selected fighter/input | P1 facing right | P1 facing left | P2 facing left | P2 facing right |
| --- | --- | --- | --- | --- |
| Athena, quarter-circle-back + A | Verified sample | Verified sample | Verified sample | Verified sample |
| Athena, quarter-circle-back + C | Verified sample | Verified sample | Verified sample | Verified sample |
| Terry, quarter-circle-forward + A | Verified sample | Verified sample | Verified sample | Verified sample |
| Kensou, quarter-circle-back + A/C | Verified samples | Verified samples | Verified samples | Verified samples |
| Andy, quarter-circle-back + A/C | Verified samples | Verified samples | Verified samples | Verified samples |

A verified sample means actual native snapshots agree with the pinned original
Lua geometry, visibility-off/on trajectories preserve hashes and PCM, and native
presentation preserves board hash and complete serialised game state. The
orientations are initial fixture orientations reached through ordinary controls.
P2 input routing uses bounded dummy recording with P1 checked neutral; it does
not represent physical-controller or Internet acceptance. Projectile ownership is
not inferred from the object list.

Each cell is a sampled input exercise, not every move/strength/timing, full-roster,
repeated-crossing, collision-outcome or frame-advantage certification. Prior Iori
evidence remains separate; no untested matrix cells are implied for him.

## Evidence by set

### kof2002

- [P1 initial right, Athena](kof2002-projectile-athena-receipt.json)
- [P1 initial right, Terry](kof2002-projectile-roster-receipt.json)
- [P1 initial left](kof2002-projectile-opposite-receipt.json)
- [P2 initial left](kof2002-projectile-p2-receipt.json)
- [P2 initial right](kof2002-projectile-p2-opposite-receipt.json)

Accepted executable SHA-256 remains `ab85e14888d2845ea3bae16073a0195d3155488e3e558599b046e7fee2826893`.

### kf2k2mp2

- [P1 initial right, Athena](kf2k2mp2-projectile-athena-receipt.json)
- [P1 initial right, Terry](kf2k2mp2-projectile-roster-receipt.json)
- [P1 initial left](kf2k2mp2-projectile-opposite-receipt.json)
- [P2 initial left](kf2k2mp2-projectile-p2-receipt.json)
- [P2 initial right](kf2k2mp2-projectile-p2-opposite-receipt.json)

Accepted executable SHA-256 remains `752c0220018852d2d4e0b797115969a96cef106d625a1626cec9b18bb6254ded`.

No production game rebuild or installed-package change was needed for this expansion.
Raw snapshots, saves, PCM and Lua wrappers remain private ignored work data.

## Additional first-orientation samples

- [KOF2002 Kensou/Andy](kof2002-projectile-extra-roster-receipt.json)
- [Magic Plus II Kensou/Andy](kf2k2mp2-projectile-extra-roster-receipt.json)

## Kensou/Andy remaining initial role/facing cells

- [KOF2002 expanded matrix](kof2002-projectile-extra-matrix-receipt.json)
- [Magic Plus II expanded matrix](kf2k2mp2-projectile-extra-matrix-receipt.json)
