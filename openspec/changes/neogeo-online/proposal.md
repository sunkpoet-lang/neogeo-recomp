# Proposal

## Why

The Neo Geo's fighting games are what the community plays online most, and online play here is ours to build: there
is no upstream to wait for. The pieces around it already exist:
- the tunnel `cps-tunel` (Iroh, a long code, direct or through n0's relay), tested with Killer Instinct between two
  homes;
- the launcher's ONLINE card.

KOF '97 is now ready for rollback: its runs repeat frame for frame, its whole state saves and loads, and it runs
about seven times faster than the board needs, which leaves room to replay frames.

## What Changes

- **Rollback with GekkoNet** (HeatXD/GekkoNet, BSD-2-Clause), pinned to a release tag and fetched at build time.
  - The session saves and loads the board's state and runs frames with both players' inputs.
  - It compares each frame's checksum between the players to catch desyncs.
  - Input delay is adjustable.
- **A network adapter of our own** for GekkoNet, using plain UDP sockets: the host learns the guest's address from
  the first packet. That's how the tunnel works, since the host's game sees the tunnel's local socket and not a known
  address. Without it, two programs on one PC, or two on a LAN, still connect directly.
- **Command line**:
  - `--host [PORT]` (default 7845) and `--join HOST:PORT`, the same contract as UltraCombo, so the ONLINE card and
    the tunnel start Neo Geo games the same way they start KI;
  - `--delay N` for the input delay;
  - `--stress`: GekkoNet's stress session on one PC, which rolls back every frame and checks that the state comes
    out the same.
- **What's off online**: save states, slow motion and frame stepping, the menu's reset and cheats, and the
  features that change the game.
- **Before playing**: both sides check they have the same game, set and version. A desync or a lost connection is
  said on screen, and the program ends cleanly.
- **The window shows** "waiting for the other player" until the session starts, and the ping and rollback count in
  the corner.

Out of scope here (later changes):
- spectators and replays (GekkoNet has both);
- the ONLINE card and the tunnel inside the launcher (the command line already fits them);
- more than two players, lobbies, matchmaking;
- the Capcom games (they follow upstream's netplay) and Killer Instinct (UltraCombo's).

## Capabilities

### New Capabilities
- `neogeo-netplay`: a two-player online session for Neo Geo games. Covers hosting or joining, rollback, what is off
  online, the checks between the players, desync detection, and the stress test.

### Modified Capabilities
<!-- none: the port's other behaviour stays as neogeo-port-frontend says when not online -->

## Impact

- New:
  - `port/neogeo_online.{h,cpp}`: the session, the adapter and the loop;
  - GekkoNet through CMake's FetchContent, built with `NO_ASIO_BUILD` (our adapter replaces asio's), linked
    statically, with ws2_32 on Windows.
- Changed:
  - `port/neogeo_main.cpp`: the options;
  - `port/neogeo_window.cpp`: frames come from the session when online, with the waiting screen and stats;
  - `port/neogeo_port.cpp`: a state in memory for the session; what is off online.
- `THIRD_PARTY_NOTICES.md`: GekkoNet (BSD-2-Clause) and zpp_bits, which it carries (MIT).
- The custom soundtrack (`neogeo-soundtrack`) was designed to leave the board untouched, so it can be on for one
  player and off for the other.
