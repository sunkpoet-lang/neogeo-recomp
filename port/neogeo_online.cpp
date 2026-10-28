// SPDX-License-Identifier: GPL-3.0-only
// Part of neogeo-recomp.
#include "neogeo_online.h"

#include "game_info.h"

#include "../runtime/game.h"
#include "../runtime/state.h"
#include "neogeo_port.h"

#include <gekkonet.h>
#include <zlib.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#ifndef NEOGEO_RECOMP_VERSION
#define NEOGEO_RECOMP_VERSION "0"
#endif

namespace port {

using cps2recomp::Archive;
using cps2recomp::NeoGeo;

namespace {
    // Every packet starts with these three bytes and a kind: H, hello (before GekkoNet's own); G, GekkoNet's
    constexpr char kTag[3] = {'N', 'G', 'O'};
    constexpr int kPredictionWindow = 8;
    constexpr unsigned kTimeoutMs = 5000; // (under the tunnel's 8 s without traffic)
    // The address GekkoNet is given for the other player: the socket knows where that is
    constexpr char kPeer[] = "peer";

    uint32_t Crc(uint32_t crc, const std::vector<uint8_t> &data) {
        // (zlib takes at most 4 GB a call; a set is far less)
        return static_cast<uint32_t>(crc32(crc, data.data(), static_cast<uInt>(data.size())));
    }
} // namespace

// --- the socket, and GekkoNet's adapter on it ---

#ifdef _WIN32
using SocketHandle = SOCKET;
constexpr SocketHandle kNoSocket = INVALID_SOCKET;
#else
using SocketHandle = int;
constexpr SocketHandle kNoSocket = -1;
#endif

struct NeoGeoOnline::Socket {
    SocketHandle handle = kNoSocket;
    sockaddr_in peer{};
    bool havePeer = false;
    bool host = false;
    std::deque<std::vector<uint8_t>> forGekko;   // GekkoNet's packets from the peer, until it asks
    std::vector<GekkoNetResult *> handedOver;    // (what receive_data last returned: the array is ours)
    std::vector<std::pair<sockaddr_in, std::vector<uint8_t>>> hellos;

    ~Socket() {
        if (handle != kNoSocket) {
#ifdef _WIN32
            closesocket(handle);
            WSACleanup();
#else
            close(handle);
#endif
        }
    }

    bool Open(uint16_t port, std::string &error) {
#ifdef _WIN32
        WSADATA wsa;
        if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
            error = "the network could not be started";
            return false;
        }
#endif
        handle = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (handle == kNoSocket) {
            error = "no socket";
            return false;
        }
#ifdef _WIN32
        u_long nonBlocking = 1;
        ioctlsocket(handle, FIONBIO, &nonBlocking);
#else
        fcntl(handle, F_SETFL, fcntl(handle, F_GETFL, 0) | O_NONBLOCK);
#endif
        sockaddr_in at{};
        at.sin_family = AF_INET;
        at.sin_addr.s_addr = htonl(INADDR_ANY);
        at.sin_port = htons(port);
        if (bind(handle, reinterpret_cast<sockaddr *>(&at), sizeof(at)) != 0) {
            error = "UDP port " + std::to_string(port) + " is in use";
            return false;
        }
        return true;
    }

    bool Resolve(const std::string &host, uint16_t port, std::string &error) {
        addrinfo hints{}, *found = nullptr;
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_DGRAM;
        if (getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &found) != 0 || !found) {
            error = "the address " + host + " was not found";
            return false;
        }
        memcpy(&peer, found->ai_addr, sizeof(peer));
        freeaddrinfo(found);
        havePeer = true;
        return true;
    }

    void Send(char kind, const void *data, size_t size, const sockaddr_in &to) {
        std::vector<uint8_t> packet(4 + size);
        memcpy(packet.data(), kTag, 3);
        packet[3] = static_cast<uint8_t>(kind);
        if (size) {
            memcpy(packet.data() + 4, data, size);
        }
        sendto(handle, reinterpret_cast<const char *>(packet.data()), static_cast<int>(packet.size()), 0,
               reinterpret_cast<const sockaddr *>(&to), sizeof(to));
    }

    static bool Same(const sockaddr_in &a, const sockaddr_in &b) {
        return a.sin_addr.s_addr == b.sin_addr.s_addr && a.sin_port == b.sin_port;
    }

    // Everything that has come: hellos kept for the session to look at, GekkoNet's from the peer queued for it
    void Pump() {
        uint8_t buffer[65536];
        while (true) {
            sockaddr_in from{};
#ifdef _WIN32
            int length = sizeof(from);
#else
            socklen_t length = sizeof(from);
#endif
            const int got = static_cast<int>(recvfrom(handle, reinterpret_cast<char *>(buffer), sizeof(buffer), 0,
                                                      reinterpret_cast<sockaddr *>(&from), &length));
            if (got < 0) {
#ifdef _WIN32
                // (Windows says so when a packet sent before found no one listening: not an error here)
                if (WSAGetLastError() == WSAECONNRESET) {
                    continue;
                }
#endif
                break;
            }
            if (got < 4 || memcmp(buffer, kTag, 3) != 0) {
                continue;
            }
            if (buffer[3] == 'H') {
                hellos.emplace_back(from, std::vector<uint8_t>(buffer + 4, buffer + got));
            } else if (buffer[3] == 'G' && havePeer && Same(from, peer)) {
                forGekko.emplace_back(buffer + 4, buffer + got);
            }
        }
    }
};

