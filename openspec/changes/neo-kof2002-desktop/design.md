# Design

## Context

See `proposal.md` for motivation. `port/neogeo_port.cpp` calls its internal `FindRom` only when `opt.rom` is empty. The current helper checks executable-relative `roms`, then working-directory `roms`, then the working-directory ZIP. The installed collection instead keeps its set in `ROM`; launcher startup works because the launcher supplies `--rom`.

`CMakeLists.txt` already embeds `games/<id>/icon.ico` when present on Windows. KOF97 has a seven-size ICO; KOF2002 has none. The user's established transformation is in `D:/CPS2/iconos/make_icons.py`, which uses actual title-logo images, crops their background, preserves aspect ratio and centres the logo. That script's hard-coded paths mean it must not be run wholesale for this task.

This design spans startup discovery, a Windows presentation asset and a portable deployment, so decisions about precedence and verification are recorded explicitly.

## Goals / Non-Goals

**Goals:** a small additive startup fix, an authentic title-logo ICO, verifiable executable resources and a reversible update to the existing test package.

**Non-Goals:** launcher redesign, auto-downloading or moving ROMs, bootleg support, reading or rewriting another launcher's settings, changes to decryption or CPU execution, and chat/network/replay work.

## Decisions

### Discovery stays game-local and deterministic

Keep explicit `--rom` handling unchanged. For automatic discovery, check executable-relative `roms/<set>.zip`, `ROM/<set>.zip`, `rom/<set>.zip`, and `<set>.zip`, in that order; then retain the existing working-directory `roms/<set>.zip` and `<set>.zip` fallbacks. This preserves the existing executable-relative first choice and makes a portable folder work from an unrelated working directory. Capitalisation matters on case-sensitive systems; Windows aliases `ROM` and `rom` naturally.

Use `SDL_GetBasePath` as the executable directory. Do not hard-code E: or the local checkout. Keep validation in `LoadNeoGeoSet`: do not treat an existing but invalid ROM as permission to silently select another copy. Retain the current missing-set fallback path when none exists so error semantics remain familiar. Inspect whether the existing existence helper needs a file-only check without changing its other call sites.

Alternative: reading the launcher's remembered ROM path would couple the standalone game to application IDs, portable preference layouts and external settings; it is unnecessary for the reported collection layout. Renaming the ROM folder would break the rest of the collection.

### Use the established title-logo pipeline

Use an actual KOF2002 title-screen image, already available among the ignored validation captures, as the input. Crop the title logo rather than the full screen including credits or INSERT COIN, then apply the established square-icon treatment and seven sizes. Store the approved presentation asset at `games/kof2002/icon.ico`; keep intermediate full game captures under ignored `work/`. Do not invent or regenerate the logo with AI.

Reuse the existing CMake RC hook. Reconfigure after adding the ICO, then inspect the resulting executable's icon group and image resources. Windows Explorer can retain the old icon in its cache; a cached shell view alone does not determine whether embedding succeeded. No system-wide cache reset is required.

### Verify actual launch modes separately

Exercise automatic discovery with no `--rom` from an unrelated working directory. Test legacy layout and competing candidates with focused discovery/integration checks, including an explicit invalid ROM path to prove it is not ignored. Use local user ROMs only for ignored smoke evidence. Recheck KOF2002 startup/frame hashes and KOF97 regression after rebuilding; a folder-selection change must not change simulation.

The existing launcher already starts KOF2002 correctly. Verify that path again after replacing the game binary, without rebuilding or changing the launcher.

## Risks / Trade-offs

- Multiple copies of a set can cause surprise selection: document and test precedence, and retain explicit-path authority.
- Windows case-insensitivity can hide Linux path defects: include distinct candidate-layout checks rather than assuming casing is interchangeable everywhere.
- Explorer can show a stale icon: inspect executable resources and compare the deployed binary with the verified build.
- Existing uncommitted decryption work is part of the current baseline: do not reset, commit or refactor it while fixing desktop integration.
- Shared DLL changes can affect other games: use the target's existing DLLs, already shown to work with KOF2002.

## Migration Plan

Build and validate in `D:/CPS2/neogeo-recomp`. Before deployment, ensure no test-launched instance is using the target file; do not terminate unrelated player sessions. Back up the existing KOF2002 executable and any documentation being replaced under a new directory inside `_respaldo_online_cps` in the E: collection. Replace only KOF2002 and its affected documentation, keeping the existing launcher, ROM folder, tunnel, DLLs and settings. Verify direct startup without a ROM argument and launcher startup. If deployment verification fails, restore the backed-up files. Update the shared handoff to show Codex completed the work while Claude had no tokens.
