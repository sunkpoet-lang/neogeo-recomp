# Validation — 30 October 2026

## Provenance and scope

Source base: 4bb5830269c1621c2b3a5e99f17cfd42b826d45c, with uncommitted variant changes. MAME reference pinned
to 8c285468743535fbbde018fef23b3cfb8c9a3ec8, matching the original protection port. Read boot_kof2k2.cpp,
prot_misc.cpp and the kf2k2mp2 definition in neogeo.cpp. Local copies: ignored work/mame-reference.
Compiler: UCRT64 GCC 16.2.0. Three compilation workers, sequential builds; existing dependencies reused offline.
No source changes to CPU cores, Z80 banks, netplay or third-party trees. No active package deployment.

## Synthetic oracle and loader

`tests/mame_decrypt_reference.py --magic2` extracts unmodified upstream function bodies/tables and substitutes
only framework declarations. No ROMs are read. Region CRC order is P/C/S/M/V:

| Seed | P | C | S | M | V |
| --- | --- | --- | --- | --- | --- |
| 12345678 | 3905b7ce | 7b9117a1 | 40ecf061 | cf37ee59 | 460c03f0 |
| deadbeef | 902ef9ae | 0cb550bf | 227783c6 | 3f35e32a | a2eb06f3 |

CTest passed both neogeo_decrypt and neogeo_startup. The former now exercises original/variant golden CRCs,
actual loader word swap/interleaving/S1, CRC rejection, unrelated-set isolation, every malformed region with
no mutation, unused program tail zero fill, linear M1 size and exhaustive fixed/four-window Z80 banking.
The startup test uses invalid synthetic archives to exercise executable discovery and explicit-path precedence.

## Original-game regression

Before modifying shared code, KOF97 and KOF2002 each ran 6,000 frames with identical scripted coin/start inputs.
After rebuilding, complete per-frame hash logs and PCM WAV bytes matched their respective baselines exactly.
KOF97's pre-existing watchdog reset and KOF2002's two unmapped accesses are present in baseline evidence;
matching these runs is not a claim to have fixed all existing board behaviour.

## Real variant

Using the player's split set and adjacent parent/BIOS: 12,000 attract frames plus a separate 9,000-frame
coin/start/button script, exit 0, no watchdog resets. Inspected actual title with MAGIC PLUS II, character selection
and combat captures. Two unmapped accesses are logged; complete hardware accuracy is not claimed. The attract
PCM has peak amplitude 22054 and 18,408,719 nonzero 16-bit samples; this proves output, not listening-quality approval.

Ignored local evidence: work/kf2k2mp2-validation/runs.json (commands, working directories, exits and durations),
*.log, *.hash, *.wav, play/ and attract/ captures; work/kf2k2mp2-regression/before contains baseline commands,
binary hashes and output. Early attract snapshots were not written because the destination directory had not
yet been created; late attract and all scripted-run screenshots are retained. Title master is play/kf2k2mp2_003000.png.
asset-provenance.json records title-master hash/crop/sizes. No recordings or ROM-derived program data enter Git.

## Acceptance boundaries

Human sound/controller review, a full playthrough, Internet netplay and actual maintained-launcher integration
remain untested for this variant. The Workbench GUI/CLI is not implemented; these manual runtime tests are not
the first Workbench trial or permission to create its private repository. Final icon/package verification is
recorded separately after the icon-enabled executable is rebuilt.

## Final executable and isolated package

The icon-enabled executable contains seven RT_ICON images (16/24/32/48/64/128/256) matching every source ICO
image byte-for-byte, verified with the Windows resource APIs. A separate work/kf2k2mp2-ready folder includes
the executable, recursively resolved UCRT DLL dependencies, project licence/notices and a local test CMD pointing
to the player's central library. With PATH restricted to Windows directories, that packaged executable ran
3,000 headless frames successfully. final-package.json records the command, exit code and all packaged file
SHA256 values. It supersedes binary.json's earlier pre-icon binary hash. No ROMs, BIOS or player settings were
copied into this folder. Human window/controller review and maintained-launcher deployment remain pending.

## Player acceptance - 3 November 2026

The player reports Magic Plus II accepted the same way as KOF98: sound correct, play with GekkoNet correct and offline
play to the ending without problems. This is the player's report, not a recorded run.
