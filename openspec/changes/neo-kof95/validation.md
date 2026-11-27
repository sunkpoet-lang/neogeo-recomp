# KOF95 validation — 19 November 2026

## Source and scope

Branch `codex/neo-kof95`, based on local main `e3b535b`; changes remain uncommitted. Prior branch and untracked archive preserved. Guest 68000/Z80 still interpreted. Original MVS cartridge identified against MAME `8c285468743535fbbde018fef23b3cfb8c9a3ec8` (all 14 members, lengths/CRC/SHA1 inspected locally). User ROMs were only read. No downloads, installed-beta changes or publication.

## Demonstrated

- Strict OpenSpec validation PASS.
- ROM-free sparse layout checks PASS: P1 continuation, every byte of V/C padding and payload mapping; truncated P/V/C/S/M input rejected without mutation, assembled layout rejected on a second pass.
- CTest: layout, existing decrypt/loader/Z80-bank tests and actual-executable synthetic startup checks PASS (3/3).
- Two cold offline runs of 3600 frames: every state hash and full WAV equal, final hash `239f5c38`.
- Own state saved at frame 1800 and loaded in a new process: all next 1800 hashes and audio suffix equal to the uninterrupted run.
- Rollback stress: 6000 frames, final hash `92a85fbc`, zero watchdog resets and unmapped addresses; exit 0. Diagnostic speed 37 simulated frames/s under forced rollback (160.30 s); this is not a normal gameplay performance claim.
- Scripted coin/start/selection/buttons: 9000 frames, final hash `94070f47`; real combat capture at frame 6000. This is automated gameplay, not user acceptance or a complete playthrough.
- Final EXE with icon and copied DLLs runs 3600 frames under Windows-only PATH: state/audio unchanged. Seven ICO payloads match embedded PE resources.
- Two actual headless processes, host and guest on loopback: 601 matching checkpoints (-1 through 599), clean exits. No product match-flow, relay or spectators were exercised.

## Images

`games/kof95/images/title.png`: full unmodified capture at frame 3300.
`games/kof95/images/gameplay.png`: full unmodified scripted combat at frame 6000.
`images/provenance.json` identifies the capture EXE; icon is derived from the actual title. Images have been visually inspected. Icon-only relink retains exact state/audio, but has a different binary hash.

## Exact remaining work

KOF97 regression PASS: rebuilt broker-enabled KOF97 against the retained pre-existing executable, 1200 state hashes and full WAV equal. Its original source identity is not inferred. The build exposed an existing main-branch broker callback using overlayHidden before its declaration; moving the declaration above the callback fixed compilation. KOF95 was then rebuilt and its final Windows-only-PATH 3600-frame state/audio check repeated successfully. Menu branch `game-window-menu` has now been reconciled manually against main using its common ancestor. The newer common menubar, volume, chat, broker and HUD paths are retained; there is no merge/cherry-pick commit or unresolved conflict.

Complete baseline remains OPEN: UI/cross-process save-state coverage beyond this attract checkpoint, format-2 fareplay/source identity, full viewer and its script, automatic versus recording, verified result/unlock, match-flow names, direct/relayed mid-game spectators/chat, launcher/catalogue/profile/discovery and human/Internet acceptance. A generic local peer success does not close those gates. No compatibility catalogue promotion or public release yet.

## Private receipts (not committed)

The private `work/kof95-validation-20261119` directory contains commands, archive identification, full hash/WAV data, own-state payload, pre-icon/final binary hashes, icon inspection and peer logs. No ROMs, states, WAVs, private paths or user settings belong in a public evidence bundle. The local runnable candidate is separate from any installed beta.

One icon relink attempt was blocked because the stress EXE was running; the original test completed, then relink succeeded after all test processes exited. The final packaged executable was tested again. No failed attempt is counted as a pass.

## Unified menu milestone

The user confirms two physical controllers, responsive characters and good music in the initial KOF95 candidate. This is local human acceptance, not Internet evidence.

Menu integration reuses the four existing `game-window-menu` commits without blindly overwriting newer shared components. Offline and hidden-window program hooks demonstrate F10/Alt/right-click, frame freeze/resume, quick save/load, slot thumbnail, volume95% after minus and half-speed after left bracket. A failed stronger assertion exposed that LoadState restored board frames but not the window counter; the counter now follows the restored board. The rerun shows frame29, advance to38, then F9 back to29.

