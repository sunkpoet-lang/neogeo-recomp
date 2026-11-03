# Validation — 30 October 2026

## Reference and implementation

Base source 4bb5830269c1621c2b3a5e99f17cfd42b826d45c plus uncommitted Magic Plus II/KOF98 changes.
MAME pinned at 8c285468743535fbbde018fef23b3cfb8c9a3ec8: prot_kof98.cpp, kof98.cpp and neogeo.cpp.
The supplied original P1/P2 CRCs are 8893df89/980aba4c. The program-only transform retains MAME's 6 MiB region;
S/C/M/V remain untouched. Runtime header reads use explicit high-byte-first words, not host-endian casts.
Protection state is serialised only for KOF98; other cartridges retain their original state byte layout.

## Synthetic and loader checks

tests/kof98_reference.py extracts the unmodified MAME decrypt_68k function body with minimal device declarations.
For artificial 6 MiB programs, seeds 12345678/deadbeef produce CRCs 18a84b60/178dc47c. Port output matches.
CTest passes independent original/Magic Plus II/KOF98 goldens, untouched KOF98 regions, malformed sizes without
mutation, actual loader interleaving/word swap/dispatch/CRC rejection, protection states/byte lanes/unknown writes/
reset and state restoration. All four Z80 bank windows include 128/256/512 KiB M1 cases. Actual startup discovery
is also exercised with invalid synthetic archives. Logs: ignored work/kof98-validation/ctest.log.

## Real game and original-game regression

KOF98 ran 10,000 headless frames with scripted coin/start/button inputs. Inspected own title and combat captures;
this is not a full playthrough or human controller/sound acceptance. The initial watchdog reset at frame 247 and
an unmapped 0x205554 write are recorded, not suppressed. PCM output is nonzero; no listening-quality claim.
Title frame 3000 and crop/hash are recorded in work/kof98-validation/asset-provenance.json.

Before shared changes, KOF97/KOF2002/Magic Plus II ran 6,000 frames with identical scripts. The final packaged
executables launched through ForkAde's persisted shared-library route reproduce each complete frame-hash log
and PCM WAV byte-for-byte. Isolated ASCII test profiles retain unrelated preferences. A separate direct EXE test
without --rom and from an unrelated working directory finds each new set via a private ROM directory junction
to the owned library; no game files are copied into the package.

## Local online and icon checks

Both packaged games passed 1,800-frame GekkoNet stress runs and 3,600-frame localhost host/join sessions through
the actual launcher, using separate player profiles/logs and ports. KOF98's final state on both peers is acdba1d5;
Magic Plus II's is a75deb16. Both logs report playing, exit 0, all requested frames and no desync. Stress replay
can repeat KOF98's initial reset in the diagnostic counter; that counter is not gameplay acceptance evidence.
These are local tests, not Internet, tunnel-service, latency or cross-platform acceptance.

Both own game ICOs and ForkAde branding match their embedded PE images; seven game-icon sizes are retained.
KI1/KI2/FBNeo bridges pass argument/exit-code/fallback tests with spaces/Unicode using probe executables; these
are routing checks, not actual KI/FBNeo gameplay or new preparation functionality.

## Package and provenance

The maintained ForkAde recipe exports pinned CPS/Neo Geo Git objects and applies an explicit 13-file allowlist
overlay, recording source hashes and the uncommitted status. Base/hash/path validation rejects invalid overlays
before copying. Full assembly/source/dependency/EXE hashes are recorded in its build manifest.
The separate 14-game folder and ZIP pass the project's required-file, prohibited game/player-data and builder-path
audit. All 92 application files of the active E: baseline still match the recorded snapshot. No commit/push or
baseline replacement occurred. The local test candidate may hold a private ROM-directory preference; the ZIP
retains a clean portable profile without that preference or ROMs.

## Harness corrections and remaining limits

An initial harness used a non-ASCII CPS2RECOMP_HOME test-profile path; the existing launcher throws a path-conversion
exception there. ASCII profile roots are used for these runs; this does not claim that non-ASCII HOME overrides
were fixed. An early peer attempt read launcher stdout rather than its per-game log and shared one profile;
the final successful attempts use independent profiles and their actual child logs. These failures remain distinct
from the final results. Own-frame captures through the screenshot mode initially show the Games page because
--page selection occurs before asynchronous readiness; CLI launch/online tests verify both entries independently.

Ignored evidence: work/kof98-validation/before, runtime.json/log, community/results.json, magic2-stress-result.json,
per-peer logs/hashes, direct-start.json, bridge-check/results.json and baseline-integrity.json. Human play quality,
full game completion and real Internet sessions remain player acceptance tasks.

## Player acceptance - 3 November 2026

The player reports about three hours of play with nearly every team: sound correct, play with GekkoNet correct and
the game running fully; offline play reaches the ending without problems. Every team to the ending (about six
hours) was not played and is not claimed. This is the player's report, not a recorded run.
