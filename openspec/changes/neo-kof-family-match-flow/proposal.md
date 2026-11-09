# Proposal

## Why
Extend the accepted KOF97 flow to the three remaining collection variants:
kof2002, kf2k2mp2 (Magic Plus II) and kof98, at the player's request.

## What Changes
- Preserve all four pre-build EXEs; reuse the existing optional broker/session
and presentation controllers without changing defaults or decryption.
- Independently verify each variant's native completed-team score reader and
register it in its own game sources. Similarity is not a support claim.
- Generalise reproducible acceptance by game, retaining failed runs and source,
recipe and EXE fingerprints. Preserve KOF97 regression and previous evidence.

## Impact
The three game adapters, explicit flow allow-list and test recipes. No vendor
sync, installed beta, ROM/config edits, training/menu-bar merge, service,
commit, push or candidate release.
