# KOF95 native result observations

Read-only probes, 26 November 2026, with the accepted candidate EXE (`work/kof95-ready`,
SHA-256 c51fbc77…) copied beside its probe. Each run played a scripted two-human versus from
a cold start (coin and start for player 1 at 3300/3400, player 2's challenge at 5000/5100), one
side holding forward and pressing C, the other idle. Work RAM 0x10B000-0x10B0FF and
0x10D800-0x10DBFF and the MVS mode bytes were logged every frame for 16000 frames, and only
the changes were kept (`neogeo-recomp/work/claude-kof95-base/probe-p{1,2}/changes.json`,
private; the commands beside them).

| Run | Versus ends | Loser's MVS player mode | Winner's mode |
|---|---|---|---|
| player 1 wins | frame 10202 | 0x10FDB7: 1 → 2 (continue), 3 at 10816, 0 at 10936 | 0x10FDB6 stays 1 |
| player 2 wins | frame 10940 | 0x10FDB6: 1 → 2, 3 at 11554, 0 at 11674 | 0x10FDB7 stays 1 |

BIOS mode 0x10FDAF stays 2 through both. In the watched ranges no byte counted a team win:
unlike KOF97 (0x10DAF5/7), KOF98 (0x10DAF9/B) and KOF2002 (0x10DAB7/9), KOF95 has no team-win
counter there, and its versus has no "wins" display. The reader
(`games/kof95/src/match_score.h`) therefore reports the loser leaving play: while both MVS
player modes are 1 the fight is a two-human fight; when one side leaves 1 and the other stays,
the one still playing has a score of one. MatchFlow awards that once (an increment after an
armed two-human fight) and takes a new baseline without an award when the challenger returns
(both modes 1 again). A player losing to the computer, or both sides leaving at once, awards
nobody.

The BIOS player-mode meaning is from
[BIOS RAM locations](https://wiki.neogeodev.org/index.php?title=BIOS_RAM_locations&oldid=9321);
the transitions above are this game's own observed behaviour. No detector, Lua or emulator
code was used.
