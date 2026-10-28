## ADDED Requirements

### Requirement: Hosting and joining
The program SHALL start a two-player online session with `--host [PORT]` (player 1, listening on that UDP port, 7845
by default) or `--join HOST:PORT` (player 2). The host SHALL accept the first peer that answers from any address,
including a tunnel's local socket. Until both sides are connected, the game SHALL not run, and the window SHALL say
that it is waiting for the other player.

#### Scenario: Two programs on one PC
- **WHEN** one program runs with `--host 7845` and another with `--join 127.0.0.1:7845`
- **THEN** both start the game at the same moment, the first's controls are player 1 and the second's player 2

#### Scenario: Through the tunnel
- **WHEN** the host's game is reached through `cps-tunel host --game-port 7845` and the guest joins the port that
  `cps-tunel join` prints
- **THEN** the session connects as it does directly

### Requirement: Rollback
Each side SHALL run its player's inputs at once, predict the other's, and when the real inputs differ, load the
state of the last confirmed frame and run the frames again. The frames run again SHALL not be shown or heard. A
local input delay SHALL be adjustable (`--delay N`, 0 to 8 frames; 1 by default).

#### Scenario: A late input
- **WHEN** the other player's input arrives three frames late and differs from the prediction
- **THEN** the game goes back three frames, runs them again with the real input, and both screens agree from then on

### Requirement: Both sides stay the same
Each side SHALL compare the checksum of the whole board's state with the other's for confirmed frames. A difference
SHALL be reported on screen and in the log with the frame, and SHALL end the session. Before starting, the two sides
SHALL check that they run the same game, the same ROM set and the same version, and SHALL refuse to play otherwise,
saying why.

#### Scenario: Different sets
- **WHEN** one player has a different `kof97.zip`
- **THEN** neither game starts, and both say the sets differ

#### Scenario: A desync
- **WHEN** the checksums of a confirmed frame differ
- **THEN** both windows say "desync at frame N" and the session ends

### Requirement: What is off online
While online, save states, slow motion, frame stepping, pausing, the menu's reset and test switch, and cheats SHALL
not be offered. Features that do not change the board (display, shaders, the custom soundtrack) SHALL stay
available. Opening the menu SHALL not stop the game.

#### Scenario: The menu online
- **WHEN** a player opens the menu during an online fight
- **THEN** the game goes on underneath, and the menu has no save state, speed or cheat rows

### Requirement: The connection
The window SHALL show the ping and the frames rolled back. A peer silent for a few seconds SHALL be dropped, staying
under the tunnel's idle limit (8 s). When the other player leaves or is dropped, the window SHALL say so, and the
program SHALL end cleanly, keeping its backup RAM.

#### Scenario: The other player closes the game
- **WHEN** the guest closes the window mid-fight
- **THEN** the host's window says the other player left, and closing it ends the program normally

### Requirement: The stress test
`--stress` SHALL run GekkoNet's stress session: the game with both players' inputs (from the controls, or from
`--script` without a window), rolled back and run again every frame, comparing each run's checksums. It SHALL report
any difference with its frame and exit with an error. A game SHALL pass it before it is offered online.

#### Scenario: KOF '97 passes
- **WHEN** `kof97 --headless --stress --frames 6000 --script ...` runs through attract mode, a credit and a fight
- **THEN** it reports no difference and exits with 0
