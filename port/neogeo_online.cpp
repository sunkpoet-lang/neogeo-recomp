// SPDX-License-Identifier: GPL-3.0-only
// Part of neogeo-recomp.
#include "neogeo_online.h"
#include "neogeo_match_flow.h"

#include "game_info.h"

#include "../runtime/game.h"
#include "../runtime/state.h"
#include "neogeo_port.h"

#include <gekkonet.h>
#include <zlib.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <filesystem>
#include <functional>
#include <map>
#include <tuple>
#include <charconv>
#include <random>
#include "../runtime/neogeo_game.h"

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
using cps2recomp::TheGame;

namespace {
    // Every packet starts with these three bytes and a kind: H, hello (before GekkoNet's own); G, GekkoNet's;
    // (match-spectators) W, a spectator's hello; X, the host's refusal, with its reason; Q, the host's status, to the
    // other player and each spectator four times a second: the room and epoch (big-endian), the phase, how many watch,
    // the two streaks, then the players' names, each ended by a 0 byte; V and U, the spectators' chat relay
    constexpr char kTag[3] = {'N', 'G', 'O'};
    constexpr int kMostSpectators = 6; // (the tunnel's eight guests: the opponent, six spectators, one kept free)

    // (the chat's clock, in milliseconds)
    uint64_t NowMs() {
        return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                         std::chrono::steady_clock::now().time_since_epoch()).count());
    }
    constexpr int kPredictionWindow = 8;
    constexpr unsigned kTimeoutMs = 5000; // (under the tunnel's 8 s without traffic)
    // The address GekkoNet is given for the other player: the socket knows where that is
    constexpr char kPeer[] = "peer";

    uint32_t Crc(uint32_t crc, const std::vector<uint8_t> &data) {
        // (zlib takes at most 4 GB a call; a set is far less)
        return static_cast<uint32_t>(crc32(crc, data.data(), static_cast<uInt>(data.size())));
    }
    uint64_t FreshRecoveryNonce();
    bool RecoveryHelloNonce(std::vector<uint8_t>&,uint64_t&);
    void SaveTo(std::vector<uint8_t>& data,NeoGeo& board){data.clear();Archive ar(data,true);board.Serialize(ar);}
    bool LoadFrom(std::vector<uint8_t>& data,NeoGeo& board){Archive ar(data,false);board.Serialize(ar);return ar.Ok();}
} // namespace

// --- the spectators' chat relay ---
//
// The players' own chat (C and A) is unchanged. With spectators, the host relays every line to whoever did not say
// it, on a channel of its own to each of the others: V, a line with who said it (its number, little-endian like the
// chat's; the role, 0 the host, 1 the other player, 2 a spectator; the name's length and the name; the text's length,
// two bytes, and the text), and U, how many have come (the chat's A). Lines are resent every quarter second until
// acknowledged, and delivered once and in order (docs/chat-protocol.md, Spectators).
class ChatRelay {
public:
    using Send = std::function<void(char kind, const std::vector<uint8_t> &payload)>;
    struct Line {
        int role = 0;
        std::string name, text;
    };
    explicit ChatRelay(Send send) : m_send(std::move(send)) {}