namespace {
    NeoGeoOnline::Socket *g_socket = nullptr; // (GekkoNet's callbacks carry no pointer of ours)

    void SendData(GekkoNetAddress *, const char *data, int length) {
        if (g_socket && g_socket->havePeer) {
            g_socket->Send('G', data, static_cast<size_t>(length), g_socket->peer);
        }
    }

    GekkoNetResult **ReceiveData(int *length) {
        g_socket->handedOver.clear();
        while (!g_socket->forGekko.empty()) {
            std::vector<uint8_t> &packet = g_socket->forGekko.front();
            auto *result = static_cast<GekkoNetResult *>(malloc(sizeof(GekkoNetResult)));
            result->addr.size = sizeof(kPeer) - 1;
            result->addr.data = malloc(result->addr.size);
            memcpy(result->addr.data, kPeer, result->addr.size);
            result->data_len = static_cast<unsigned int>(packet.size());
            result->data = malloc(packet.size() ? packet.size() : 1);
            if (!packet.empty()) {
                memcpy(result->data, packet.data(), packet.size());
            }
            g_socket->handedOver.push_back(result);
            g_socket->forGekko.pop_front();
        }
        *length = static_cast<int>(g_socket->handedOver.size());
        return g_socket->handedOver.data();
    }

    void FreeData(void *data) {
        free(data);
    }

    GekkoNetAdapter g_adapter{SendData, ReceiveData, FreeData};
} // namespace

// --- the session ---

NeoGeoOnline::~NeoGeoOnline() {
    Stop();
}

void NeoGeoOnline::Stop() {
    if (m_session) {
        gekko_destroy(&m_session);
    }
    if (m_socket) {
        if (g_socket == m_socket) {
            g_socket = nullptr;
        }
        delete m_socket;
        m_socket = nullptr;
    }
    if (m_port) {
        cps2recomp::TheGame().online = false;
        m_port->online = false;
    }
}

