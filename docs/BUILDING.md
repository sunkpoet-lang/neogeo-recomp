# Build and check the source

Clone the source with Git. Supply your own game ZIP and `neogeo.zip`; neither
belongs in the source tree or a contribution. Supported sets are defined by
each `games/<id>/game.cfg`, including exact required members and checksums.

```sh
git clone https://github.com/sunkpoet-lang/neogeo-recomp.git
cd neogeo-recomp
make GAME=kof98
build/kof98/kof98 --rom /path/to/kof98.zip
```

Use a C++20 toolchain, CMake 3.28+, Ninja, Python 3, SDL3 and zlib. Windows builds
use MSYS2 UCRT64 packages; run the build from that environment so its compiler,
libraries and runtime DLLs agree. macOS requires SDL 3.4.16 or later. GekkoNet is
fetched at the tag and SHA-256 fixed in `CMakeLists.txt`; reviewing an update is
a separate operation. The initial dependency fetch requires Internet access.

For direct CMake builds, first generate the game information (the Makefile
already does this):

```sh
python3 tools/gamecfg.py --header work/kof98/gen/game_info.h kof98
cmake -S . -B build/kof98 -G Ninja -DGAME=kof98 -DNEOGEO_BUILD_TESTS=ON
cmake --build build/kof98 --parallel 2
ctest --test-dir build/kof98 --output-on-failure
```

Replace `kof98` consistently to build `kof95`, `kof97`, `kof2002` or `kf2k2mp2`.
Magic Plus II needs its parent game files as specified in its configuration.
Builds, generated files and player settings stay outside published source.
Windows players also need the matching SDL3/zlib runtime dependencies beside
the executable or supplied by their environment; building is not an installer.

## Check gameplay separately

Unit tests validate synthetic fixtures, codecs and bounded state readers.
Actual boot, graphics, sound and controls require the matching owned sets.
A repeatable headless run is useful for comparing two builds:

```sh
build/kof98/kof98 --headless --rom /path/to/kof98.zip --frames 1200 --settings check.ini --eeprom check.dat --hash-log hashes.txt --audio-dump sound.wav
```

Run comparisons in separate fresh folders with identical settings and inputs.
Keep generated state, audio, replays and RAM dumps private. Matching hashes and
audio do not demonstrate Internet acceptance or complete character coverage.
The KOF98 training README and OpenSpec receipts distinguish measured collision
coverage from pending throws and advanced drills.

Artwork is embedded; players need no Python, Pillow, Lua or font installation.
The optional artwork regeneration command is documented with its provenance in
`assets/recorder-inputs/README.md`. Preserve upstream licences and credits in
`THIRD_PARTY_NOTICES.md`. Native execution currently uses a 68000 interpreter;
full static translation remains pending.