    void Push(const Line &line, uint64_t now) {
        std::vector<uint8_t> payload(4);
        const uint32_t number = m_next++;
        for (int i = 0; i < 4; i++) {
            payload[i] = static_cast<uint8_t>(number >> (8 * i));
        }
        const std::string name = line.name.substr(0, 255);
        payload.push_back(static_cast<uint8_t>(line.role));
        payload.push_back(static_cast<uint8_t>(name.size()));
        payload.insert(payload.end(), name.begin(), name.end());
        const std::string text = line.text.substr(0, 4096);
        payload.push_back(static_cast<uint8_t>(text.size()));
        payload.push_back(static_cast<uint8_t>(text.size() >> 8));
        payload.insert(payload.end(), text.begin(), text.end());
        m_out.emplace_back(number, std::move(payload));
        m_send('V', m_out.back().second);
        m_sentAt = now;
    }
    void Receive(char kind, const uint8_t *data, size_t size, uint64_t now) {
        (void)now;
        if (kind == 'U') {
            uint32_t count = 0;
            if (!recomp::chat::DecodeAck(data, size, count)) {
                return;
            }
            while (!m_out.empty() && m_out.front().first < count) {
                m_out.pop_front();
            }
            return;
        }
        if (kind != 'V' || size < 8) {
            return;
        }
        const uint32_t number = data[0] | data[1] << 8 | data[2] << 16 | static_cast<uint32_t>(data[3]) << 24;
        const int role = data[4];
        const size_t nameSize = data[5];
        if (6 + nameSize + 2 > size) {
            return;
        }
        const size_t textSize = data[6 + nameSize] | data[7 + nameSize] << 8;
        if (8 + nameSize + textSize > size) {
            return;
        }
        if (number == m_heardUpTo) {
            Line line;
            line.role = role;
            recomp::chat::Clean(std::string(reinterpret_cast<const char *>(data + 6), nameSize), recomp::chat::kNameMost, line.name);
            recomp::chat::Clean(std::string(reinterpret_cast<const char *>(data + 8 + nameSize), textSize), recomp::chat::kLineMost,
                                line.text);
            if (!line.text.empty()) {
                m_heard.push_back(std::move(line));
            }
            m_heardUpTo++;
        }
        m_send('U', recomp::chat::EncodeAck(m_heardUpTo));
    }
    void Pump(uint64_t now) {
        if (!m_out.empty() && now - m_sentAt >= recomp::chat::kResendMs) {
            for (const auto &[number, payload] : m_out) {
                m_send('V', payload);
            }
            m_sentAt = now;
        }
    }
    std::vector<Line> Take() {
        std::vector<Line> lines;
        lines.swap(m_heard);
        return lines;
    }

private:
    Send m_send;
    std::deque<std::pair<uint32_t, std::vector<uint8_t>>> m_out;
    uint32_t m_next = 0, m_heardUpTo = 0;
    uint64_t m_sentAt = 0;
    std::vector<Line> m_heard;
};

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
    bool recovery = false;
    uint64_t room=0,epoch=1,lastReceived=0,stalePackets=0;
    std::vector<std::pair<char,std::vector<uint8_t>>> controls;
    std::deque<std::vector<uint8_t>> forGekko;   // GekkoNet's packets from the peer, until it asks
    std::vector<GekkoNetResult *> handedOver;    // (what receive_data last returned: the array is ours)
    std::vector<std::pair<sockaddr_in, std::vector<uint8_t>>> hellos;
    std::vector<std::pair<char, std::vector<uint8_t>>> forChat; // (this fork: the chat's C and A, from the peer)
    // (match-spectators) the host's: each spectator's address and GekkoNet's name for it, when each was last heard,
    // their hellos and chat packets; the spectator's: the host's refusal and last status; both: the relay's packets
    std::vector<std::pair<sockaddr_in, std::string>> watchers;
    std::vector<std::pair<sockaddr_in, std::vector<uint8_t>>> watchHellos;
    std::deque<std::string> forGekkoFrom;
    std::map<std::string, uint64_t> watcherHeard;
    std::vector<std::tuple<std::string, char, std::vector<uint8_t>>> fromWatchers;
    std::vector<std::pair<char, std::vector<uint8_t>>> forRelay;
    std::string refused;
    std::vector<uint8_t> status;
    uint64_t statusMs = 0;
    int chatLoss = 0;
    uint32_t lossSeed = 12345;
    bool ChatLost() {
        lossSeed = lossSeed * 1103515245u + 12345u;
        return static_cast<int>((lossSeed >> 16) % 100) < chatLoss;
    }
    const std::string *WatcherName(const sockaddr_in &from) const {
        for (const auto &[at, name] : watchers) {
            if (Same(at, from)) {
                return &name;
            }
        }
        return nullptr;
    }
    const sockaddr_in *WatcherAt(const char *name, size_t size) const {
        for (const auto &[at, one] : watchers) {
            if (one.size() == size && !memcmp(one.data(), name, size)) {
                return &at;
            }
        }
        return nullptr;
    }

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
        // (match-spectators) room for a burst: GekkoNet sends a late spectator the match's state as hundreds of
        // datagrams of 900 bytes at once, more than the system's default buffer holds
        const int buffered = 4 << 20;
        setsockopt(handle, SOL_SOCKET, SO_RCVBUF, reinterpret_cast<const char *>(&buffered), sizeof(buffered));
        setsockopt(handle, SOL_SOCKET, SO_SNDBUF, reinterpret_cast<const char *>(&buffered), sizeof(buffered));
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
        std::vector<uint8_t> wrapped;
        if(recovery && (kind=='G' || kind=='C' || kind=='A')) {
            if(!room)return;
            Recovery::Put(wrapped,room);Recovery::Put(wrapped,epoch);
            if(size){const auto* first=static_cast<const uint8_t*>(data);wrapped.insert(wrapped.end(),first,first+size);}
            data=wrapped.data();size=wrapped.size();
        }
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

    // All payloads in an opted-in session are tied to the pinned peer/room/epoch.
    void Pump() {
        uint8_t buffer[65536];
        while(true) {
            sockaddr_in from{};
#ifdef _WIN32
            int length=sizeof(from);
#else
            socklen_t length=sizeof(from);
#endif
            const int got=static_cast<int>(recvfrom(handle,reinterpret_cast<char*>(buffer),sizeof(buffer),0,reinterpret_cast<sockaddr*>(&from),&length));
            if(got<0) {
#ifdef _WIN32
                if(WSAGetLastError()==WSAECONNRESET)continue;
#endif
                break;
            }
            if(got<4 || memcmp(buffer,kTag,3)!=0)continue;
            const char kind=static_cast<char>(buffer[3]);
            if(kind=='H'){hellos.emplace_back(from,std::vector<uint8_t>(buffer+4,buffer+got));continue;}
            if(kind=='W'){if(host && watchHellos.size()<64)watchHellos.emplace_back(from,std::vector<uint8_t>(buffer+4,buffer+got));continue;}
            if(kind=='X'){if(!host && havePeer && Same(from,peer))refused.assign(reinterpret_cast<const char*>(buffer+4),static_cast<size_t>(got-4));continue;}
            if(kind=='Q'){if(!host && havePeer && Same(from,peer) && got>=24){status.assign(buffer+4,buffer+got);statusMs=NowMs();}continue;}
            // (a spectator's packets, at the host, with the room and epoch when the match has them)
            if(host && (kind=='G' || kind=='C' || kind=='A' || kind=='U')) {
                if(const std::string* name=WatcherName(from)) {
                    const uint8_t* bytes=buffer+4;size_t count=static_cast<size_t>(got-4);
                    if(recovery && kind!='U') {
                        if(count<16 || !room || Recovery::Get(bytes)!=room)continue;
                        if(Recovery::Get(bytes+8)!=epoch){++stalePackets;continue;}
                        bytes+=16;count-=16;
                    }
                    watcherHeard[*name]=NowMs();
                    if(kind=='G'){forGekko.emplace_back(bytes,bytes+count);forGekkoFrom.push_back(*name);}
                    else if(fromWatchers.size()<256)fromWatchers.emplace_back(*name,kind,std::vector<uint8_t>(bytes,bytes+count));
                    continue;
                }
            }
            if(!havePeer || !Same(from,peer))continue;
            if(recovery && std::strchr("NRKSTPL",kind) && got==20) {
                if(controls.size()<64)controls.emplace_back(kind,std::vector<uint8_t>(buffer+4,buffer+got));
                continue;
            }
            if(kind=='V' || kind=='U'){if(forRelay.size()<256)forRelay.emplace_back(kind,std::vector<uint8_t>(buffer+4,buffer+got));continue;}
            if(kind!='G' && kind!='C' && kind!='A')continue;
            const uint8_t* payload=buffer+4;size_t size=static_cast<size_t>(got-4);
            if(recovery) {
                if(size<16 || !room || Recovery::Get(payload)!=room)continue;
                if(Recovery::Get(payload+8)!=epoch){++stalePackets;continue;}
                payload+=16;size-=16;
            }
            lastReceived=NowMs();
            if(kind=='G'){forGekko.emplace_back(payload,payload+size);forGekkoFrom.push_back(kPeer);}
            else forChat.emplace_back(kind,std::vector<uint8_t>(payload,payload+size));
        }
    }

};

namespace {
    NeoGeoOnline::Socket *g_socket = nullptr; // (GekkoNet's callbacks carry no pointer of ours)

