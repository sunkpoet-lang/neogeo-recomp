## ADDED Requirements

### Requirement: The custom soundtrack in the port
For a game whose sources list its music, the program SHALL offer the `custom_soundtrack` feature on the menu's Sound
page and on the launcher's Sound card, with a row there to choose the folder. It SHALL take the folder from
`--soundtrack DIR` and SHALL switch between the files and the board's music with a key while playing (F12, as in
CPS-2 Recomp), saying which is on for a moment on screen. In a headless
run, it SHALL write the files' sound into `--audio-dump` as it is heard.

#### Scenario: Choosing a folder in the launcher
- **WHEN** the player turns the custom soundtrack on in the launcher, chooses a folder and presses Play
- **THEN** the game starts with that folder's music

#### Scenario: Switching while playing
- **WHEN** the player presses the switch key during a stage that has a file
- **THEN** the board's tune is heard from its start, and pressing the key again brings the file back from where the
  game is in the song
