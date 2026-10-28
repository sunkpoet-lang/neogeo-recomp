## 1. Bring the player over

- [x] 1.1 Take `port/soundtrack.{h,cpp}`, `port/decoders.{h,cpp}`, `machine/soundtrack.cpp` and `third_party/stb_vorbis.c`, `dr_flac.h`, `dr_mp3.h`, `dr_wav.h` from cps2recomp with their history (`git filter-repo --paths-from-file`, merged as the toolkit was); their headers stay cps2recomp's, as the toolkit's other files do
- [x] 1.2 Add them to `CMakeLists.txt` (the decoders with `-w`) and to `THIRD_PARTY_NOTICES.md`; build `kof97` and check that the hash at frame 3000 is still `7ef55b19`

## 2. The commands

- [x] 2.1 `NEOGEORECOMP_SOUNDTRACK_LOG`: every command the 68000 sends to 0x320000, with its frame and the Z80's reply, to stderr
- [ ] 2.2 `Soundtrack::Command(byte, frame)` beside `Request`; `runtime/neogeo_game.h` gets the game's music list, stop command and fades; `IsMusic` from that list; SNK's reserved commands never music

## 3. The listening copy

- [ ] 3.1 `machine/neogeo_listener.{h,cpp}`: a Z80 and a `NeoGeoSound` with the board's sound program, samples and banks; given each command at the Z80 clock it was sent; run to the board's clock each frame; created from the board's sound side when turned on
- [ ] 3.2 Check: with a listener and nothing replaced, the listener's sound matches the board's sample for sample over a 3000-frame run (`--audio-dump` of each)
- [ ] 3.3 Save states: the listener and the soundtrack's state in a section after the board's, outside `StateHash`; state version up one; older states load without it

## 4. KOF '97's music

- [ ] 4.1 Log runs: attract mode with sound, a one-player ladder to Orochi (cheats and save states to get there), each team's stage, continue and game over; note each command with what is on screen
- [ ] 4.2 `--music-test N` (one command alone, headless) to hear each one and find the ones that end by themselves
- [ ] 4.3 `games/kof97/src/music.cpp`: every piece of music with its number, name and "once", plus the stop and fade commands; add it to the game's sources

## 5. The port

- [ ] 5.1 `custom_soundtrack` in `RegisterNeoGeoPortFeatures` for a game with a music list (Off, On, Playlist shuffled); `--soundtrack`, `--list-songs`, `songs.txt` in the game's own folder
- [ ] 5.2 Mixing in `neogeo_window.cpp` and the headless `--audio-dump`: the listener plus the file while a soundtrack is on, the board otherwise; nothing made while held or fast-forwarded
- [ ] 5.3 F12 switches between the files and the board's music, with its note on screen; `--keys` letter `m` for the tests
- [ ] 5.4 The launcher: the Sound card's soundtrack row for KOF '97 (`collection.sh` passes the game's list)

## 6. Checks

- [ ] 6.1 Same inputs, with no soundtrack and with a folder that replaces every song: hash logs identical frame for frame
- [ ] 6.2 Headless `--audio-dump` with test files (tones made by a script, not music): each file starts and stops at the game's frames; loop, once and playlist behave as the folder's rules say
- [ ] 6.3 Save a state mid-stage, load it on another stage: the first stage's file resumes where it was
- [ ] 6.4 The user plays with their own folder from the launcher: stage tunes, F12, a save state, the menu's switch
- [ ] 6.5 README and `games/kof97/README.md`: how to make a folder, with KOF '97's song list
