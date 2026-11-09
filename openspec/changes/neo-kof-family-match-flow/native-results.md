# Native result observations

Read-only probes, 6 November 2026. Each game ran with its own ROM profile,
without the new flow enabled, without cheats or memory writes. The exact
commands, copied EXEs, RAM logs and screenshots are retained locally under
`build/kof-family-reader-probes-20261106/<game>/{p1,p2}`. Raw game states and
screenshots are not source/release assets.

| Set | Phase bus address | P1/P2 team-win bytes | P1 increment frames | P2 increment frame |
|---|---|---|---|---|
| kof2002 | 0x10B056 | 0x10DAB7 / 0x10DAB9 | 15230, 28589 | 10068 |
| kf2k2mp2 | 0x10B056 | 0x10DAB7 / 0x10DAB9 | 15230, 28600 | 15050 |
| kof98 | 0x10B094 | 0x10DAF9 / 0x10DAFB | 15128, 29343 | 15197 |

P1 traces ran 34000 frames and P2 traces 18000. During human fights BIOS mode
0x10FDAF was 2, and player modes 0x10FDB6/7 were both 1. Fight phases 1/2
are observed for KOF2002; phase 2 suffices for the verified MP2/KOF98 traces.
Score increments follow completed teams, not individual character health KOs.
The opposite winner remains unchanged in each trace. KOF98 increments arrive
29 frames after its phase clears, so scoring must retain the armed human bout
rather than requiring an active-fight phase in the increment frame.

## Source leads and independent verification

[BIOS RAM locations](https://wiki.neogeodev.org/index.php?title=BIOS_RAM_locations&oldid=9321)
documents user/player modes. Fightcade detector facts for
[KOF2002](https://github.com/fightcadeorg/fightcade-detectors/blob/main/kof2002.inf),
[MP2](https://github.com/fightcadeorg/fightcade-detectors/blob/main/kf2k2mp2.inf)
and [KOF98](https://github.com/fightcadeorg/fightcade-detectors/blob/main/kof98.inf)
suggested raw offsets. Native byte bus reads require XOR1 against those raw
word-swapped offsets; adjacent even bytes were checked and did not count.
The local FBNeo training hitbox documentation suggested the phase addresses.
These are address facts used as leads: no Lua, detector implementation, game
program bytes or emulator routine was copied. Acceptance uses the native
runtime observations above and each production callback's own tests.

The same result addresses in MP2 and its parent do not substitute for MP2's
independent traces. These reader observations alone do not certify networking,
rollback, native ownership or installed-launcher operation.
