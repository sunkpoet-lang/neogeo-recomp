# The King of Fighters '97 Recomp (this fork, in progress)

The arcade game *The King of Fighters '97* (SNK, 1997, Neo Geo MVS) on this fork's Neo Geo board, as a game of the
collection like the Capcom ones: its page in the launcher, the controls of the Controllers page, the in-game menu, save
states, the Display page's shaders and scaling. The board is modelled (memory, banks, interrupts, inputs, the LSPC
video, the Z80 and its YM2610 through ymfm) and the game runs in the 68000's interpreter, about seven times faster
than the board needs. Nothing is translated to C++ yet.

## Playing it

`kof97.exe` goes beside the collection's launcher, like the other games. Its set is MAME's `kof97.zip`, with the Neo
Geo's system set `neogeo.zip` (with `sp-s2.sp1`) beside it: in the launcher's ROM folder, or chosen on its page.

Its controls are the collection's (the Controllers page): the Neo Geo's A and C on a six-button stick's first two
buttons of the top row (light and medium punch), B and D on the first two of the bottom row (light and medium kick), as
KOF is played on one. A coin counts however short the press.

While playing: Esc or F1 (or the left stick's click, or Start and Select together) opens the in-game menu (save states,
coin, speed, sound, the cheats, the Display page, test menu, reset, quit). P stops and starts, O goes on a frame, - and
= change the speed, F5 / F9 save and load a state, F6 / F7 the slot, F2 the operator's test switch, 9 service, F11
full screen. The board's backup RAM is kept in the game's saves folder, so the board sets itself up only the first time.

## Playing online

Two players, rollback by GekkoNet (`port/neogeo_online.h`). Both need the same `kof97.zip`, `neogeo.zip` and
version; the game checks before it starts.

    kof97 --host 7845 --rom ROM/kof97.zip                 player 1, waiting on UDP port 7845
    kof97 --join 127.0.0.1:7845 --rom ROM/kof97.zip       player 2 (another PC: its address)

Over the internet with no ports to open, through a UDP tunnel such as `cps-tunel` (Iroh, a code to share; not
published yet): the host runs `cps-tunel host --game-port 7845` and then `kof97 --host 7845`, and sends the code it prints;
the other runs `cps-tunel join CODE --listen 7846` and then `kof97 --join 127.0.0.1:7846`. `--delay N` holds the local
controls back N frames (1 by default). Online there are no save states, pausing, slowing, reset or cheats, and each
player's backup RAM is left as it was. `--stress` runs GekkoNet's stress test on one PC.

## Its own sources

`src/kof97.cpp` (runtime/neogeo_game.h): for now the Cheats page's infinite energy for each player, with the players'
health at 0x108239 and 0x108439 as fbneo-training-mode documents it. It writes only once the game has the board: the
system ROM tests the work RAM at power-on, and a write then fails that test.

## Building and checking

    make GAME=kof97
    build/kof97/kof97 --headless --rom /path/to/kof97.zip --frames 3000 --shot-every 300 --shot-dir shots \
        --script "1300:coin1:1,1450:start1:8" --hash-log hashes.txt --audio-dump kof97.wav

`--menu FRAME:KEYS[:shot.png]` works the menu (u d l r, a a button, b back), `--keys TICK:KEY,...` presses keys
(p, o, -, =; f and NAME=N sets a feature), `--features` and `--settings` as the launcher gives them.

Next steps: the custom soundtrack (AST), the training mode (over fight/), spectators and replays for online play
(GekkoNet has both), and the translation to C++.
