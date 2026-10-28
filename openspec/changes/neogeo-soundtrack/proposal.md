# Proposal

## Why

CPS-2 Recomp's Super Turbo lets players hear their own music (an arranged soundtrack, say) in place of the board's,
and players ask for the same in KOF. KOF '97 is the first game here, so this change builds that feature for the Neo
Geo, and every later game will use it too. Online play (`neogeo-online`) will compare the whole board's state between
two players. That means the soundtrack must change only what is heard, never the board: one player may use it while
the other doesn't.

## What Changes

- **A listening copy of the sound board.** When a soundtrack is on, a second Z80 and YM2610, outside the board, get
  the same commands from the 68000 at the same moments. The one difference: a request for a piece of music the player
  has a file for reaches the copy as "the music stops". Only the copy is heard. The board's own Z80 and YM2610 run as
  always, and the 68000 reads the replies of the board's own Z80. So the board's state, its hash and the game are
  the same with or without a soundtrack. (CPS-2 Recomp edits the request in the board itself; that works there
  because a CPS-2 game never reads back what its sound side does. A Neo Geo game reads the Z80's reply at 0x320000.)
- **The player's files**, with the same folder rules as CPS-2 Recomp (`port/soundtrack.h`):
  - one file a song, named after the song (`title.ogg`) or its command number in hexadecimal (`21.ogg`);
  - Ogg Vorbis, FLAC, MP3 or WAV;
  - `soundtrack.ini` sets a song's file, loop, volume and start point;
  - a folder with no named file is a playlist.
  
  Those files are taken from cps2recomp with their history: the player, the decoders (stb_vorbis, dr_flac, dr_mp3,
  dr_wav) and the "what is playing" counter kept in the game's frames.
- **The feature in the port.** It is the `custom_soundtrack` feature on the menu's Sound page and the launcher's
  Sound card. `--soundtrack DIR` chooses the folder (the launcher's folder row passes it). `--list-songs` lists the
  songs and writes `songs.txt`. A key switches between the board's music and the files while playing.
- **The game says what its music is.** A Neo Geo sound program has no shared table of songs like the CPS-2's. So a
  game's sources list its music commands (number, name, played once or looped), along with the commands that stop or
  fade the music. Finding them: `NEOGEORECOMP_SOUNDTRACK_LOG` logs every command the 68000 sends, with its frame,
  and `--music-test` plays one command alone.
- **KOF '97's songs**: found by playing (attract mode, every team's stage, the bosses, the endings, the continue and
  the game over screens) and listed in `games/kof97/src/music.cpp`.

Out of scope:
- Replacing sound effects or voices.
- Other games' song lists: each new game adds its own.
- Online play itself (`neogeo-online`). This change only makes sure the soundtrack cannot cause a desync.

## Capabilities

### New Capabilities
- `neogeo-soundtrack`: the player's own music heard in place of the board's, with the board's state unchanged. Covers
  the listening copy of the sound board, the folder of files and its rules, the songs a game declares, and the tools
  for finding them.

### Modified Capabilities
- `neogeo-port-frontend`: the port gains the `custom_soundtrack` feature (menu, launcher, options, switch key) for
  games whose sources list their music.

## Impact

- New: `machine/neogeo_listener.{h,cpp}` (the listening copy), `port/soundtrack.{h,cpp}` and `port/decoders.{h,cpp}`
  from cps2recomp, `third_party/stb_vorbis.c`, `dr_flac.h`, `dr_mp3.h`, `dr_wav.h`, and `games/kof97/src/music.cpp`.
- Changed:
  - `machine/neogeo.{h,cpp}`: the command, and the Z80 time it was sent, are also given to the listener; the board's
    behaviour is unchanged.
  - `machine/soundtrack.{h,cpp}`: a request can be a single Neo Geo command byte.
  - `runtime/neogeo_game.h`: the music list.
  - `port/neogeo_port.cpp`, `neogeo_window.cpp`, `neogeo_main.cpp`: mixing, feature, options and key.
  - `CMakeLists.txt`, `THIRD_PARTY_NOTICES.md` (stb_vorbis and dr_libs: public domain or MIT-0), `games/kof97/README.md`.
- Cost: a second Z80 and YM2610 run only while a soundtrack is on. That is cheap next to the 68000's interpreter.
- Save states also keep the listener and the playing song, in a section that is outside the board's hash.
