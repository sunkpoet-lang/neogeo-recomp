// SPDX-License-Identifier: GPL-3.0-only
// Part of neogeo-recomp.
//
// Online play for a Neo Geo game: two players, each with the whole board, kept the same by rollback (GekkoNet,
// BSD-2-Clause). Each side runs its own player's controls at once and predicts the other's; when the other's arrive
// and differ, the board goes back to the last frame both agree on (NeoGeo::Serialize) and runs the frames again,
// unseen and unheard. Each side compares a checksum of the board with the other's, and a difference ends the session.
//
//   <game> --host [PORT]         player 1, waiting on that UDP port (7845)
//   <game> --join HOST:PORT      player 2
//   <game> --delay N             the local controls held back N frames (0-8; 1): fewer frames to take back
//   <game> --stress              no network: GekkoNet's stress session, rolling back every frame to check that the
//                                game comes out the same (a game passes it before it is played online)
//
// The packets go over one UDP socket of the port's own (not GekkoNet's asio): the host takes the first player who
// says hello as its peer, from whatever address, which is what a tunnel (cps-tunel) needs, its packets coming from a
// local port of its own. Before GekkoNet starts, the two say hello with the game, a checksum of its ROM set and the
// program's version, and play only if they are the same. (openspec change neogeo-online)
#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct GekkoSession;

namespace port {

struct NeoGeoPort;

struct OnlineOptions {
    enum class Mode { Off, Host, Join, Stress } mode = Mode::Off;
    uint16_t port = 7845; // the host's, or the one --join goes to
    std::string host;     // --join's address
    int delay = 1;
};

class NeoGeoOnline {
public:
    enum class Status { Waiting, Connecting, Playing, Ended };

    NeoGeoOnline() = default;
    NeoGeoOnline(const NeoGeoOnline &) = delete;
    NeoGeoOnline &operator=(const NeoGeoOnline &) = delete;
    ~NeoGeoOnline();

    // Opens the socket and the session; false with a message. The port's board must be as it powered on.
    bool Start(NeoGeoPort &port, const OnlineOptions &options, std::string &error);
    // One tick (a frame's time): the local player's controls (the stress session: both players'); the session's
    // frames are run on the port's board. Returns how many frames were run that are to be seen (0 or more).
    int Tick(const uint16_t local[2]);
    Status GetStatus() const {
        return m_status;
    }
    bool Failed() const {
        return m_failed;
    }
    // What the window says (waiting, connecting, a desync, the other player gone; "" while playing)
    const std::string &Message() const {
        return m_message;
    }
    // A line for a corner of the screen: the ping, the delay and the frames taken back
    std::string Stats() const;
    // How far this side is ahead of the other: the window waits a little longer when it is
    float FramesAhead() const;
    void Stop();

    // (the adapter's, for GekkoNet's callbacks)
    struct Socket;

private:
    void Pump();
    void SendHello();
    void Events();
    void Run();
    void End(const std::string &message, bool failed);

    NeoGeoPort *m_port = nullptr;
    OnlineOptions m_options;
    GekkoSession *m_session = nullptr;
    Socket *m_socket = nullptr;
    Status m_status = Status::Waiting;
    bool m_failed = false;
    std::string m_message;
    int m_local = 0, m_remote = 1; // the players' handles in the session
    std::vector<uint8_t> m_hello;  // what this side says hello with
    bool m_heard = false;          // the other side's hello has come, and it is the same game
    uint64_t m_ticks = 0;
    uint32_t m_stateSize = 0;
    std::vector<uint8_t> m_state;
    int m_shown = 0;
    int m_coinFrames = 0;
};

} // namespace port