Final KOF95 3600-frame hashes/WAV and KOF97 1200-frame hashes/WAV match prior references; CTest3/3, strict OpenSpec and whitespace checks pass. One rerun initially reused backup RAM from its previous output directory, so it was not a cold comparison; fresh-directory runs passed. Failed logs remain private. Save/load UI and hidden program events do not constitute physical mouse/gamepad acceptance of the new bar. No desktop input or firewall changes were performed.

Sources reconciled: port/menu.*, neogeo_{main,port,window}.*, present.*, CMakeLists.txt; existing recomp-common implementation retained. `images/menu.png` is an actual offscreen GPU render from the executable, not a mock. Private receipts: work/kof95-menu-validation-final, work/kof95-menu-validation/slots-v2 and work/kof95-validation-20261119/kof97-regression/menu-summary.json. Candidate refreshed at work/kof95-ready, prior accepted executable backed up at work/kof95-ready-before-menu. No commit/push or installed-beta replacement. Replay/format2/viewer and remaining online/launcher gates stay open.

## Local replay/viewer milestone — 19 November 2026

Final candidate SHA-256: `6adee7edaee6c7cba36aa11d2323af7bb5598e1fd7d49f3378696dd3451531fb`.
Gameplay/state identity: `5b0c7089a70567b159955652763e9d93cabc715bc85a64af7168a2ef5cfd0d7c`.
The manifest includes board/CPU/video/state/game hooks, port state and ymfm sources; it normalises line endings and separates the platform/pointer ABI. LF/CRLF equivalence and different-ABI rejection were checked. This is a conservative source compatibility gate, not proof of cross-platform compatibility.

Local format-2 manual recording reproduces all 1,200 state hashes and the entire WAV. The 600-tick viewer script matches linear truth for every visited state across pause, step, rewind/forward tiers, slow motion, seeks, endpoints and A–B looping. Seven executable-level negative cases reject identity, game, backend, excessive duration, truncation, malformed initial-state marker and final-checksum corruption. An earlier same-runtime revision also passed a 7,200-frame recording/audio comparison; that receipt is kept under its own executable hash rather than attributed to the final binary.

A 20,000-frame native two-human sequence starts automatic capture at absolute frame 5,100, closes naturally after 5,221 recorded frames and reproduces every hash. Its three round markers are 1,212 / 2,523 / 3,775, and the file is 26,998 bytes. The CPU-only control creates no automatic file. The KOF95-specific MVS participant flags were inspected during actual native runs; they are not assumed to apply to every Neo Geo game.

Hidden application hooks verify keyboard pause/steps, synthetic controller steps, mouse HUD navigation, and audio-rate changes/silence during pause. Native Windows file/folder dialogs and physical viewer-controller acceptance remain manual checks. No desktop input was sent. State preflight verifies structure and exact consumption; it does not claim a complete semantic fuzz audit of every chip state.

CTest6/6 passes. The packaged EXE with Windows-only PATH reproduces the prior cold 3,600-frame KOF95 hashes/WAV, retains all seven embedded icon sizes and preserves user settings. A freshly rebuilt KOF97 reproduces its previous 1,200 hashes and WAV. Strict OpenSpec and whitespace checks pass. Public `viewer.png` and `replay-menu.png` are real offscreen renders, visually inspected with hashes in image provenance.

Private receipts: `work/kof95-replay-validation-packaged`, `work/kof95-auto-replay-validation-packaged`, `work/kof95-replay-gui-validation-packaged`, `work/kof95-identity-validation`, `work/kof95-replay-package` and `work/kof97-replay-regression`. Earlier failed draft/test runs are retained separately. The runnable candidate is refreshed at `work/kof95-ready`; its previous menu EXE is backed up at `work/kof95-ready-before-replays`. No test replay/state is bundled. Recordings themselves contain an initial game state and remain private.

Still OPEN: confirmed online recording (offline capture stops before netplay), authenticated names and verified result/score, applicable unlock, direct/relayed spectators/chat acceptance, launcher integration/discovery, video export and human/Internet acceptance. Generic local names do not represent authenticated accounts. No commit, push, installed-beta replacement or firewall changes. Earlier remaining-work paragraphs are historical milestones, superseded only for the local replay/viewer gates documented here.

