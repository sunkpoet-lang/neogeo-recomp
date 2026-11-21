# Installed FBNeo training reference audit

The user explicitly authorises useful reuse from their installed
`fbneo-training-mode` directory. This authorisation covers investigation and
adaptation, not an assertion that every source has a confirmed redistribution
licence. The installed tree is fingerprinted separately from current upstream;
do not label it the latest release. No installed file is edited.

## Useful components and native integration order

| Component | Reference | Native treatment |
| --- | --- | --- |
| Body hurt/attack, guard and push boxes | `hitboxes/kof-hitboxes.lua` | First KOF98 read-only adapter; verify phase, coordinates, camera, facing and active-bit classification before drawing |
| Projectile boxes | Object pointer list in the same viewer | Bounded list traversal, RAM-range checks, duplicate/cycle detection and native projectile traces |
| Throws/throwable areas | Throw watchpoints and type checks | Separate capability; capture temporary values without modifying board RAM, or label unavailable |
| Training GUI pages | `guipages.lua`, per-game pages | Native sections for Session, Resources, Dummy, Recordings, Hitboxes and HUD; retain existing keyboard/controller navigation |
| Combo/damage/resources | `games/kof98/kof98.lua` | Independently validate each field; source itself warns about MAX support, so do not copy its meter writes as a fix |
| Random guard/reversal/delay | Shared training controller/pages | Extend existing bounded deterministic dummy state, with save/load and online/replay guards |
| Missions and drills | `addon/missions.lua` | Later optional data-driven drills; importing the Lua execution environment is not required |
| Input artwork/history | `inputs`, `resources` | Existing current-only recorder design remains approved; do not restore an unwanted history list |

The upstream README identifies the project as peon2's independent FBNeo-targeted
Lua training suite, not an officially endorsed part of FBNeo. Source references:
[project](https://github.com/peon2/fbneo-training-mode),
[KOF viewer](https://github.com/peon2/fbneo-training-mode/blob/master/hitboxes/kof-hitboxes.lua).

## Critical differences to preserve

- The Lua viewer uses emulator memory access, callbacks, CPU registers and
  watchpoints. Loading `guipages.lua` into a standalone executable is not native
  integration and does not produce these hooks automatically.
- Its fallback throw reader calls `memory.writebyte(...,0)` when `clear` is true.
  Do not transplant this into a visual overlay. Hitbox display must be read-only.
- KOF98 uses a write watchpoint at `0x10EBDF` for temporary throw data. An end-frame
  snapshot alone is not proof of complete throw coverage.
- Lua coordinates depend on camera offsets, signed byte/word fields, facing,
  the16-pixel ground offset and emulator picture margins. Boxes must match the
  actual native display, including jumps and camera motion.
- Profiles distinguish KOF95,97,98 and2002. Magic Plus II needs its own checked
  profile; a parent relationship does not establish an identical memory map.
- No licence file or clear file-level licence was found in the inspected local
  subset or upstream root listing. Preserve authorship/source fingerprints and
  resolve inherited terms before publishing literal source adaptations. Continue
  independent native work from verified memory facts in the meantime.

## First deliverable and acceptance gates

KOF98 first: an offline-only, read-only body-box inspector with independent toggles
for hurt, attack, push and axes. Projectile and throw coverage remain separately
labelled. No invented boxes/frame advantage or universal game support.

Verify real native scenes: idle, walk, crouch, normal/special attack, jump,
facing switch, camera movement, projectile, KO and new round. Compare boxes to
the actual collision state/reference where available, not merely the same reader
twice. Reject invalid IDs/addresses and bound object counts. Confirm state hashes
and WAV with overlay off/on, clean save/load/seek, no stale boxes after reset, and
online/replay exclusion for this first training slice.

Then expose native GUI sections and restore/hide/move HUD controls, retaining the
approved bottom input panels and upper portrait resources. Per-game capability
records drive which features appear; never show unsupported switches as working.

## Current status

This audit/specification is complete; hitbox runtime and GUI expansion are not
implemented by this document. Private trace/source receipts are in
`work/training-fbneo-audit`; source-only hashes are recorded alongside this audit.
The read-only probe informs the next adapter investigation, not hitbox acceptance.
