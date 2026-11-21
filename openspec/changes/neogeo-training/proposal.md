# Why
Before this change the KOF candidates exposed shared menus and some cheats, but
no native training session. Players need a practice entry point, measurements and a
configurable dummy without installing Fightcade scripts.

# What Changes
- Research the user-provided willococa extension and Tommy's training contract.
- Reuse the existing `Game::inTraining`, `kFeatureTraining`, feature/settings and
  overlay hooks; add set-specific verified adapters instead of a universal RAM map.
- Implement verified KOF98 session entry/exit, reset, health/time, read-only meter,
  current-input controls, basic dummy and bounded recording slots.
- Add read-only KOF98 player boxes and a native Hitboxes page; use the installed
  FBNeo training suite as researched reference with attribution and per-file hashes.
- Keep projectiles/throws, broader GUI sections, reversals and per-set adapters
  as separately validated work; do not import an unrestricted Lua execution host.
- Disable state-changing training aids during online play and replay playback.
- Treat KOF98 as the best-documented reference; independently validate KOF95,
  KOF97, KOF2002 and Magic Plus II before claiming support.

# Capabilities
## New Capabilities
- `neogeo-training`: native offline training sessions, per-set capability gates and
  a consistent HUD/menu that does not pretend unsupported measurements exist.

# Impact
The implemented KOF98 slices touch native training/presentation hooks, its adapter,
menus and isolated test candidates. Their measured scope and remaining gates are
in tasks.md and validation.md. Other game adapters and the full training suite
are not complete. No Lua installer/runtime or installed launcher update is added;
online replays remain documented separately under `neo-kof95`.
