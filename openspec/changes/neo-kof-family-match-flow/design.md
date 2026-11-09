# Design

## Scope
Continue codex/kof97-match-flow from e3f334b with the KOF97 work preserved.
Games: kof2002 original, kf2k2mp2 split bootleg, kof98 original. Configuration
and variant-specific cartridge decryption remain unchanged. Claude's menu-bar
and training proposal stays separate.

## Decisions
- Capture all four current EXEs before building. Reuse the proven MatchFlow,
Recovery and native GameRoom adapters; extend explicit opt-in only to the
three independently tested IDs. Default netplay stays unchanged.
- Keep native result maps in each games/<id>/src. Fightcade detector offsets
and the player's training documentation are investigation leads only. Read
native bus addresses and BIOS human flags during genuine team matches;
verify both winners and do not count character KOs, attract or CPU-only play.
Do not copy Lua/detector implementation or assume KOF97 addresses carry over.
- Use the existing production observer for long natural-match traces and6000
rollback stress separately; the KOF97 combined34000 stress timed out and is
not the appropriate runtime-cost baseline. Use fresh profiles and outputs.
- Sequential builds with three jobs, max two independent test tasks. Native
owner tests use exclusive port7845 serially. Other local pairs/outages get
exclusive ports and per-game output paths. No shared test directory deletion.

## Acceptance per new game
6000 offline state/WAV equivalence against its own saved EXE, own icon resource
identity, verified full human team matches through the live observer,6000
stress,20 local peers with latest board/flow checkpoints (tail32 excluded),
3600 recovered frames, stale/chat and expiry checks, native owner/backup and
legacy compatibility. Record any unverified game as pending. Regress KOF97
when shared code changes. No Internet/Iroh-game or installed-beta claim.