bool NeoGeoOnline::Start(NeoGeoPort &port, const OnlineOptions &options, std::string &error) {
    m_port = &port;
    m_options = options;
    m_options.delay = std::clamp(m_options.delay, 0, kPredictionWindow);
    port.online = true;
    cps2recomp::TheGame().online = true;
    NeoGeo &board = *port.board;

    // The state GekkoNet keeps: the board as it saves itself, with room to spare (its size hardly changes)
    m_state.clear();
    Archive ar(m_state, true);
    board.Serialize(ar);
    m_stateSize = static_cast<uint32_t>(m_state.size() + m_state.size() / 4 + 4096);

    GekkoConfig config{};
    config.num_players = 2;
    config.max_spectators = 0;
    config.max_input_prediction_window = kPredictionWindow;
    config.input_size = sizeof(uint16_t);
    config.state_size = m_stateSize;
    config.desync_detection = true;
    config.check_distance = 10;

    if (options.mode == OnlineOptions::Mode::Stress) {
        gekko_create(&m_session, GekkoStressSession);
        gekko_start(m_session, &config);
        m_local = gekko_add_actor(m_session, GekkoLocalPlayer, nullptr);
        m_remote = gekko_add_actor(m_session, GekkoLocalPlayer, nullptr);
        gekko_set_local_delay(m_session, m_local, static_cast<unsigned char>(m_options.delay));
        gekko_set_local_delay(m_session, m_remote, static_cast<unsigned char>(m_options.delay));
        m_status = Status::Playing;
        return true;
    }

    // The hello: the game, its set's checksum (the cartridge's and the system's ROMs as loaded) and the version
    const cps2recomp::NeoGeoRoms &roms = board.Roms();
    uint32_t crc = 0;
    for (const std::vector<uint8_t> *data : {&roms.program, &roms.fix, &roms.soundProgram, &roms.samples, &roms.sprites,
                                             &roms.bios, &roms.fixBios, &roms.soundBios, &roms.zoom}) {
        crc = Crc(crc, *data);
    }
    char hello[64] = {};
    snprintf(hello, sizeof(hello), "%s|%08x|%s", GAME_ID, crc, NEOGEO_RECOMP_VERSION);
    m_hello.assign(hello, hello + strlen(hello));

    m_socket = new Socket;
    g_socket = m_socket;
    const bool host = options.mode == OnlineOptions::Mode::Host;
    m_socket->host = host;
    if (!m_socket->Open(host ? options.port : 0, error)) {
        return false;
    }
    if (!host && !m_socket->Resolve(options.host, options.port, error)) {
        return false;
    }
    gekko_create(&m_session, GekkoGameSession);
    gekko_start(m_session, &config);
    gekko_net_adapter_set(m_session, &g_adapter);
    gekko_set_disconnect_timeout(m_session, kTimeoutMs);
    GekkoNetAddress peer{const_cast<char *>(kPeer), sizeof(kPeer) - 1};
    // (the host is player 1, the one who joins player 2)
    if (host) {
        m_local = gekko_add_actor(m_session, GekkoLocalPlayer, nullptr);
        m_remote = gekko_add_actor(m_session, GekkoRemotePlayer, &peer);
    } else {
        m_remote = gekko_add_actor(m_session, GekkoRemotePlayer, &peer);
        m_local = gekko_add_actor(m_session, GekkoLocalPlayer, nullptr);
    }
    gekko_set_local_delay(m_session, m_local, static_cast<unsigned char>(m_options.delay));
    m_status = Status::Waiting;
    m_message = host ? "Waiting for the other player (port " + std::to_string(options.port) + ")"
                     : "Calling " + options.host + ":" + std::to_string(options.port);
    fprintf(stderr, "online: %s\n", m_message.c_str());
    return true;
}

void NeoGeoOnline::End(const std::string &message, bool failed) {
    m_status = Status::Ended;
    m_failed = failed;
    m_message = message;
    fprintf(stderr, "online: %s\n", message.c_str());
}

void NeoGeoOnline::SendHello() {
    if (m_socket->havePeer) {
        m_socket->Send('H', m_hello.data(), m_hello.size(), m_socket->peer);
    }
}

// The hellos that came: the host takes the first one's sender as its peer; each side checks it is the same game
void NeoGeoOnline::Pump() {
    m_socket->Pump();
    for (auto &[from, said] : m_socket->hellos) {
        if (!m_socket->havePeer && m_socket->host) {
            m_socket->peer = from;
            m_socket->havePeer = true;
        }
        if (!Socket::Same(from, m_socket->peer)) {
            continue; // (someone else: not the player this session is with)
        }
        if (m_socket->host) {
            SendHello(); // (each hello answered: the first answer may be lost)
        }
        if (m_heard) {
            continue;
        }
        if (said != m_hello) {
            const std::string theirs(said.begin(), said.end()), ours(m_hello.begin(), m_hello.end());
            const auto field = [](const std::string &text, int n) {
                size_t from = 0;
                for (int i = 0; i < n; i++) {
                    from = text.find('|', from);
                    from = from == std::string::npos ? text.size() : from + 1;
                }
                return text.substr(from, text.find('|', from) - from);
            };
            std::string why = "The other player has another game (" + field(theirs, 0) + ")";
            if (field(theirs, 0) == field(ours, 0)) {
                why = field(theirs, 1) != field(ours, 1) ? "The other player's ROM set is not the same as yours"
                                                         : "The other player has another version (" + field(theirs, 2) + ", yours " +
                                                               field(ours, 2) + ")";
            }
            End(why, true);
            return;
        }
        m_heard = true;
        m_status = Status::Connecting;
        m_message = "Connecting";
    }
    m_socket->hellos.clear();
}

