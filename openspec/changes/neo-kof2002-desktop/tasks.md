# Tasks

## 1. Direct ROM discovery

- [x] 1.1 Extend executable-relative discovery in `port/neogeo_port.cpp` to the documented layouts without changing explicit `--rom`; verify selection order, legacy fallbacks and missing/invalid-path errors with targeted checks.
- [x] 1.2 Update startup documentation in `port/neogeo_main.cpp` and `games/kof2002/README.md`; verify the documented no-argument collection layout by running the executable from an unrelated working directory.

## 2. KOF2002 title-screen icon

- [x] 2.1 Create `games/kof2002/icon.ico` from the real title-screen logo using the established crop/centre treatment; visually inspect the logo and verify all seven required ICO sizes.
- [x] 2.2 Reconfigure and build KOF2002 with the existing Windows resource hook; verify the built executable contains the intended icon group and image resources, independently of Explorer's cache.

## 3. Integration and regression

- [x] 3.1 Run KOF2002 using automatic discovery and using an explicit ROM argument with identical initial conditions; verify matching 3000-frame hashes and successful title-screen rendering.
- [x] 3.2 Rebuild KOF97 and verify its 3000-frame hash log matches the saved baseline; run existing synthetic decryption/banking CTest checks and verify they still pass.

## 4. Portable deployment

- [x] 4.1 Back up the installed KOF2002 executable and affected documentation, replace only those files and verify the deployed executable matches the tested build by SHA256; preserve launcher, DLLs, ROMs, tunnel and user settings.
- [x] 4.2 Verify deployed direct startup without `--rom` from an unrelated working directory and the existing launcher Play path; record results and icon resource verification in this change's validation report.
- [x] 4.3 Update the shared handoff with Codex ownership and completion/evidence, and maintain the changelog using 4-20's changelogger workflow if updating that file; verify the resulting documentation describes delivered behaviour and no commit or push was made.

## Workflow follow-up

- Review the implementation evidence before archiving this change.
- Keep the upstream launcher migration separate and preserve Killer Instinct 1 and 2, their UltraCombo bridges, settings and existing netplay.
