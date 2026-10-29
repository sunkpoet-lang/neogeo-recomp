# Validation report

Date: 2026-10-29 (America/Mexico_City). Change: neo-kof2002-desktop.

## Delivered behaviour

Automatic startup now checks the executable directory's roms/, ROM/, rom/ and its own ZIP, then the existing
working-directory fallbacks. An explicit --rom remains authoritative. Discovery accepts regular files, so a
directory named like a ZIP is not selected as an archive. No decryption or simulation logic changed.

The Windows executable contains the actual KOF2002 title-screen logo, cropped from the previously inspected
frame-3000 capture and processed with the existing square() function in D:/CPS2/iconos/make_icons.py. Only that
function was reused; the hard-coded batch main was not run. Intermediate captures remain ignored under work/.

## Automated checks

- CTest: 2/2 passed. Synthetic decryption/banking/loader test: 6.09 seconds. Startup process integration test:
  10.31 seconds. The startup test launches the actual game with synthetic invalid ZIP fixtures and requires no ROMs.
- Ten discovery scenarios passed: executable-relative ROM, lowercase rom, legacy roms, ZIP beside the executable,
  working-directory roms, working-directory ZIP, roms precedence, executable-before-working-directory precedence,
  explicit invalid-path authority, and missing-set diagnostics.
- The startup test normalises Windows path casing for comparisons; a lowercase rom folder is validly found through
  the earlier ROM spelling on a case-insensitive filesystem. Working-directory diagnostics are correctly relative.
- Parsed the built PE resources with pefile. One icon group, seven images, sizes
  16, 24, 32, 48, 64, 128, 256. Every embedded image is byte-identical to its source ICO entry.
  Preview: work/desktop-check/kof2002-icon-preview.png; details: work/desktop-check/icon-resources.json.
- KOF2002 automatic and explicit-path runs each completed 3000 frames without watchdog resets. All frame hashes
  match each other and the previous decryption baseline; final state eb3961f6.
  Hash-log SHA256: 3199ade01c66422645f28825d614efa1a71864da2e6a6c6522fe5dd11153cd98.
- Visually inspected the automatic-run frame-3000 screenshot: correct title screen, identical to the previous run.
- Rebuilt KOF97: all 3000 hashes match the original baseline, final state e61c6f3c. Its existing startup watchdog
  reset at frame 247 remains unchanged. The check waited for the full process output before the final comparison.

## Deployment

Target: E:/CPS TEST/CPS-LAUNCHER-ONLINE-PRUEBA/CPS-LAUNCHER.
Backup: E:\CPS TEST\CPS-LAUNCHER-ONLINE-PRUEBA\CPS-LAUNCHER\_respaldo_online_cps\kof2002-desktop-20261029-033919.
Replaced only kof2002.exe and KOF2002-README.md. New executable SHA256:
e1a0ae032bac0b64f23274c71ddcd2ec4a2482cacb6c40508e2ab63472f75047.
The deployed binary is byte-identical to the PE-resource-verified build.

- Deployed direct executable, no --rom, launched from an unrelated working directory: 3000 frames, exit 0,
  final state eb3961f6, hashes identical to the staged automatic run.
- Existing launcher invoked with --play kof2002 -- --headless --frames 3000: exit 0, 3000 frame hashes;
  last entry: 3000 5a871106.
- All 44 snapshotted existing executables, shared DLLs, portable marker and INI settings retained
  their exact SHA256 values. This includes the launcher, KI1/KI2 bridges and the tunnel. No ROMs were modified.
- Startup tests used the collection's existing shared DLLs; no library replacement was required.

## Documentation and scope

Updated game README and CLI startup notes. Created CHANGELOG.md with the 4-20 changelogger workflow: current
Mexico City CST date, English (UK) matching the repository, typed bullets, no historical entries rewritten.
The original shared handoff recorded Codex completion while Claude had no tokens; later coordination notes
preserve Claude's work without assuming current token availability.

This is a desktop-startup fix, not an online migration. No netplay code, public service, chat, replay or identity
feature changed. No commit or push was made during the original delivery; publication was subsequently
authorised separately. The two existing startup write warnings (25fff0 and 00000c) remain;
neither prevented these startup/title checks. Explorer may retain a cached old icon; PE resource verification
establishes the delivered icon independently of the shell cache.
