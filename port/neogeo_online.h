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
//   <game> --watch HOST:PORT     a spectator: the match as the host's game confirms it, a little behind; --spectate
//                                makes a --join one (ForkAde's launcher's Watch) (openspec change match-spectators)
//   <game> --stress              no network: GekkoNet's stress session, rolling back every frame to check that the
//                                game comes out the same (a game passes it before it is played online)
//
// The packets go over one UDP socket of the port's own (not GekkoNet's asio): the host takes the first player who
// says hello as its peer, from whatever address, which is what a tunnel (cps-tunel) needs, its packets coming from a
// local port of its own. Before GekkoNet starts, the two say hello with the game, a checksum of its ROM set and the
// program's version, and play only if they are the same. (openspec change neogeo-online)
#pragma once

#include "match_flow.h"
#include "recovery.h"
#include "confirmed_replay.h"
#include <array>
#include <cstdio>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "../recomp-common/chat.h"
#include "neogeo_chat.h"

struct GekkoSession;

namespace port {

struct NeoGeoPort;

inline bool VerifiedMatchFlowGame(const std::string& game) {
    return game=="kof97" || game=="kof2002" || game=="kf2k2mp2" || game=="kof98";
}

struct OnlineOptions {
    enum class Mode { Off, Host, Join, Stress, Watch } mode = Mode::Off;
    uint16_t port = 7845; // the host's, or the one --join goes to
    std::string host;     // --join's address
    int delay = 1;
    bool matchFlow=false,arcadeWait=false;
    std::string checkpointLog,flowLog;
    // ForkAde's in-game chat: --nick (this player's name, said in the hello), --chat-log DIR, and for the tests
    // --net-say [FRAME:]TEXT and --chat-loss PCT (that share of the chat's packets dropped as sent)
    std::string name, chatLog;
    std::vector<std::pair<uint64_t, std::string>> says;
    int chatLoss = 0;
    // (match-spectators) the host's: how many may watch (0: none); a spectator's: frames behind; --spectate
    int spectators = 6, spectatorDelay = 15;
    bool spectate = false;
};

class ChatRelay;

class NeoGeoOnline {
public:
    enum class Status { Waiting, Connecting, Playing, Recovering, Ended };

    NeoGeoOnline() = default;
    NeoGeoOnline(const NeoGeoOnline &) = delete;
    NeoGeoOnline &operator=(const NeoGeoOnline &) = delete;
    ~NeoGeoOnline();

    // Opens the socket and the session; false with a message. The port's board must be as it powered on.
    bool Start(NeoGeoPort &port, const OnlineOptions &options, std::string &error);
    // One tick (a frame's time): the local player's controls (the stress session: both players'); the session's
    // frames are run on the port's board. Returns how many frames were run that are to be seen (0 or more).
    int Tick(const uint16_t local[2],bool advanceWaiting=true);
    bool ArcadeWaiting()const{return m_options.matchFlow&&m_options.arcadeWait&&ChatHost()&&!m_heard&&m_status!=Status::Ended;}
    bool Recovering()const{return m_recovery.Deadline()!=0&&m_status!=Status::Ended;}
    uint64_t Epoch()const{return m_recovery.Epoch();}
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
    // (online-hud) The same values as rows: at the top right (PING, DELAY, ROLLBACK), and at the top left how
    // many watch, or WATCHING on a spectator's screen; none while not playing
    void StatRows(std::vector<std::string> &right, std::string &left) const;
    // How far this side is ahead of the other: the window waits a little longer when it is
    float FramesAhead() const;
    void Stop();

    // ForkAde's in-game chat (docs/chat-protocol.md): its C and A packets on the session's socket, never GekkoNet's,
    // and each player's name after a 0 byte in the hello, outside the part both sides compare; the screen's part is
    // Chat (neogeo_chat.h), which each tick runs
    recomp::chat::Channel::Said ChatSay(const std::string &text, std::string &line);
    std::vector<std::string> ChatHeard();
    std::string ChatName(int player) const; // ("" for none given)
    int ChatLocal() const;
    bool ChatHost() const;
    bool ChatReady() const; // (the match is on)
    bool ChatOver() const;
    std::vector<std::string> ChatUndelivered() const;
    // (match-spectators) the lines that came, each with who said it ("" for the other player) and whether a
    // spectator did; whether this side watches; how many watch
    struct Heard {
        std::string name, text;
        bool spectator = false;
    };
    std::vector<Heard> ChatHeardLines();
    bool Spectating() const { return m_options.mode == OnlineOptions::Mode::Watch; }
    int Watching() const;
    // (the players' names as the host's status says them, for a spectator's screen)
    std::string ShownName(int player) const { return Spectating() ? m_statusNames[player] : ChatName(player); }
    NeoGeoChat &Chat() {
        return m_chatScreen;
    }
    const OnlineOptions &Options() const {
        return m_options;
    }
    std::vector<std::pair<uint64_t, std::string>> &Says() {
        return m_options.says;
    }

    // (the adapter's, for GekkoNet's callbacks)
    struct Socket;

private:
    std::string Mismatch(const std::vector<uint8_t> &said, bool recoveryCompatible) const;
    std::vector<uint8_t> HelloOut() const;
    void SendStatus(int phase);
    void ApplyStatus();
    void WatchHellos();
    void AddSpectators();
    void SpectatorLeft(int handle);
    bool CreateSpectateSession();
    void PumpChat();
    void RelayLine(int role, const std::string &name, const std::string &text, const std::string &except);
    void UpdateFlowMessage();
    bool CreateGameSession();
    void SendControl(char kind,uint64_t epoch);
    void PrepareRecovery();
    void RequestRecovery();
    bool ResetRecoveryEpoch();
    void ApplyRecovery(const Recovery::Action& action);
    Recovery m_recovery;
    uint64_t m_localNonce=0,m_peerNonce=0,m_roomNonce=0;
    bool m_prepared=false,m_everPlayed=false;
    void PublishFlow();
    MatchFlow m_flow;
    void Pump();
    void SendHello();
    void Events();
    void Run();
    void ReplayBegin();
    void ReplayEnd();
    std::unique_ptr<Replay> m_replay;
    ConfirmedReplayProof m_replayProof;
    unsigned m_replayBase=0;
    bool m_replayAttempted=false,m_replayCap=false;
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
    std::unique_ptr<recomp::chat::Channel> m_chat; // (ForkAde's in-game chat)
    std::string m_theirName;
    NeoGeoChat m_chatScreen;
    std::vector<uint8_t> m_arcadeState;
    uint64_t m_challengerUntil=0;
    std::string m_lastFlowMessage;
    FILE* m_checkpointLog=nullptr;
    FILE* m_flowLog=nullptr;
    // (match-spectators) the host's spectators: GekkoNet's name for each (its address is the socket's), its nickname,
    // handle (-1 until the match runs for it) and chat channels; the host's relay to the other player, or this side's
    // from the host; what the host's last status said
    struct Spectator {
        std::string name, nick;
        int handle = -1;
        std::shared_ptr<recomp::chat::Channel> own;
        std::shared_ptr<ChatRelay> relay;
    };
    std::vector<Spectator> m_spectators;
    std::shared_ptr<ChatRelay> m_relay;
    int m_hostPhase = -1, m_seenWatching = 0;
    std::array<std::string, 2> m_statusNames;
};

} // namespace port
