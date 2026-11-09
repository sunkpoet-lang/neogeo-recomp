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
    // Every packet starts with these three bytes and a kind: H, hello (before GekkoNet's own); G, GekkoNet's
    constexpr char kTag[3] = {'N', 'G', 'O'};

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
            if(!havePeer || !Same(from,peer))continue;
            if(recovery && std::strchr("NRKSTPL",kind) && got==20) {
                if(controls.size()<64)controls.emplace_back(kind,std::vector<uint8_t>(buffer+4,buffer+got));
                continue;
            }
            if(kind!='G' && kind!='C' && kind!='A')continue;
            const uint8_t* payload=buffer+4;size_t size=static_cast<size_t>(got-4);
            if(recovery) {
                if(size<16 || !room || Recovery::Get(payload)!=room)continue;
                if(Recovery::Get(payload+8)!=epoch){++stalePackets;continue;}
                payload+=16;size-=16;
            }
            lastReceived=NowMs();
            if(kind=='G')forGekko.emplace_back(payload,payload+size);
            else forChat.emplace_back(kind,std::vector<uint8_t>(payload,payload+size));
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
    m_chat = std::make_unique<recomp::chat::Channel>([socket, loss = options.chatLoss, seed = uint32_t{12345}](
                                                         char kind, const std::vector<uint8_t> &payload) mutable {
        seed = seed * 1103515245u + 12345u; // (--chat-loss: dropped at random, as a bad network would)
        if (socket->havePeer && static_cast<int>((seed >> 16) % 100) >= loss) {
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
    config.num_players=2;config.max_spectators=0;config.max_input_prediction_window=kPredictionWindow;
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
    m_socket->forGekko.clear();m_socket->forChat.clear();
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
    m_socket->forGekko.clear();m_socket->forChat.clear();m_socket->epoch=m_recovery.Epoch();
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

void NeoGeoOnline::SendHello() {
    if (m_socket->havePeer) {
        // (this fork: with this player's name after a 0 byte)
        auto base=m_hello;
        if(m_options.matchFlow) {
            char nonce[18];std::snprintf(nonce,sizeof nonce,":%016llx",static_cast<unsigned long long>(m_localNonce));
            base.insert(base.end(),nonce,nonce+17);
        }
        const std::vector<uint8_t> hello = recomp::chat::HelloWithName(base, m_options.name);
        m_socket->Send('H', hello.data(), hello.size(), m_socket->peer);
    }
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
            continue; // (someone else: not the player this session is with)
        }
        if (m_socket->host) {
            SendHello(); // (each hello answered: the first answer may be lost)
        }
        if (m_heard) {
            continue;
        }
        uint64_t peerNonce=0;
        const bool recoveryCompatible=!m_options.matchFlow||RecoveryHelloNonce(said,peerNonce);
        if (said != m_hello || !recoveryCompatible) {
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
    }
    m_socket->hellos.clear();
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
    if(!m_options.matchFlow || m_status==Status::Ended)return;
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
    UpdateFlowMessage();
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
    if (m_chat) {
        m_chat->Pump(NowMs()); // (the chat's lines not acknowledged yet, again)
    }
    gekko_add_local_input(m_session, m_local, &mine);
    Run();
    if(m_status==Status::Recovering)PrepareRecovery();
    return m_shown;
}

void NeoGeoOnline::Run() {
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
            if(m_options.matchFlow){m_flow.Save(event->data.save.frame);if(m_flowLog){const auto& s=m_flow.state;fprintf(m_flowLog,"%d %d %d %d %d %d %d %d %d %d %d %d %d\n",event->data.save.frame,int(s.joined[0]),int(s.joined[1]),s.streak[0],s.streak[1],s.rounds[0],s.rounds[1],int(s.counted),s.quiet,s.score[0],s.score[1],int(s.scoreKnown),int(s.scoreArmed));}}
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
            if(m_options.matchFlow){if(!m_flow.Load(event->data.load.frame)){End("The match presentation checkpoint is unavailable",true);break;}PublishFlow();}
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
            if(m_options.matchFlow&&!event->data.adv.running_ahead){
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
            if(m_options.matchFlow){const bool resumed=Recovering();m_recovery.Complete();m_prepared=false;m_everPlayed=true;if(resumed){m_challengerUntil=NowMs()+1200;fprintf(stderr,"online: recovery playing epoch %llu\n",static_cast<unsigned long long>(Epoch()));}}
            m_message.clear();
            break;
        case GekkoPlayerDisconnected:
            RequestRecovery();
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

// ForkAde's in-game chat over this session
recomp::chat::Channel::Said NeoGeoOnline::ChatSay(const std::string &text, std::string &line) {
    if(m_options.matchFlow&&Recovering())return recomp::chat::Channel::Said::Wait;
    return m_chat ? m_chat->Say(text, NowMs(), &line) : recomp::chat::Channel::Said::Empty;
}

std::vector<std::string> NeoGeoOnline::ChatHeard() {
    return m_chat ? m_chat->TakeHeard() : std::vector<std::string>{};
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
