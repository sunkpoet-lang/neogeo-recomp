## ADDED Requirements

### Requirement: The player's music in place of the board's
When the custom soundtrack is on and the player's folder has a file for a piece of music the game asks for, the port
SHALL play that file in place of the board's music, from the moment the game asks for it, and SHALL keep the board's
sound effects and voices. The port SHALL follow the game: when the game stops, fades or changes the music, the file
SHALL do the same. A piece with no file SHALL be heard from the board as always.

#### Scenario: A stage with a file
- **WHEN** the folder has `esaka.ogg` and a fight starts on the Esaka stage
- **THEN** the file plays instead of the board's tune, and the hits and voices are still heard

#### Scenario: A stage without a file
- **WHEN** the folder has no file for the next stage's tune
- **THEN** the board's tune is heard on that stage

#### Scenario: The round ends
- **WHEN** the game stops or fades the music at the end of a fight
- **THEN** the file stops or fades at the same frame

### Requirement: The board's state is unchanged
Turning the custom soundtrack on or off, choosing a folder, or switching between the files and the board's music
while playing SHALL NOT change the board's state. The hash of each frame SHALL be the same as with no soundtrack, for
the same inputs.

#### Scenario: Two runs, one with a soundtrack
- **WHEN** the same headless run with the same inputs is made with no soundtrack and with a folder that replaces
  every song
- **THEN** both hash logs are identical frame for frame

#### Scenario: What the game reads back
- **WHEN** the game reads the sound side's reply while a file plays
- **THEN** it reads what it would read with no soundtrack

### Requirement: The folder's rules
The folder SHALL follow CPS-2 Recomp's rules for a custom soundtrack:
- one file a song, named after the song's name or its command number in hexadecimal;
- Ogg Vorbis, FLAC, MP3 or WAV;
- `soundtrack.ini` for a song's file, loop, volume and start point;
- a file's own loop tags when the ini gives no loop;
- a song the board plays once is played once.

A folder with no file named for a song SHALL be played as a playlist, in place of all the game's music.

#### Scenario: A loop from the ini
- **WHEN** `soundtrack.ini` gives a song `loop = 0:12.5 - 2:47.25`
- **THEN** after 2:47.25 the file goes on from 0:12.5, with no gap

#### Scenario: A playlist
- **WHEN** the folder has files that name no song
- **THEN** they play one after another wherever the game has music, and stop while the game is held

### Requirement: A game's music list
A game SHALL get the custom soundtrack only when its sources list its music. The list SHALL give each piece's command
number and name, say whether the board plays it once, and give the commands that stop or fade the music. The port
SHALL list the songs (`--list-songs`, and `songs.txt` in a folder it is shown).

#### Scenario: Listing KOF '97's songs
- **WHEN** `kof97 --list-songs` is run
- **THEN** it prints each piece of music's file name, number and description

#### Scenario: A game without a list
- **WHEN** a game's sources list no music
- **THEN** its menu and launcher page offer no custom soundtrack

### Requirement: Save states and speed
What is playing (the song, where in it, its fade) SHALL be counted in the game's frames and SHALL be kept with save
states. Loading a state SHALL bring back the music the game had there, at that point in it. While the game is fast
forwarded or held, nothing SHALL be heard, and the music SHALL go on from where the game is when it plays again.

#### Scenario: Loading a state
- **WHEN** a state saved a minute into a stage is loaded during another stage
- **THEN** the first stage's file plays from a minute in

### Requirement: Tools to find the music
The port SHALL log every command the game sends to the sound side, with its frame, when `NEOGEORECOMP_SOUNDTRACK_LOG`
is set. It SHALL be able to play one command alone in a headless run (`--music-test`), so that whoever writes a
game's list can hear what each command is and whether it ends by itself.

#### Scenario: Logging a fight
- **WHEN** a headless run of a fight is made with the log on
- **THEN** stderr has a line for each command the 68000 sent, with its number and frame
