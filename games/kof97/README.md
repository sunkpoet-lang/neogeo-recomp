# The King of Fighters '97 Recomp (this fork, in progress)

The arcade game *The King of Fighters '97* (SNK, 1997, Neo Geo MVS) on this fork's Neo Geo board, in a window with
its sound. The board is modelled (memory, banks, interrupts, inputs, the LSPC video, the Z80 and its YM2610 through
ymfm) and the game runs in the 68000's interpreter, about seven times faster than the board needs. Nothing is
translated to C++ yet, and there is no menu, HUD or saved settings.

## Playing it

The program is `kof97-neogeo.exe`. Put it in a `NeoGeo` folder with your own files in `NeoGeo/roms`: MAME's
`kof97.zip` and the Neo Geo's system set `neogeo.zip` (with `sp-s2.sp1`). In the collection's launcher it is the game
"The King of Fighters '97" (it starts through the bridge `kof97.exe` beside the launcher), with its desktop shortcut.

| | Player 1 | Player 2 |
|---|---|---|
| Move | arrows | I J K L |
| A, C | A, S | T, Y |
| B, D | Z, X | G, H |
| Start | 1 | 2 |
| Coin | 5 | 6 |

Gamepads (the first is player 1): stick or pad, West A, South B, North C, East D, Start, Back for a coin. A coin
counts however short the press. F11 full screen, F12 a screenshot, Esc quits. 9 is the service button, F2 the test
button. The first time it starts, the board sets itself up and restarts once.

## Building and checking

    make neogeo-run GAME=kof97
    build/kof97/kof97-neogeo --headless --rom /path/to/kof97.zip --frames 3000 --shot-every 300 --shot-dir shots \
        --script "1300:coin1:1,1450:start1:8" --hash-log hashes.txt --audio-dump kof97.wav

Next steps: the translation to C++, a menu with saved settings, then online play (`neogeo-online`, with GekkoNet).