## Confirmed online recording — 20 November 2026

This milestone supersedes the online-recording limitation above. Online sessions
now save one confirmed segment on teardown/recovery, capped at 72,000 frames and
with a 60-frame minimum. They can include menus/character selection; they are not
yet result-delimited versus sets. Spectators do not record. Longer-session
rotation is still open. Nicknames are session metadata, not authenticated names.

`ConfirmedReplayProof` verifies both each confirmed input and its contiguous
before/after state-hash chain against the last simulation. A stale predicted tail
is truncated, with the final checksum taken from the last accepted proof. No
live speculative state is used as the replay's final checksum. SDK raw format-2
envelopes are validated before extraction, including version, player/input width,
length, compression and empty state/checksum/application fields.

The initial SDK peek can already hold local-delay padding inputs (observed during
stress). The draft assumption that its full length was an empty envelope was
wrong and was rejected without writing a file. The corrected parser checks the
actual 50-byte field envelope. Actual SDK source hashes are retained privately;
the local source copy is not assumed byte-identical to a remote tag. No SDK
files were edited or downloaded.

The 6,000-frame forced-rollback test on EXE
`66dd70b977edd1cfa119af5bcc45911a462caff31c001f91a200758bd3dd9aad`
produced a 2,199-byte online replay and reproduced every final SDK checkpoint.
It ran at 33 simulated frames/s under diagnostic forced rollback plus capture;
that is not a normal netplay or latency benchmark. Draft loopback produced host
1,199 and guest 1,200 confirmed frames, with 1,199 shared states identical.

Final packaged EXE:
`c51fbc77ddb497be1fc30aefaa7dbc074cee2c0ca9e0fc86c997e4ed804ed0db`.
Its Windows-only-PATH tests reproduce all 1,200 confirmed stress states.
The final two-process loopback run records host 1,198 and guest 1,200, with all 1,198
shared replay states identical. The short host tail is deliberately omitted.
The packaged path tests also verify
file write/read in a folder containing spaces and a non-ASCII character. The
prior cold 3,600-frame hashes/WAV remain exact, automatic capture can be disabled,
and the ROM-folder guard creates no replay in the ROM directory. An initial test
assertion expected a stderr refusal, whereas this path reports through the UI
note; that assertion was corrected and a fresh guard run passed. Existing valid
cold/stress evidence was rechecked instead of rerunning it against persisted RAM.
The current KOF97 rebuild also preserves its previous 1,200 hashes and full WAV.

Seven CTest cases cover confirmed-prefix chains/stale predictions, SDK envelope
rejection, codec, local phase, structural state preflight, cartridge layout and
startup/decryption. Strict OpenSpec and whitespace validation pass. Private
receipts: `work/kof95-online-replay-accept`, `work/kof95-online-replay-peer`,
`work/kof95-online-replay-peer-final`, `work/kof95-online-delivery` and
`work/kof97-online-replay-regression`. Test states/replays/WAV remain private.

The candidate at `work/kof95-ready` was refreshed while preserving `user/`; the
previous local-replay EXE is at `work/kof95-ready-before-online-replays`. This is
not an installed beta/launcher promotion. Real Internet, spectators/relay/chat,
recovery acceptance in the full product and verified results remain separate.
No firewall modification, desktop automation, commit, push or deployment.

Training references requested by the user were investigated separately in
`neogeo-training`: proposal/design/requirements/tasks only, with no training
runtime, copied Lua or installed dependencies. Its documented plan validates
KOF98 first as the reference, then each other set independently; it does not
reuse one game's RAM map across the KOF family.

## ForkAde match flow, spectators and catalogue — 26 November 2026 (Claude)

Worktree `D:/CPS2/neogeo-kof95-base`, branch `claude/kof95-base` from `codex/neo-kof95` 3c032cb (Codex's
uncommitted training work in `D:/CPS2/neogeo-recomp` untouched). Program: `build/kof95/kof95.exe`, Release with
`FORKADE_BROKER_DIR=D:/CPS2/ForkAde`, SHA-256 2b8fadbe39dde16c…; baseline: the accepted candidate
`work/kof95-ready/kof95.exe` c51fbc77ddb497be….

