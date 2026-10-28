## 1. GekkoNet in the build

- [x] 1.1 FetchContent GekkoNet at tag `v20261024222910-9da2363`, `GekkoLib` only, `NO_ASIO_BUILD`, static; link it (and ws2_32 on Windows) to the Neo Geo games; build `kof97` and check the hash at frame 3000 is still `7ef55b19`
- [x] 1.2 `THIRD_PARTY_NOTICES.md`: GekkoNet (BSD-2-Clause) and zpp_bits (MIT), fetched at build time

## 2. The session

- [x] 2.1 `port/neogeo_online.{h,cpp}`: the session (game or stress), Save/Load/Advance events on the board, the checksum, `state_size` measured
- [x] 2.2 The UDP adapter: one non-blocking socket, the host learns its peer from the first packet, the "NGO" prefix, the hello with game, set fingerprint and version
- [x] 2.3 Options: `--host [PORT]`, `--join HOST:PORT`, `--delay N`, `--stress`; online sessions start from a fresh backup RAM and don't write it back

## 3. Headless and the stress test

- [x] 3.1 The headless loop runs the session: `--stress` with `--script` inputs; `--host`/`--join` without a window for tests
- [x] 3.2 Stress: KOF '97 over 6000 frames (attract mode, a credit, a fight) reports no difference
- [x] 3.3 Two headless programs on one PC (`--host`, `--join 127.0.0.1`), with scripted inputs on each side: they connect, play 3000 frames with no desync, and end with the same state hash (a frame's hash may be from a prediction, so the logs are compared by GekkoNet's confirmed checksums, not line by line)

## 4. The window

- [x] 4.1 `RunNeoGeoWindow` online: the waiting screen, frames from the session, rolled-back frames neither shown nor heard, pacing by frames ahead
- [ ] 4.2 What is off online: keys, the menu's rows, the cheats; the menu doesn't stop the game
- [x] 4.3 The ping, delay and rollbacks in a corner; the desync and "the other player left" messages
- [ ] 4.4 Two KOF windows on one PC, one with the keyboard and one with the stick: a full fight with no desync

## 5. Through the tunnel (with the user)

- [ ] 5.1 `cps-tunel host --game-port 7845` + `kof97 --host 7845` and `cps-tunel join CODE` + `kof97 --join 127.0.0.1:PORT` on one PC
- [ ] 5.2 A match with a friend through the tunnel; note the ping and rollbacks
- [ ] 5.3 `games/kof97/README.md` and the README: how to play online today (the command line, the tunnel), what is off
