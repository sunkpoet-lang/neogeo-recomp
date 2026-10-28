# Design

## Context

CPS-2 Recomp's custom soundtrack (`machine/soundtrack.{h,cpp}`, `port/soundtrack.{h,cpp}`) works like this:
- It watches the request the 68000 leaves in the memory it shares with the sound side.
- When the player has a file for the music asked for, it rewrites that request into "the music stops" inside the
  board.
- It then mixes the file over the board's sound.

That is safe on a CPS-2 because the game never reads back what the sound side does.

On the Neo Geo, the 68000 sends a byte to 0x320000 and the Z80 gets an NMI. The Z80 answers with a byte the 68000
reads at the same address (`NeoGeo::m_reply`). And `neogeo-online` will compare the board's full hash (Z80 and YM2610
included) between two players, who may have different soundtracks. So the CPS-2 trick would change the board's state
here, and maybe the game.

`neogeo-recomp` already carries `machine/soundtrack.h` and the `songs` fields of `runtime/game.h` (they came with the
toolkit), but it does not build `soundtrack.cpp` or the port's player.

## Goals / Non-Goals

**Goals:**
- The player's files replace the board's music. Sound effects and voices stay the board's.
- The board's state is identical with or without a soundtrack, so hashes, replays and online play don't notice it.
- The same folder rules, file formats and launcher row as CPS-2 Recomp, so players' folders and habits carry over.
- KOF '97's music fully listed.

**Non-Goals:**
- Replacing effects or voices; mixing several files at once.
- A generic finder for the songs in a Neo Geo sound program. Unlike the CPS-2, SNK's sound programs differ from game
  to game, so each game lists its own.
- Rendering the soundtrack again after a rollback. Like most rollback games, the sound already played is left as it
  was.

## Decisions

### 1. A listening copy of the sound board, outside the board

`NeoGeoListener` is a second Z80 and a second `NeoGeoSound`, with the same sound program, samples and banks as the
board's. It exists only while a soundtrack is on. The board passes it each command at the Z80 clock it was sent
(`NeoGeo::SoundCommand`), and the listener runs to the same clock each frame. A request for music that has a file
reaches the listener as the game's "music stops" command; every other command reaches it unchanged. What is heard is
the listener's output plus the file. The board's own YM2610 output is dropped while the listener exists.

- *Why not rewrite the command in the board, as CPS-2 does?* The Z80's reply and the Z80 and YM2610 state would
  differ, so hashes would no longer match between players, and the 68000 might act on the reply.
- *Why not mute channels on the board's chip?* SNK's drivers share the ADPCM-A channels between drums, voices and
  effects. Muting by channel would cut voices or leave drums in. Blocking key-ons would also change the chip's state.
- *Cost:* the Z80 interpreter and ymfm at 55,555 Hz cost a few percent of a core. That's small next to the 68000's
  interpreter, and paid only with a soundtrack on.

The listener is kept with save states in its own section after the board's, and it is not in `StateHash`. A state
from a run without a soundtrack loads with the listener re-created from the board's sound side, with its Z80, chip
and banks copied. So the board's tune is heard where the game is, and the file starts at the next music request.

### 2. Music requests as single command bytes

`Soundtrack::Request` reads CPS-2's sixteen bytes. A second entry, `Soundtrack::Command(uint8_t, frame)`, takes one
Neo Geo command and returns the byte the listener should get. A game's list gives its music codes, so `IsMusic`
comes from `songs`, not from `FindMusic`. It also gives the stop and fade commands (`TheGame().musicStop`, plus a
list of fades with their lengths in frames), which follow what the game does to a file. `State`, `Last`, `Frame` and
`Serialize` are used as they are.

The commands SNK's drivers reserve (0x01 slot switch, 0x03 reset, and the BIOS's eye-catch) are never music, and they
always reach the listener.

### 3. The port's player taken with its history

`port/soundtrack.{h,cpp}`, `port/decoders.{h,cpp}` and the four decoders in `third_party` come from cps2recomp with
`git filter-repo` history, as the rest of the toolkit did. The changes for the Neo Geo:
- `Make` runs at the chip's own rate or the device's;
- `SongList` reads the game's list;
- the files' headers say they belong to neogeo-recomp.

The feature, options, `songs.txt`, F12 and its on-screen note follow `port/main.cpp`'s code, moved into
`neogeo_port.cpp` and `neogeo_window.cpp`.

### 4. Finding KOF '97's music

1. Run `NEOGEORECOMP_SOUNDTRACK_LOG=1` headless with inputs that cover: attract mode, a one-player ladder (Orochi
   included), and a stage of every team. Use save states and cheats to get there quickly.
2. Use `--music-test N` to play one command alone and find whether it ends by itself.
3. Write it all down in `games/kof97/src/music.cpp`:
   - names in the style `team_stage`, `orochi`, `ending_<team>`, `continue`, `game_over`;
   - which pieces play once;
   - the stop and fade commands.

## Risks / Trade-offs

- **The listener drifts from the board** (a bank switch or a timer it misses) → it is built from the same classes
  and fed the same commands at the same clocks. A test compares the listener's output with no music replaced against
  the board's, and they must be identical.
- **KOF '97 sends music as more than one command** (a "prepare" byte and then the song) → the log shows it. If so,
  `Command` keeps a short state of what came before. That state is part of `Last` and is kept with save states.
- **A song the board plays with effects mixed into its own data** (a jingle with a voice) → listed as "played once".
  Whether to replace it is the player's choice; the files are optional.
- **More CPU on weak machines** → only while a soundtrack is on, and the file decoding is the same as CPS-2's.

## Migration Plan

Nothing to migrate. Save states gain an optional section; the state version goes up by one, and older states load
without a listener.

## Open Questions

- Which command or commands KOF '97 uses to stop and fade its music: found with the log in task 4.
- Whether to choose by team (Esaka, Aomori, ...) or by stage name for files: settled when the list is written. The
  hexadecimal numbers always work.