    void SendData(GekkoNetAddress *addr, const char *data, int length) {
        if (!g_socket) {
            return;
        }
        // (match-spectators) the address GekkoNet gives is a name: the peer's, or a spectator's
        if (addr && addr->data && g_socket->host) {
            if (const sockaddr_in *at = g_socket->WatcherAt(static_cast<const char *>(addr->data), addr->size)) {
                g_socket->Send('G', data, static_cast<size_t>(length), *at);
                return;
            }
        }
        if (g_socket->havePeer) {
            g_socket->Send('G', data, static_cast<size_t>(length), g_socket->peer);
        }
    }

    GekkoNetResult **ReceiveData(int *length) {
        g_socket->handedOver.clear();
        while (!g_socket->forGekko.empty()) {
            std::vector<uint8_t> &packet = g_socket->forGekko.front();
            auto *result = static_cast<GekkoNetResult *>(malloc(sizeof(GekkoNetResult)));
            const std::string from = g_socket->forGekkoFrom.empty() ? std::string(kPeer) : g_socket->forGekkoFrom.front();
            if (!g_socket->forGekkoFrom.empty()) {
                g_socket->forGekkoFrom.pop_front();
            }
            result->addr.size = static_cast<unsigned int>(from.size());
            result->addr.data = malloc(result->addr.size);
            memcpy(result->addr.data, from.data(), result->addr.size);
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
    if(m_socket && ChatHost())for(int i=0;i<3;++i)SendStatus(4); // (match-spectators: the spectators told, and the other player)
    if(m_options.matchFlow&&m_heard&&m_socket)for(int i=0;i<3;++i)SendControl('L',Epoch());
    if(m_checkpointLog){std::fclose(m_checkpointLog);m_checkpointLog=nullptr;}
    if(m_flowLog){std::fclose(m_flowLog);m_flowLog=nullptr;}
    if(m_socket&&m_socket->stalePackets)fprintf(stderr,"online: ignored stale epoch packets: %llu\n",static_cast<unsigned long long>(m_socket->stalePackets));
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
        m_port->arcadeWaiting=false;
        m_port=nullptr;
    }
}

bool NeoGeoOnline::Start(NeoGeoPort &port, const OnlineOptions &options, std::string &error) {
    if(m_session||m_socket){error="An online session is already active";return false;}
    if(options.matchFlow&&!VerifiedMatchFlowGame(GAME_ID)){error="ForkAde match flow is not verified for this game";return false;}
    m_port = &port;
    m_options = options;
    m_status=Status::Waiting;m_failed=false;m_heard=false;m_ticks=0;m_theirName.clear();m_chatScreen={};
    m_recovery.Reset(options.mode==OnlineOptions::Mode::Host,NowMs());m_prepared=m_everPlayed=false;
    if(options.matchFlow){try{m_localNonce=FreshRecoveryNonce();}catch(...){error="A recovery session nonce could not be created";return false;}m_flow.Reset();PublishFlow();}
    auto openLog=[&](const std::string& name,FILE*& file){
        if(name.empty())return true;
#ifdef _WIN32
        file=_wfopen(std::filesystem::path(std::u8string(reinterpret_cast<const char8_t*>(name.c_str()))).c_str(),L"w");
#else
        file=std::fopen(name.c_str(),"w");
#endif
        return file!=nullptr;
    };
    if(!openLog(options.checkpointLog,m_checkpointLog)||!openLog(options.flowLog,m_flowLog)){error="The online evidence log could not be opened";return false;}
    port.arcadeWaiting=options.matchFlow&&options.arcadeWait&&options.mode==OnlineOptions::Mode::Host;
    m_options.delay = std::clamp(m_options.delay, 0, kPredictionWindow);
    port.online = true;
    cps2recomp::TheGame().online = true;
    NeoGeo &board = *port.board;

    // The state GekkoNet keeps: the board as it saves itself, with room to spare (its size hardly changes)
    SaveTo(m_state,board);
    if(options.matchFlow)m_arcadeState=m_state;
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
    const std::string helloText=std::string(hello)+(options.matchFlow?"|forkade-recovery-1":"");
    m_hello.assign(helloText.begin(),helloText.end());

    m_socket = new Socket;
    // ForkAde's in-game chat, on the session's socket
    Socket *socket = m_socket;
    socket->chatLoss = options.chatLoss; // (--chat-loss: dropped at random, as a bad network would)
    m_chat = std::make_unique<recomp::chat::Channel>([socket](char kind, const std::vector<uint8_t> &payload) {
        if (socket->havePeer && !socket->ChatLost()) {
            socket->Send(kind, payload.data(), payload.size(), socket->peer);
        }
    });
    // (match-spectators) the host's relay to the other player, or this side's from the host
    m_relay = std::make_shared<ChatRelay>([socket](char kind, const std::vector<uint8_t> &payload) {
        if (socket->havePeer && !socket->ChatLost()) {
            socket->Send(kind, payload.data(), payload.size(), socket->peer);
        }
    });
    g_socket = m_socket;
    const bool host = options.mode == OnlineOptions::Mode::Host;
    m_socket->host = host;m_socket->recovery=options.matchFlow;m_socket->room=host?m_localNonce:0;
    if (!m_socket->Open(host ? options.port : 0, error)) {
        return false;
    }
    if (!host && !m_socket->Resolve(options.host, options.port, error)) {
        return false;
    }
    if (Spectating()) {
        // (match-spectators) a spectator: its session is made once the host has answered
        m_status = Status::Waiting;
        m_message = "Calling " + options.host + ":" + std::to_string(options.port) + " to watch";
        fprintf(stderr, "online: %s\n", m_message.c_str());
        return true;
    }
    if(!CreateGameSession()){error="The rollback session could not start";return false;}
    m_status = Status::Waiting;
    m_message = port.arcadeWaiting ? "WAITING FOR A CHALLENGER" : host ? "Waiting for the other player (port " + std::to_string(options.port) + ")"
                     : "Calling " + options.host + ":" + std::to_string(options.port);
    fprintf(stderr, "online: %s\n", m_message.c_str());
    return true;
}

namespace {
uint64_t FreshRecoveryNonce() {
    std::random_device random;
    const auto value=(static_cast<uint64_t>(random())<<32)^static_cast<uint64_t>(random());
    return value?value:1;
}
bool RecoveryHelloNonce(std::vector<uint8_t>& hello,uint64_t& nonce) {
    const std::string text(hello.begin(),hello.end());
    const auto colon=text.rfind(':');
    if(colon==std::string::npos || text.size()-colon-1!=16)return false;
    const std::string base=text.substr(0,colon);
    if(!base.ends_with("|forkade-recovery-1"))return false;
    const auto parsed=std::from_chars(text.data()+colon+1,text.data()+text.size(),nonce,16);
    if(parsed.ec!=std::errc{} || parsed.ptr!=text.data()+text.size() || !nonce)return false;
    hello.assign(base.begin(),base.end());return true;
}
}

bool NeoGeoOnline::CreateGameSession() {
    GekkoConfig config{};
    // (match-spectators: the host's only; added once the match runs, from its confirmed state)
    const int spectators=ChatHost() ? std::clamp(m_options.spectators,0,kMostSpectators) : 0;
    config.num_players=2;config.max_spectators=static_cast<unsigned char>(spectators);config.max_input_prediction_window=kPredictionWindow;
    config.input_size=sizeof(uint16_t);config.state_size=m_stateSize;
    config.desync_detection=true;config.check_distance=10;
    if(!gekko_create(&m_session,GekkoGameSession))return false;
    gekko_start(m_session,&config);
    gekko_net_adapter_set(m_session,&g_adapter);gekko_set_disconnect_timeout(m_session,kTimeoutMs);
    GekkoNetAddress peer{const_cast<char*>(kPeer),sizeof(kPeer)-1};
    if(ChatHost()) {m_local=gekko_add_actor(m_session,GekkoLocalPlayer,nullptr);m_remote=gekko_add_actor(m_session,GekkoRemotePlayer,&peer);}
    else {m_remote=gekko_add_actor(m_session,GekkoRemotePlayer,&peer);m_local=gekko_add_actor(m_session,GekkoLocalPlayer,nullptr);}
    gekko_set_local_delay(m_session,m_local,static_cast<unsigned char>(m_options.delay));
    return m_local>=0 && m_remote>=0;
}

void NeoGeoOnline::SendControl(char kind,uint64_t epoch) {
    if(!m_socket || !m_socket->havePeer || !m_roomNonce || !kind)return;
    std::vector<uint8_t> bytes;Recovery::Put(bytes,m_roomNonce);Recovery::Put(bytes,epoch);
    m_socket->Send(kind,bytes.data(),bytes.size(),m_socket->peer);
}

void NeoGeoOnline::PrepareRecovery() {
    if(m_prepared)return;
    // Outside all SDK event iterations: keep socket, peer and reliable chat channel.
    if(m_session)gekko_destroy(&m_session);
    m_socket->forGekko.clear();m_socket->forGekkoFrom.clear();m_socket->forChat.clear();
    for(Spectator& one:m_spectators)one.handle=-1; // (match-spectators: re-added when the new session runs)
    m_prepared=true;
}

void NeoGeoOnline::RequestRecovery() {
    if(!m_options.matchFlow || !m_everPlayed){End("The other player left",false);return;}
    if(m_recovery.Waiting())return;
    m_status=Status::Recovering;
    const auto action=m_recovery.Begin(NowMs());
    if(action.expired){End("The reconnection sequence is exhausted",true);return;}
    SendControl(action.reply,action.epoch);
    fprintf(stderr,"online: recovery requested, fixed 60-second deadline\n");
}

bool NeoGeoOnline::ResetRecoveryEpoch() {
    if(m_session)gekko_destroy(&m_session);
    if(!LoadFrom(m_arcadeState,*m_port->board)){End("The initial recovery state could not be restored",true);return false;}
    m_socket->forGekko.clear();m_socket->forGekkoFrom.clear();m_socket->forChat.clear();
    m_socket->epoch=m_recovery.Epoch();
    for(Spectator& one:m_spectators)one.handle=-1; // (match-spectators: re-added when the new session runs)
    m_port->frame=0;m_port->coinFrames=m_port->start2Frames=0;m_port->board->Sound().made.clear();
    m_port->arcadeWaiting=false;m_port->paused=false;m_port->steps=0;m_port->menu.Close();
    m_flow.Reset();PublishFlow();
    if(!CreateGameSession()){End("The recovered rollback session could not start",true);return false;}
    if(m_checkpointLog)std::fprintf(m_checkpointLog,"# epoch %llu\n",static_cast<unsigned long long>(Epoch()));
    if(m_flowLog)std::fprintf(m_flowLog,"# epoch %llu\n",static_cast<unsigned long long>(Epoch()));
    fprintf(stderr,"online: restored epoch %llu at frame 0\n",static_cast<unsigned long long>(Epoch()));
    m_prepared=true;return true;
}

void NeoGeoOnline::ApplyRecovery(const Recovery::Action& action) {
    if(action.expired){End("Reconnection timed out after 60 seconds",true);return;}
    if(action.left){End("The other player left",false);return;}
    if(m_recovery.Waiting()) {
        m_status=Status::Recovering;PrepareRecovery();
    }
    if(action.reset && !ResetRecoveryEpoch())return;
    if(action.ready){m_status=Status::Connecting;m_prepared=false;}
    SendControl(action.reply,action.epoch);
}

void NeoGeoOnline::End(const std::string &message, bool failed) {
    m_status = Status::Ended;
    m_failed = failed;
    m_message = message;
    fprintf(stderr, "online: %s\n", message.c_str());
}

// (this fork: with this player's name after a 0 byte, and the match flow's nonce before it)
std::vector<uint8_t> NeoGeoOnline::HelloOut() const {
    auto base=m_hello;
    if(m_options.matchFlow) {
        char nonce[18];std::snprintf(nonce,sizeof nonce,":%016llx",static_cast<unsigned long long>(m_localNonce));
        base.insert(base.end(),nonce,nonce+17);
    }
    return recomp::chat::HelloWithName(base, m_options.name);
}

void NeoGeoOnline::SendHello() {
    if (m_socket->havePeer) {
        const std::vector<uint8_t> hello = HelloOut();
        m_socket->Send(Spectating() ? 'W' : 'H', hello.data(), hello.size(), m_socket->peer);
    }
}

// Why a hello is not this match's ("" when it is): another game, ROM set or version
std::string NeoGeoOnline::Mismatch(const std::vector<uint8_t> &said, bool recoveryCompatible) const {
    if (said == m_hello && recoveryCompatible) {
        return "";
    }
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
    if(m_options.matchFlow && !recoveryCompatible)why="The other player has incompatible ForkAde recovery protocol";
    return why;
}

// --- (match-spectators) the spectators, at the host and on their side ---

int NeoGeoOnline::Watching() const {
    return static_cast<int>(m_spectators.size());
}

bool NeoGeoOnline::CreateSpectateSession() {
    GekkoConfig config{};
    config.num_players=2;config.max_spectators=0;config.max_input_prediction_window=kPredictionWindow;
    config.spectator_delay=static_cast<unsigned int>(std::clamp(m_options.spectatorDelay,1,300));
    config.input_size=sizeof(uint16_t);config.state_size=m_stateSize;
    config.desync_detection=false;
    if(!gekko_create(&m_session,GekkoSpectateSession))return false;
    gekko_start(m_session,&config);
    gekko_net_adapter_set(m_session,&g_adapter);gekko_set_disconnect_timeout(m_session,kTimeoutMs);
    GekkoNetAddress peer{const_cast<char*>(kPeer),sizeof(kPeer)-1};
    m_remote=gekko_add_actor(m_session,GekkoRemotePlayer,&peer);
    return m_remote>=0;
}

void NeoGeoOnline::SendStatus(int phase) {
    if(!m_socket || !ChatHost() || !m_port)return;
    std::vector<uint8_t> q;
    Recovery::Put(q,m_options.matchFlow?m_localNonce:0);Recovery::Put(q,m_socket->epoch);
    q.push_back(static_cast<uint8_t>(phase));q.push_back(static_cast<uint8_t>(Watching()));
    for(int side=0;side<2;++side)q.push_back(static_cast<uint8_t>(std::clamp(m_port->streak[side],0,255)));
    for(const std::string& name:{ChatName(0),m_heard?ChatName(1):std::string()}){q.insert(q.end(),name.begin(),name.end());q.push_back(0);}
    if(m_heard && m_socket->havePeer)m_socket->Send('Q',q.data(),q.size(),m_socket->peer);
    for(const auto& [at,name]:m_socket->watchers)m_socket->Send('Q',q.data(),q.size(),at);
}

void NeoGeoOnline::ApplyStatus() {
    if(!m_socket || m_socket->host || m_socket->status.size()<24)return;
    const std::vector<uint8_t> q=std::move(m_socket->status);m_socket->status.clear();
    const uint64_t room=Recovery::Get(q.data()),epoch=Recovery::Get(q.data()+8);
    if(m_options.matchFlow && m_roomNonce && room!=m_roomNonce)return;
    m_seenWatching=q[17];
    if(!Spectating())return;
    size_t at=20;
    for(std::string& name:m_statusNames) {
        size_t end=at;
        while(end<q.size() && q[end])end++;
        name=recomp::chat::Printable(std::string(q.begin()+static_cast<std::ptrdiff_t>(at),q.begin()+static_cast<std::ptrdiff_t>(end)),recomp::chat::kNameMost);
        at=std::min(q.size(),end+1);
    }
    m_port->streak[0]=q[18];m_port->streak[1]=q[19];
    if(m_hostPhase!=q[16])fprintf(stderr,"online: watching: the host's phase %d (epoch %llu)\n",q[16],static_cast<unsigned long long>(epoch));
    m_hostPhase=q[16];
    auto drop=[&]{if(m_session)gekko_destroy(&m_session);m_socket->forGekko.clear();m_socket->forGekkoFrom.clear();};
    if(m_hostPhase==4){drop();End("The host stopped hosting",false);return;}
    if(m_hostPhase==3) {
        if(m_status!=Status::Recovering)fprintf(stderr,"online: watching: the players are reconnecting\n");
        drop();m_status=Status::Recovering;m_message="Reconnecting";return;
    }
    if(m_options.matchFlow && epoch!=m_socket->epoch) {
        fprintf(stderr,"online: watching epoch %llu\n",static_cast<unsigned long long>(epoch));
        drop();m_socket->epoch=epoch;
    }
    if(m_heard && !m_session) {
        if(!CreateSpectateSession()){End("The spectator session could not start",true);return;}
        m_status=Status::Connecting;
    }
    if(m_status!=Status::Playing)
        m_message=m_hostPhase!=0?"Waiting for the match"
                  :m_statusNames[0].empty()?"WAITING FOR A CHALLENGER":m_statusNames[0]+" IS WAITING FOR A CHALLENGER";
}

void NeoGeoOnline::AddSpectators() {
    if(!m_session || m_status!=Status::Playing || !ChatHost())return;
    for(Spectator& one:m_spectators) {
        if(one.handle>=0)continue;
        GekkoNetAddress at{const_cast<char*>(one.name.data()),static_cast<unsigned int>(one.name.size())};
        one.handle=gekko_add_actor(m_session,GekkoSpectator,&at);
        fprintf(stderr,"online: spectator %s (%s) %s at frame %llu\n",one.name.c_str(),one.nick.c_str(),one.handle>=0?"added":"not added",
                static_cast<unsigned long long>(m_port->board->Frames()));
    }
}

void NeoGeoOnline::SpectatorLeft(int handle) {
    for(size_t i=0;i<m_spectators.size();++i) {
        if(m_spectators[i].handle!=handle)continue;
        fprintf(stderr,"online: spectator %s left\n",m_spectators[i].name.c_str());
        m_socket->watcherHeard.erase(m_spectators[i].name);
        auto& list=m_socket->watchers;
        list.erase(std::remove_if(list.begin(),list.end(),[&](const auto& w){return w.second==m_spectators[i].name;}),list.end());
        m_spectators.erase(m_spectators.begin()+static_cast<std::ptrdiff_t>(i));
        return;
    }
}

void NeoGeoOnline::WatchHellos() {
    const uint64_t now=NowMs();
    for(size_t i=0;i<m_spectators.size();) {
        const Spectator& one=m_spectators[i];
        const auto heard=m_socket->watcherHeard.find(one.name);
        if(one.handle<0 && (heard==m_socket->watcherHeard.end() || now-heard->second>10000)) {
            fprintf(stderr,"online: spectator %s gone\n",one.name.c_str());
            auto& list=m_socket->watchers;
            list.erase(std::remove_if(list.begin(),list.end(),[&](const auto& w){return w.second==one.name;}),list.end());
            m_socket->watcherHeard.erase(one.name);
            m_spectators.erase(m_spectators.begin()+static_cast<std::ptrdiff_t>(i));
            continue;
        }
        ++i;
    }
    auto hellos=std::move(m_socket->watchHellos);m_socket->watchHellos.clear();
    for(auto& [from,whole]:hellos) {
        std::vector<uint8_t> said;std::string name;
        recomp::chat::SplitHello(whole,said,name);
        auto refuse=[&](const std::string& why){m_socket->Send('X',why.data(),why.size(),from);};
        if(!m_socket->WatcherName(from)) {
            const int most=std::clamp(m_options.spectators,0,kMostSpectators);
            uint64_t theirNonce=0;
            if(m_options.matchFlow && !RecoveryHelloNonce(said,theirNonce)){refuse(Mismatch(said,false));continue;}
            if(most==0){refuse("This match does not allow spectators");continue;}
            if(static_cast<int>(m_spectators.size())>=most){refuse("The match is full ("+std::to_string(most)+" watching)");continue;}
            const std::string why=Mismatch(said,true);
            if(!why.empty()){refuse(why);continue;}
            int number=1;
            for(bool taken=true;taken;) {
                taken=false;
                for(const Spectator& one:m_spectators)taken|=one.name=="spec"+std::to_string(number);
                if(taken)number++;
            }
            Spectator one;one.name="spec"+std::to_string(number);one.nick=name;
            Socket* socket=m_socket;const std::string to=one.name;
            auto send=[socket,to](char kind,const std::vector<uint8_t>& payload) {
                if(socket->ChatLost())return;
                if(const sockaddr_in* at=socket->WatcherAt(to.data(),to.size()))socket->Send(kind,payload.data(),payload.size(),*at);
            };
            one.own=std::make_shared<recomp::chat::Channel>(send);
            one.relay=std::make_shared<ChatRelay>(send);
            m_spectators.push_back(one);m_socket->watchers.emplace_back(from,one.name);
            fprintf(stderr,"online: spectator %s (%s) said hello\n",one.name.c_str(),name.c_str());
        }
        if(const std::string* known=m_socket->WatcherName(from))m_socket->watcherHeard[*known]=now;
        const std::vector<uint8_t> hello=HelloOut();
        m_socket->Send('H',hello.data(),hello.size(),from);
    }
    AddSpectators();
}

// The hellos that came: the host takes the first one's sender as its peer; each side checks it is the same game
void NeoGeoOnline::Pump() {
    m_socket->Pump();
    for (auto &[from, whole] : m_socket->hellos) {
        // (the name after a 0 byte is not compared)
        std::vector<uint8_t> said;
        std::string name;
        recomp::chat::SplitHello(whole, said, name);
        if (!m_socket->havePeer && m_socket->host) {
            m_socket->peer = from;
            m_socket->havePeer = true;
        }
        if (!Socket::Same(from, m_socket->peer)) {
            // (someone else: not the player this session is with; told so, as a spectator would be)
            if (m_socket->host) {
                const std::string why = "The match is full: two players are already playing";
                m_socket->Send('X', why.data(), why.size(), from);
            }
            continue;
        }
        if (m_socket->host) {
            SendHello(); // (each hello answered: the first answer may be lost)
        }
        if (m_heard) {
            continue;
        }
        uint64_t peerNonce=0;
        const bool recoveryCompatible=!m_options.matchFlow||RecoveryHelloNonce(said,peerNonce);
        if (const std::string why = Mismatch(said, recoveryCompatible); !why.empty()) {
            End(why, true);
            return;
        }
        if(m_options.matchFlow) {
            m_peerNonce=peerNonce;m_roomNonce=ChatHost()?m_localNonce:m_peerNonce;
            m_socket->room=m_roomNonce;m_socket->lastReceived=NowMs();
        }
        if(m_port->arcadeWaiting) {
            const auto solo=m_port->frame;
            if(!LoadFrom(m_arcadeState,*m_port->board)){End("The initial arcade state could not be restored",true);return;}
            m_port->frame=0;m_port->coinFrames=m_port->start2Frames=0;m_port->board->Sound().made.clear();m_port->cameIn=m_port->otherIn=false;
            m_port->arcadeWaiting=false;m_port->menu.Close();m_port->paused=false;m_port->steps=0;
            fprintf(stderr,"online: arcade reset after %llu solo frames\n",static_cast<unsigned long long>(solo));
        }
        if(m_options.matchFlow)m_challengerUntil=NowMs()+1200;
        m_heard = true;
        m_theirName = name;
        m_status = Status::Connecting;
        m_message = "Connecting";
        if (Spectating()) {
            // (match-spectators) the host took this spectator: its session, which waits for the match's state
            if (!CreateSpectateSession()) {
                End("The spectator session could not start", true);
                return;
            }
            m_message = "Waiting for the match";
        }
    }
    m_socket->hellos.clear();
    if (m_socket->host) {
        WatchHellos();
    } else if (!m_socket->refused.empty()) {
        End(recomp::chat::Printable(m_socket->refused, 120), true);
        return;
    } else {
        ApplyStatus();
        if (m_status == Status::Ended) {
            return;
        }
    }
    auto controls=std::move(m_socket->controls);m_socket->controls.clear();
    for(const auto& [kind,payload]:controls) {
        if(!m_options.matchFlow || !m_heard || payload.size()!=16 || Recovery::Get(payload.data())!=m_roomNonce)continue;
        const auto action=m_recovery.Receive(kind,Recovery::Get(payload.data()+8),NowMs());
        if(action.accepted)m_socket->lastReceived=NowMs();
        ApplyRecovery(action);
        if(m_status==Status::Ended)break;
    }
    // (the chat's packets, once the other side is known)
    for (const auto &[kind, payload] : m_socket->forChat) {
        if (m_heard && m_chat) {
            m_chat->Receive(kind, payload.data(), payload.size(), NowMs());
        }
    }
    m_socket->forChat.clear();
    // (match-spectators) the relay's packets from the host; at the host, each spectator's
    for (const auto &[kind, payload] : m_socket->forRelay) {
        if (m_heard && m_relay) {
            m_relay->Receive(kind, payload.data(), payload.size(), NowMs());
        }
    }
    m_socket->forRelay.clear();
    for (const auto &[name, kind, payload] : m_socket->fromWatchers) {
        for (Spectator &one : m_spectators) {
            if (one.name != name) {
                continue;
            }
            if (kind == 'U') {
                one.relay->Receive(kind, payload.data(), payload.size(), NowMs());
            } else {
                one.own->Receive(kind, payload.data(), payload.size(), NowMs());
            }
        }
    }
    m_socket->fromWatchers.clear();
}

void NeoGeoOnline::PublishFlow() {
    const auto& state = m_flow.state;
    m_port->cameIn = state.joined[ChatLocal()];
    m_port->otherIn = state.joined[1 - ChatLocal()];
    for (int side = 0; side < 2; ++side) {
        m_port->streak[side] = state.streak[side];
        m_port->roundsWon[side] = state.rounds[side];
    }
    m_port->roundCounted = state.counted;
    m_port->roundQuiet = state.quiet;
}

void NeoGeoOnline::UpdateFlowMessage() {
    if(!m_options.matchFlow || m_status==Status::Ended || Spectating())return;
    if(Recovering())m_message=ChatHost()?"Waiting for the same player (up to 60 seconds)":"Rejoining the same match (up to 60 seconds)";
    else if(m_heard && NowMs()<m_challengerUntil)m_message=Epoch()>1?"Connection restored - starting again":"Here comes a new challenger";
    else if(m_status==Status::Connecting && m_message=="Here comes a new challenger")m_message="Connecting";
    else if(m_status==Status::Playing)m_message=m_port->cameIn?"":"PRESS START TO JOIN";
    if(m_message!=m_lastFlowMessage) {
        m_lastFlowMessage=m_message;
        fprintf(stderr,"online: match flow: %s\n",m_message.empty()?"Playing":m_message.c_str());
    }
}

int NeoGeoOnline::Tick(const uint16_t local[2],bool advanceWaiting) {
    const uint64_t inputEpoch=Epoch();
    m_ticks++;
    m_shown = 0;
    // (after the tick, whatever it did, the chat's: lines heard, the tests' lines, the log, the end)
    struct ChatAfter {
        NeoGeoOnline *online;
        ~ChatAfter() {
            if (online) {
                online->m_chatScreen.Tick(*online, online->m_port->board->Frames());
            }
        }
    } chatAfter{m_options.mode == OnlineOptions::Mode::Stress ? nullptr : this};
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
    if (ChatHost() && m_ticks % 15 == 0) {
        // (match-spectators) the host's status, to the other player and the spectators
        SendStatus(m_status == Status::Ended ? 4 : Recovering() ? 3 : !m_heard ? 0 : m_status == Status::Playing ? 2 : 1);
    }
    UpdateFlowMessage();
    if (Spectating()) {
        // (match-spectators) a spectator: no input of its own; the confirmed frames as they come, faster when behind
        if (!m_heard) {
            if (m_ticks % 15 == 1) {
                SendHello();
            }
            return 0;
        }
        if (m_ticks % 60 == 0) {
            SendHello(); // (the host keeps a spectator it hears, while the match does not run for it too)
        }
        if (m_socket->statusMs && NowMs() - m_socket->statusMs > kTimeoutMs) {
            End("The connection to the match was lost", false);
            return 0;
        }
        PumpChat();
        if (!m_session) {
            return 0;
        }
        gekko_network_poll(m_session);
        Run();
        for (int more = 0; more < 2 && m_status == Status::Playing &&
                           gekko_spectator_buffered_frames(m_session) > std::clamp(m_options.spectatorDelay, 1, 300) + 8; ++more) {
            Run();
        }
        return m_shown;
    }
    if (!m_heard) {
        if(ArcadeWaiting()&&advanceWaiting){uint16_t pads[2]={mine,0};m_port->RunFrame(pads);return 1;}
        if (!m_socket->host && m_ticks % 15 == 1) {
            SendHello(); // (four times a second until the host answers)
        }
        return 0;
    }
    if(m_options.matchFlow) {
        if(m_everPlayed && !m_recovery.Waiting() && NowMs()-m_socket->lastReceived>=kTimeoutMs)RequestRecovery();
        if(m_status==Status::Recovering)PrepareRecovery();
        ApplyRecovery(m_recovery.Tick(NowMs()));
        if(m_status==Status::Ended || m_recovery.Waiting() || Epoch()!=inputEpoch)return 0;
    }
    gekko_network_poll(m_session);
    PumpChat(); // (the chat's lines not acknowledged yet, again)
    gekko_add_local_input(m_session, m_local, &mine);
    Run();
    if(m_status==Status::Recovering)PrepareRecovery();
    return m_shown;
}

void NeoGeoOnline::Run() {
    if (!m_session) {
        return;
    }
    Events();
    if (m_status == Status::Ended || m_status==Status::Recovering) {
        return;
    }
    int count = 0;
    GekkoGameEvent **events = gekko_update_session(m_session, &count);
    NeoGeo &board = *m_port->board;
    for (int i = 0; i < count && m_status != Status::Ended && m_status!=Status::Recovering; i++) {
        GekkoGameEvent *event = events[i];
        switch (event->type) {
        case GekkoSaveEvent: {
            if(m_options.matchFlow&&!Spectating()){m_flow.Save(event->data.save.frame);if(m_flowLog){const auto& s=m_flow.state;fprintf(m_flowLog,"%d %d %d %d %d %d %d %d %d %d %d %d %d\n",event->data.save.frame,int(s.joined[0]),int(s.joined[1]),s.streak[0],s.streak[1],s.rounds[0],s.rounds[1],int(s.counted),s.quiet,s.score[0],s.score[1],int(s.scoreKnown),int(s.scoreArmed));}}
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
            if(m_checkpointLog)fprintf(m_checkpointLog,"%d %08x\n",event->data.save.frame,*event->data.save.checksum);
            break;
        }
        case GekkoLoadEvent: {
            if(m_options.matchFlow&&!Spectating()){if(!m_flow.Load(event->data.load.frame)){End("The match presentation checkpoint is unavailable",true);break;}PublishFlow();}
            std::vector<uint8_t> data(event->data.load.state, event->data.load.state + event->data.load.state_len);
            Archive ar(data, false);
            board.Serialize(ar);
            if (!ar.Ok()) {
                End("A state the session kept did not load", true);
            }
            if (Spectating()) {
                // (match-spectators) the match's state as the host confirmed it, its frame count with it
                fprintf(stderr, "online: watching from frame %llu (%u bytes)\n", static_cast<unsigned long long>(board.Frames()),
                        event->data.load.state_len);
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
            if(m_options.matchFlow&&!Spectating()&&!event->data.adv.running_ahead){
                AdvanceNeoGeoFlow(m_flow,board,players);
                PublishFlow();
            }
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
    for (int i = 0; i < count && m_status != Status::Ended && m_status!=Status::Recovering; i++) {
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
            if(m_options.matchFlow&&!Spectating()){const bool resumed=Recovering();m_recovery.Complete();m_prepared=false;m_everPlayed=true;if(resumed){m_challengerUntil=NowMs()+1200;fprintf(stderr,"online: recovery playing epoch %llu\n",static_cast<unsigned long long>(Epoch()));}}
            m_message.clear();
            AddSpectators();
            break;
        case GekkoPlayerDisconnected:
            // (match-spectators) a spectator leaving is not the other player leaving
            if (ChatHost() && event->data.disconnected.handle != m_remote) {
                SpectatorLeft(event->data.disconnected.handle);
                break;
            }
            if (Spectating()) {
                if (m_hostPhase != 3) {
                    End("The match ended or the connection was lost", false);
                }
                break;
            }
            RequestRecovery();
            break;
        case GekkoSpectatorPaused:
            fprintf(stderr, "online: watching paused, waiting for the match\n");
            break;
        case GekkoSpectatorUnpaused:
            fprintf(stderr, "online: watching again\n");
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
        // (match-spectators) WATCHING on a spectator's screen; how many watch on the players'
        const int watching = ChatHost() ? Watching() : m_seenWatching;
        if (Spectating()) {
            snprintf(text, sizeof(text), "WATCHING   PING %u", static_cast<unsigned>(net.last_ping));
        } else if (watching > 0) {
            snprintf(text, sizeof(text), "PING %u  DELAY %d  ROLLBACK %u   %d WATCHING", static_cast<unsigned>(net.last_ping),
                     m_options.delay, rollbacks.last_frames, watching);
        } else {
            snprintf(text, sizeof(text), "PING %u  DELAY %d  ROLLBACK %u", static_cast<unsigned>(net.last_ping), m_options.delay,
                     rollbacks.last_frames);
        }
    }
    return text;
}

void NeoGeoOnline::StatRows(std::vector<std::string> &right, std::string &left) const {
    right.clear();
    left.clear();
    if (!m_session || m_status != Status::Playing) {
        return;
    }
    if (m_options.mode == OnlineOptions::Mode::Stress) {
        right.push_back(Stats());
        return;
    }
    GekkoRollbackStats rollbacks{};
    gekko_rollback_stats(m_session, &rollbacks);
    GekkoNetworkStats net{};
    gekko_network_stats(m_session, m_remote, &net);
    right.push_back("PING " + std::to_string(net.last_ping));
    if (Spectating()) {
        left = "WATCHING";
        return;
    }
    right.push_back("DELAY " + std::to_string(m_options.delay));
    right.push_back("ROLLBACK " + std::to_string(rollbacks.last_frames));
    const int watching = ChatHost() ? Watching() : m_seenWatching;
    if (watching > 0) {
        left = std::to_string(watching) + " WATCHING";
    }
}

// ForkAde's in-game chat over this session
recomp::chat::Channel::Said NeoGeoOnline::ChatSay(const std::string &text, std::string &line) {
    if(m_options.matchFlow&&Recovering())return recomp::chat::Channel::Said::Wait;
    const auto said = m_chat ? m_chat->Say(text, NowMs(), &line) : recomp::chat::Channel::Said::Empty;
    if (said == recomp::chat::Channel::Said::Sent && ChatHost()) {
        RelayLine(0, ChatName(0), line, ""); // (match-spectators: the host's line to the spectators too)
    }
    return said;
}

std::vector<std::string> NeoGeoOnline::ChatHeard() {
    std::vector<std::string> lines;
    for (Heard &one : ChatHeardLines()) {
        lines.push_back(std::move(one.text));
    }
    return lines;
}

// (match-spectators) every line heard, with who said it; the host relays each to whoever did not say it
std::vector<NeoGeoOnline::Heard> NeoGeoOnline::ChatHeardLines() {
    std::vector<Heard> lines;
    if (m_chat) {
        for (std::string &text : m_chat->TakeHeard()) {
            if (ChatHost()) {
                RelayLine(1, ChatName(1), text, "");
            }
            lines.push_back({"", std::move(text), false});
        }
    }
    if (ChatHost()) {
        for (Spectator &one : m_spectators) {
            for (std::string &text : one.own->TakeHeard()) {
                const std::string name = one.nick.empty() ? "Spectator" : one.nick;
                RelayLine(2, name, text, one.name);
                lines.push_back({name, std::move(text), true});
            }
        }
    } else if (m_relay) {
        for (ChatRelay::Line &one : m_relay->Take()) {
            lines.push_back({one.name.empty() ? (one.role == 2 ? "Spectator" : one.role == 0 ? "Player 1" : "Player 2") : one.name,
                             std::move(one.text), one.role == 2});
        }
    }
    return lines;
}

void NeoGeoOnline::RelayLine(int role, const std::string &name, const std::string &text, const std::string &except) {
    const uint64_t now = NowMs();
    if (role == 2 && m_relay && m_heard) {
        m_relay->Push({role, name, text}, now);
    }
    for (Spectator &one : m_spectators) {
        if (one.name != except && one.relay) {
            one.relay->Push({role, name, text}, now);
        }
    }
}

void NeoGeoOnline::PumpChat() {
    const uint64_t now = NowMs();
    if (m_chat) {
        m_chat->Pump(now);
    }
    if (m_relay) {
        m_relay->Pump(now);
    }
    for (Spectator &one : m_spectators) {
        one.own->Pump(now);
        one.relay->Pump(now);
    }
}

std::string NeoGeoOnline::ChatName(int player) const {
    return player == ChatLocal() ? recomp::chat::Printable(m_options.name, recomp::chat::kNameMost) : m_theirName;
}

int NeoGeoOnline::ChatLocal() const {
    return m_options.mode == OnlineOptions::Mode::Join ? 1 : 0;
}

bool NeoGeoOnline::ChatHost() const {
    return m_options.mode == OnlineOptions::Mode::Host;
}

bool NeoGeoOnline::ChatReady() const {
    return m_chat && m_heard && m_status == Status::Playing;
}

bool NeoGeoOnline::ChatOver() const {
    return m_status == Status::Ended;
}

std::vector<std::string> NeoGeoOnline::ChatUndelivered() const {
    return m_chat ? m_chat->Unacknowledged() : std::vector<std::string>{};
}

float NeoGeoOnline::FramesAhead() const {
    return m_session && m_status == Status::Playing ? gekko_frames_ahead(m_session) : 0.0f;
}

} // namespace port
