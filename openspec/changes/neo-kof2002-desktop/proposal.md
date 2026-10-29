# Proposal

## Why

KOF2002 works from the collection launcher, but double-click startup looks for `roms/kof2002.zip` while the installed collection uses `ROM/kof2002.zip`. Its executable also lacks the title-screen icon required by the user's existing game presentation convention.

## What Changes

- Extend automatic Neo Geo ROM discovery to the collection's `ROM` folder beside the executable while preserving existing `roms` and working-directory layouts and explicit `--rom` arguments.
- Add KOF2002's actual title-screen logo as a multi-resolution Windows icon, embedded through the existing CMake resource integration.
- Verify direct startup independently of the launcher's configured ROM argument; document both supported launch paths.
- Rebuild KOF2002, back up the installed executable and replace only the necessary files in the authorised E: test collection.

## Capabilities

### New Capabilities

- `neogeo-desktop`: automatic game-ROM discovery and the executable's game-specific Windows icon.

### Modified Capabilities

None. Existing collection launch arguments and game behaviour remain covered by their current specifications.

## Impact

- `port/neogeo_port.cpp`, the startup notes in `port/neogeo_main.cpp`, and `games/kof2002/README.md`.
- New `games/kof2002/icon.ico`; reuse the existing `CMakeLists.txt` Windows resource hook unless verification reveals a specific defect.
- Targeted discovery checks and actual executable smoke checks, including KOF97 regression.
- Deployment target: `E:/CPS TEST/CPS-LAUNCHER-ONLINE-PRUEBA/CPS-LAUNCHER/kof2002.exe`; preserve its launcher, shared DLLs, ROMs, settings and other games.
- No changes to decryption, CPU execution, network protocol, chat, profiles or replays. No commit or push.
