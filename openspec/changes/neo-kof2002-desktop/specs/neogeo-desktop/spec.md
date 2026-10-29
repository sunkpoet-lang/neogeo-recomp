# Spec Delta

## Purpose

Make Neo Geo game executables usable directly from a portable collection, with predictable ROM discovery and a recognisable game-specific Windows icon.

## ADDED Requirements

### Requirement: Portable ROM discovery
Without an explicit ROM argument, a game SHALL discover its named set in supported folders beside its executable independently of the process working directory. Supported executable-relative layouts SHALL include `roms`, `ROM`, `rom` and the executable's own directory. Existing working-directory fallback layouts SHALL remain supported.

#### Scenario: Direct startup in the installed collection
- **WHEN** KOF2002 is started without `--rom`, with its set and BIOS in `ROM` beside its executable
- **THEN** the game loads that set even if the process working directory is elsewhere

#### Scenario: Legacy layout
- **WHEN** the game set is in the existing executable-relative `roms` folder
- **THEN** direct startup continues to load that set

#### Scenario: Existing working-directory layout
- **WHEN** there is no executable-relative candidate and the set exists in `roms` in the working directory or directly in that directory
- **THEN** the game continues to discover that set

### Requirement: Predictable selection and failure
An explicit `--rom` argument SHALL take precedence over automatic discovery. Automatic discovery SHALL prefer executable-relative candidates over working-directory fallbacks and use a documented deterministic order. A selected invalid set SHALL retain the loader's CRC or BIOS error instead of silently switching to a different set.

#### Scenario: Explicit path
- **WHEN** the launcher or user supplies a ROM path and other candidate sets also exist
- **THEN** only the explicit path is selected

#### Scenario: Multiple executable-relative candidates
- **WHEN** both `roms/kof2002.zip` and `ROM/kof2002.zip` exist beside the executable
- **THEN** the existing `roms` candidate takes precedence

#### Scenario: Missing or invalid game data
- **WHEN** no candidate exists, or the selected candidate fails ROM or BIOS validation
- **THEN** startup reports the relevant missing file or validation error without changing the user's files

### Requirement: Game-specific Windows icon
The KOF2002 executable SHALL embed an icon based on its actual title-screen logo, following the established collection convention. The icon SHALL preserve the logo's aspect ratio, centre it on a square matching background, and provide 16, 24, 32, 48, 64, 128 and 256 pixel images.

#### Scenario: Built executable
- **WHEN** the Windows KOF2002 executable is built and its embedded resources are inspected
- **THEN** the game icon contains the title logo and all required sizes

### Requirement: Deployment and regression preservation
Updating the test collection SHALL preserve ROMs, user settings, shared libraries, the tunnel, the launcher and other game executables. Replaced files SHALL have a recoverable backup. ROM discovery and icon changes SHALL not alter game simulation or the existing launcher interface.

#### Scenario: Updated KOF2002
- **WHEN** the rebuilt game is installed in the test collection
- **THEN** direct startup and launcher startup both work, with the title-screen icon embedded

#### Scenario: Regression check
- **WHEN** KOF97 runs with the same set, inputs and initial conditions before and after the change
- **THEN** its frame hashes remain identical