Changes: `games/kof95/src/match_score.h` (the result reader, `native-results.md`), registered in `kof95.cpp`;
KOF95 admitted to the ForkAde match flow (`port/neogeo_online.h`) and the native room build (`CMakeLists.txt`);
`tools/check_neogeo_flow.py --game kof95` with a KOF95 trace (two human versus matches won by player 1, the second
after player 2 challenges again) and the menu test linked with `recomp-common/menubar.cpp` (it no longer linked
since the menu bar); `tests/kof95_match_score_test.cpp`; `tools/check_kof95_spectators.py`;
`tools/check_kof95_tunnel.py`.

| Check | Result |
|---|---|
| `check_neogeo_flow.py --game kof95` (`build/kof95-flow-acceptance/report.json`) | PASS, 42 cases: embedded icons equal to the baseline; menu, score and recovery units; offline 6000 scripted frames, state and sound byte for byte as the baseline; stress 6000 with the human fight armed; two real team matches, streak 2 (34000 frames); 20 host/guest pairs, 363-365 matching checkpoints each; outages both ways and one way (3689 recovered checkpoints each), forever (60 s timeout), the native owner (809); the legacy wire both ways against the baseline |
| Spectators on loopback, 1800 frames (`build/kof95-spectators`) | players 1800/1800 agreed; spectator at 4 s 1663/1663 equal from frame 122, at 12 s 1404/1404 from frame 379; players' frames the same with and without spectators; chat with accents once each way, spectators get the players' lines, the host names both |
| Through `cps-tunel`, direct, 3600 frames, spectator after 20 s (`build/kof95-tunnel/kof95-direct`) | players 3590/3590, spectator 2937/2937 equal from frame 650, chat ("suerte, ñandú") both ways, spectator admitted |
| Through `cps-tunel`, relay only (two runs) | players 3590/3590 and chat both ways; **the spectator gets no frame**: admitted at frame ~631, then "connection to the match was lost". KOF97's product build fails the same way (`build/kof97-tunnel-relay`): a shared Neo Geo defect, not KOF95's |
| Names strip, a hidden online pair (`build/claude-probe/strip`) | "Daniel 0 VS 0 Iñaki" at the top; the ñ shows as the font's fallback glyph (the shared Neo Geo font) |
| Volume | `--volume 50` kept in settings.ini; the level is outside the game state (generic, already checked on KOF97/98) |
| ForkAde catalogue | `kof95` entered as a candidate (no profile); `tests/test_catalogue.py` has the same 6 failures before and after (pre-existing, Git Bash paths and older counts) |

Cause of the relay defect: neogeo-recomp builds GekkoNet v20261024222910-9da2363 unpatched, which resends the whole
spectator state (some 280 KB, ~300 datagrams of 900 bytes) every 200 ms until it is acknowledged; through n0's relay
the burst is lost and the host drops the spectator. sf3comp and MvC2 pace the chunks (patch_gekkonet_sf3/mvc2.cmake).

Unlock: the arcade boss code (hold Start on team select: Up+B, Right+C, Left+A, Down+D, and the order Up+B, Right+C,
Down+D, Left+A once and twice, on team select and on Team Edit) was tried with scripted inputs; none showed Saisyu or
Omega Rugal, and the A press selected a team. Sources disagree on the code; open (2.7b).

Not covered: Internet between two PCs, a real window (menu bar, strip, viewer with a pad), the launcher and the
package. Private: `neogeo-recomp/work/claude-kof95-base` (probes, captures). No commit, push or installed change.

### Spectators through the relay fixed for every Neo Geo game — 26 November 2026

`tools/patch_gekkonet_neogeo.cmake`, run by `CMakeLists.txt` after GekkoNet is fetched (idempotent; it says so when
the source is already patched): the host sends the spectator's state 10 chunks every 16 ms (about 560 KB/s), going
round the chunks until the spectator acknowledges, in place of every chunk at once every 200 ms; the spectators'
inputs are kept a minute (3600 frames, 600 before). Only the host's sending changes: the packets and the spectator's
side are GekkoNet's own. Built against a patched copy (`build/gekkonet-src`), not the shared one under
`neogeo-recomp/build/kof97/_deps`.

