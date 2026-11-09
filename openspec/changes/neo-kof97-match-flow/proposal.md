# Proposal

## Why
ForkAde block 5 needs Neo Geo match flow. Limit this first delivery to KOF97,
keeping its existing default netplay and offline board unchanged.

## What Changes
- Explicit --forkade-online KOF97-only opt-in: native hosting through the
existing ForkAde broker, arcade wait, initial-state admission, Start prompts,
names and Hide names, read-only verified team-match streaks and recovery.
- Reuse credited CPS MatchFlow/Recovery controllers with a Neo Geo adapter;
no vendored common changes, source pin updates or installed beta replacement.
- Preserve default wire, chat sequencing, player backup/settings and offline
state/audio; verify rollback, local peers, recovery and native ownership.

## Impact
KOF97 source/build/tests and this specification. Other Neo Geo games are not
accepted or enabled by this slice. No training/menu branch merge or research
implementation from Claude's separate proposal; no commit/push/deploy.
