# The King of Fighters '97 Recomp (this fork, in progress)

The arcade game *The King of Fighters '97* (SNK, 1997, Neo Geo MVS) on this fork's Neo Geo board. **Step 1 only**
(openspec change `neogeo-boot`): the board is modelled (memory, banks, interrupts, inputs, the LSPC video, the Z80
answering with a silent YM2610) and the game boots and plays in the 68000's interpreter, with no window and no sound.
Nothing is translated yet, and there is no launcher entry.

You need your own files: MAME's `kof97.zip` and the Neo Geo system set `neogeo.zip` (with `sp-s2.sp1`) beside it.

    make neogeo-run GAME=kof97
    build/kof97/neogeo-run --rom /path/to/kof97.zip --frames 3600 --shot-every 300 --shot-dir shots \
        --script "600:coin1:10,700:start1:6"

Next steps: the translation to C++, the sound (YM2610), the window and the launcher, then online play
(`neogeo-online`, with GekkoNet).
