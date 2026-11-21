# Design
Use main as baseline, preserving existing branches and untracked archives. Pin cartridge facts to MAME 8c285468743535fbbde018fef23b3cfb8c9a3ec8, src/mame/snk/neogeo.cpp ROM_START(kof95). Validate all file sizes/CRCs. P1 swaps 1 MiB halves after ordinary word swap. Samples have a 2 MiB zero-filled hole at 0x600000; sprites have a 4 MiB zero-filled hole at 0x1400000. Transform only exact kof95 layout; reject malformed input before mutation. Other sets keep their existing paths.

Use existing Neo Geo state/online code with measured validation; reconcile menu work deliberately. Reuse recomp-common menu/viewer/volume and format-2 fareplay reference. No remote state deserialisation without appropriate validation. Record source identities and actual executable hashes. Separate automated local, user gameplay and Internet acceptance. Capture images from headless hooks only; no desktop input. Derive icon from real title, not AI.

## Replay implementation gates
Reuse the pinned CPS fork format-2 codec/shared viewer. Bind identities to normalised source manifests, state version, game and host ABI. Preflight marks, vector sizes, bool encodings and complete payload consumption using the current state schema before native restore; this is structural validation, not a claim that every native chip semantic has been fuzzed. Verify malformed files, incompatible identities and checksum mismatch, including final frame. Internal viewer rewind snapshots are own-process data.

KOF95 local automatic recordings use observed MVS two-human flags (mode at 10fdaf and participant bytes 10fdb6/10fdb7); validate CPU-only and late second-player runs separately. Finalise after 120 non-versus frames or application exit, bounded at 72000 frames. Do not record speculative online inputs; confirmed online capture is a separate remaining gate. Round-start markers follow a reset bout clock and its first decrement (10a836/10a837), tested against native traces; do not infer winners or scores from timer events.
## Confirmed online recording

The next implementation records one bounded online session segment, including its
initial state, rather than pretending that every session is already a verified set.
GekkoNet's pinned raw confirmed-input stream is the authority for inputs. A bounded
ledger stores the last simulation of each frame: inputs and before/after state hashes.
Finalisation accepts only the contiguous prefix whose inputs AND hash chain match.
This prevents speculative tails and stale post-rollback states from entering a replay.
No spectator recording; no authenticated-name or winner claims. Session teardown and
recovery finalise before destroying the SDK session. The cap remains 72,000 frames;
SDK storage is stopped near that boundary while play continues. No SDK patch/download.
Tests must include rollback stress, malformed envelopes, stale-chain truncation,
host/guest loopback and replay hashes. Internet acceptance remains a separate gate.