int NeoGeoOnline::Tick(const uint16_t local[2]) {
    m_ticks++;
    m_shown = 0;
    if (m_status == Status::Ended) {
        return 0;
    }
    uint16_t mine = local[0];
    uint16_t other = local[1];
    // (the menu's coin: through this player's controls, as everything that changes the game must be)
    if (m_port->coinFrames > 0) {
        m_port->coinFrames--;
        mine |= NeoGeo::kCoin;
    }
    if (m_options.mode == OnlineOptions::Mode::Stress) {
        gekko_add_local_input(m_session, m_local, &mine);
        gekko_add_local_input(m_session, m_remote, &other);
        Run();
        return m_shown;
    }
    Pump();
    if (m_status == Status::Ended) {
        return 0;
    }
    if (!m_heard) {
        if (!m_socket->host && m_ticks % 15 == 1) {
            SendHello(); // (four times a second until the host answers)
        }
        return 0;
    }
    gekko_network_poll(m_session);
    gekko_add_local_input(m_session, m_local, &mine);
    Run();
    return m_shown;
}

void NeoGeoOnline::Run() {
    Events();
    if (m_status == Status::Ended) {
        return;
    }
    int count = 0;
    GekkoGameEvent **events = gekko_update_session(m_session, &count);
    NeoGeo &board = *m_port->board;
    for (int i = 0; i < count && m_status != Status::Ended; i++) {
        GekkoGameEvent *event = events[i];
        switch (event->type) {
        case GekkoSaveEvent: {
            m_state.clear();
            Archive ar(m_state, true);
            board.Serialize(ar);
            if (m_state.size() > m_stateSize) {
                End("The board's state outgrew what the session keeps", true);
                break;
            }
            memcpy(event->data.save.state, m_state.data(), m_state.size());
            *event->data.save.state_len = static_cast<unsigned int>(m_state.size());
            *event->data.save.checksum = Crc(0, m_state);
            break;
        }
        case GekkoLoadEvent: {
            std::vector<uint8_t> data(event->data.load.state, event->data.load.state + event->data.load.state_len);
            Archive ar(data, false);
            board.Serialize(ar);
            if (!ar.Ok()) {
                End("A state the session kept did not load", true);
            }
            break;
        }
        case GekkoAdvanceEvent: {
            uint16_t players[2] = {};
            memcpy(players, event->data.adv.inputs, std::min<size_t>(sizeof(players), event->data.adv.input_len));
            // A frame run again (or ahead) is neither seen nor heard: the chip runs the same, its samples dropped
            auto &sound = board.Sound();
            const bool keep = sound.keep;
            const bool again = event->data.adv.rolling_back || event->data.adv.running_ahead;
            sound.keep = keep && !again;
            m_port->RunFrame(players);
            sound.keep = keep;
            if (!again) {
                m_shown++;
            }
            break;
        }
        default: break;
        }
    }
    Events();
}

void NeoGeoOnline::Events() {
    int count = 0;
    GekkoSessionEvent **events = gekko_session_events(m_session, &count);
    for (int i = 0; i < count && m_status != Status::Ended; i++) {
        const GekkoSessionEvent *event = events[i];
        switch (event->type) {
        case GekkoPlayerSyncing:
            m_message = "Connecting " + std::to_string(event->data.syncing.current) + "/" + std::to_string(event->data.syncing.max);
            break;
        case GekkoSessionStarted:
            if (m_status != Status::Playing) {
                fprintf(stderr, "online: playing\n");
            }
            m_status = Status::Playing;
            m_message.clear();
            break;
        case GekkoPlayerDisconnected:
            End("The other player left", false);
            break;
        case GekkoDesyncDetected: {
            const auto &d = event->data.desynced;
            fprintf(stderr, "online: desync at frame %d (here %08x, the other side %08x)\n", d.frame, d.local_checksum, d.remote_checksum);
            End("Desync at frame " + std::to_string(d.frame), true);
            break;
        }
        default: break;
        }
    }
}

std::string NeoGeoOnline::Stats() const {
    if (!m_session || m_status != Status::Playing) {
        return "";
    }
    GekkoRollbackStats rollbacks{};
    gekko_rollback_stats(m_session, &rollbacks);
    char text[128];
    if (m_options.mode == OnlineOptions::Mode::Stress) {
        snprintf(text, sizeof(text), "STRESS  rollbacks %u", rollbacks.total_count);
    } else {
        GekkoNetworkStats net{};
        gekko_network_stats(m_session, m_remote, &net);
        snprintf(text, sizeof(text), "PING %u  DELAY %d  ROLLBACK %u", static_cast<unsigned>(net.last_ping), m_options.delay,
                 rollbacks.last_frames);
    }
    return text;
}

float NeoGeoOnline::FramesAhead() const {
    return m_session && m_status == Status::Playing ? gekko_frames_ahead(m_session) : 0.0f;
}

} // namespace port
