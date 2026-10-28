# Design

## Context

The Neo Geo port runs a frame with `NeoGeoPort::RunFrame(players)`, which sets the board's inputs, runs the game's
sources (`beforeFrame`, `frame`: cheats) and `NeoGeo::RunFrame`. `NeoGeo::Serialize` keeps the whole board, and
`StateHash` is the CRC-32 of it. KOF '97's hash logs repeat run after run.

GekkoNet (C API, `gekkonet.h`) drives a game through events:
- **Save**: write the state and a checksum into its buffer.
- **Load**: restore a state.
- **Advance**: run one frame with these inputs, marked `rolling_back` when it is a frame run again.

It sends packets through a `GekkoNetAdapter`: three function pointers (send, receive, free), with addresses as
opaque bytes. Its built-in adapter (asio) needs every remote address up front, but behind `cps-tunel` the host's
game sees packets from the tunnel's ephemeral local port.

## Goals / Non-Goals

**Goals:**
- A two-player online session for KOF '97 that works directly and through the tunnel, with rollback and desync
  detection.
- A stress test that proves the game safe for rollback before anyone plays it online.
- Nothing changes for offline play: same hashes, same menu.

**Non-Goals:**
- Spectators, replays, the launcher's ONLINE card (later changes; the command line is made for them).
- More than two players; lobbies.

## Decisions

### 1. GekkoNet fetched and pinned, without asio

`FetchContent_Declare(GekkoNet GIT_TAG v20261024222910-9da2363)`, built with `NO_ASIO_BUILD=ON`, static, its
`GekkoLib` subdirectory only (the examples need vcpkg). Only the Neo Geo games link it. A newer tag is a deliberate
update.

- *Why not asio?* Our adapter is needed anyway for the tunnel. Dropping asio keeps the build small and leaves one
  socket path to test.

### 2. Our UDP adapter learns the peer's address

`port/neogeo_online.cpp` opens one non-blocking UDP socket (Winsock on Windows, BSD sockets elsewhere).
- The guest knows the host's address (`--join`).
- The host binds its port and takes the first valid packet's sender as its peer, ignoring every other sender.
- GekkoNet is given one fixed address string ("peer") for the remote actor, and the adapter maps it to the real
  endpoint.

Every packet carries a 4-byte prefix ("NGO" + kind), so stray traffic is dropped:
- **Hello** packets come before GekkoNet starts. They carry the game id, the set's fingerprint (a CRC-32 of the
  game's and the system's ROM data as loaded) and the program version. Both sides compare them, and play only when
  they match; otherwise each says what differs and stops.
- **Gekko** packets carry GekkoNet's own data.

### 3. The state GekkoNet keeps

A Save event serializes the board into the event's buffer: `NeoGeo::Serialize` with an `Archive` into a reused
vector, then copied. The checksum is that data's CRC-32, the same as `StateHash`. `state_size` is the serialized
size measured at start-up, plus margin. A Load event deserializes it. The prediction window is 8 frames, so
GekkoNet keeps about 9 states.

What's in the state is only the board. The game's sources' per-frame hooks run inside the frame from the board's
state, so online they are limited to ones that change nothing (the cheats are off, decision 5).

### 4. The loop

In the window, `RunNeoGeoWindow` keeps its pacing (the board's 59.19 Hz, against the clock). When online, each
tick:
1. `gekko_network_poll`;
2. the local player's controls go in through `gekko_add_local_input` (a 16-bit word, `NeoGeo::players` bits);
3. `gekko_update_session`, whose events are handled in order.

Advance runs `RunFrame` with the two inputs. While `rolling_back` it runs with sound and picture kept off, the same
as fast-forwarding: the chip runs, its samples are dropped, and only the last frame's picture is shown. When
`gekko_frames_ahead` is over 1, the tick waits slightly longer (about 1.6 %, as GekkoNet's example does), so the side
that's ahead slows down to meet the other.

Headless (`--stress`, and tests of `--host`/`--join` without a window), the same session code runs from
`neogeo_main.cpp`'s loop with `--script` inputs.

### 5. What is off online

`NeoGeoPort::online` is set while a session runs. With it set:
- `Key` ignores pause, step and speed;
- `MenuAct` refuses Save, Load, Reset and Test;
- the menu hides those rows and the Cheats page.

The game's features marked as changing the game (the cheats today) are forced off for the session and come back
after. The menu, when open, does not stop the frames.

### 6. Desync and disconnection

- `GekkoDesyncDetected` shows "Desync at frame N" (also on stderr with both checksums) and ends the session.
- `GekkoPlayerDisconnected` shows "The other player left".
- The disconnect timeout is 5 s, under the tunnel's 8 s idle limit. Ending the session keeps the window open on its
  last picture until the player closes it.

## Risks / Trade-offs

- **Hidden state outside `Serialize`** (something the board keeps that isn't saved) would desync the two players →
  the stress test finds it on one PC before any real match, with check distance 10 over thousands of frames.
- **Rollback cost**: up to 8 frames run again in one tick, about 8/7 of a frame's time at the interpreter's speed,
  which could stutter on slow PCs → the default delay of 1 keeps rollbacks short. The rollback stats in the corner
  show it; the C++ translation will make it go away.
- **The first frames differ when the backup RAM differs** (one player's board set up, the other's new) → the backup
  RAM is part of the state, so a difference shows up as a desync at frame 1. Online sessions start from a fresh
  backup RAM (the board's power-on setup, the same on both sides) and don't write it back. Tested in task 4.
- **UDP on Windows**: the firewall may ask the first time the host listens; through the tunnel only localhost is
  used.

## Open Questions

- Whether to show a short count-in ("3, 2, 1") after connecting: not needed for a first test.