| Check (`cps-tunel`, relay only, 3600 frames, spectator after 22 s) | Before | After |
|---|---|---|
| KOF95 (`build/kof95-paced-tunnel`, program `build/kof95-paced/kof95.exe`) | spectator 0 frames (two runs) | players 3590/3590; spectator 2848/2848 equal from frame 657; chat both ways |
| KOF97 (`build/kof97-paced-tunnel`, `build/kof97/kof97.exe` from this branch) | spectator 0 frames | players 3590/3590; spectator 2854/2854 equal from frame 659; chat both ways |
| KOF95 spectators on loopback (`build/kof95-paced-spectators`) | passed | passed: 1643/1643 and 1384/1384, players the same with and without spectators |

KOF98, KOF2002 and Magic Plus II get the same patch when built from this branch; they were not run through the relay
here. An unpatched spectator receives the paced chunks the same way (the receiving code is unchanged), not run.

Unlock, second round: the code as Daniel sent it from a 2005 forum post (Team Edit? on YES, hold Start, Up+B,
Right+C, Left+A, Down+D, release Start), with YES chosen or confirmed first, and with three timings (the direction
2 frames before the button, 8-frame presses, the code twice): the directions and A still drove the menu (cursor,
YES/NO, team chosen) and the bosses' place stayed empty. Open (2.7b).

### The bosses' code on the original MVS set — 26 November 2026

- The inputs reach the game as they should: during the code the BIOS's player 1 current byte (0x10FD96) reads
  0x21, 0x48, 0x14 and 0x82 in turn, with Start held (0x10FDAC bit 0) throughout.
- With the BIOS's MVS flag (0x10FD82) at 0 during the select screens (a probe build, `build/kof95-probe`, never
  shipped) the game shows the home version's credits and prompts, and the bosses still do not appear.
- The program ROM (084-p1, word-swapped, halves swapped) holds no table of the code in any of the layouts searched
  (the four combined bytes, active high or low, strides 1-8; direction and button apart; word tables) and no run of
  compare-immediate instructions with those values.

So the code Daniel found (Team Edit, hold Start, Up+B, Right+C, Left+A, Down+D) appears to belong to the home
versions (AES NGH-084 and Neo Geo CD, as the post says), not to this original MVS set. Not proven by
disassembly of the select screen. Open: leave the bosses locked as in the arcade, or find the select grid's flag.

### Saisyu Kusanagi and Omega Rugal open — 26 November 2026

Found with a probe build (`build/kof95-probe`, never shipped; `work/claude-kof95-base` holds its scripts): from a
state saved before team select, groups of 512 work-RAM bytes that stay 0 through the select screens were written 1
before every frame, and each capture's empty boss slot compared with an unwritten run. One of 108 groups showed both
bosses; splitting it twice more left one byte, 0x10e79a. A normal 16000-frame versus never writes it (0 after the
power-on RAM test). With it at 1 both appear in Team Edit's bottom row (Saisyu left, Omega Rugal right), Omega Rugal
was picked into a team (Iori, Omega Rugal, King) and the fight ran. In the home mode (the probe's MVS flag at 0 from
boot) the Start code did not open them either, so it belongs to the home cartridge's program.

`games/kof95/src/kof95.cpp` writes it before every frame while the game has the board (not during the BIOS's RAM test),
through the port's RunFrame, so offline, online and replay playback all see it. No option: both sides of a match
always agree. Image `games/kof95/images/bosses.png` (provenance.json, exe db950afb…).

This changes the state from the game's first frame (frame 239 in the checker's offline script) and, as the select
screen gains the bosses' cells, the scripted run's later sound. So the final acceptance used the final program as its
own baseline (`build/kof95-flow-acceptance-2`, `--baseline build/claude-probe/kof95-unlock-baseline.exe`): PASS, 41
cases, streak 2, outages 3689/3688 recovered checkpoints, native owner 811. The offline equality with Codex's
candidate stands for the build before the unlock (`build/kof95-flow-acceptance`). Through the relay with the final
program: players 3590/3590, spectator 2864/2864 from frame 653 (`build/kof95-final-tunnel`). Replays recorded with
earlier builds carry another source identity (kof95.cpp changed), so they are refused as another build; not run.
