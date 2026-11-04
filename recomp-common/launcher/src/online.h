// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of recomp-common: https://github.com/tmcleroy/recomp-common
#pragma once
// Online play (launcher::Online): the addresses another player can reach this machine at, for the Host row, and
// the lobby's list of open matches, and the waiting room's players and invitations.

#include <string>
#include <vector>

namespace launcher {

struct LocalAddress {
    std::string address; // "203.0.113.5"
    std::string kind;    // "home network", "Tailscale", "VPN" or "" (a public address)
};

// This machine's IPv4 addresses (not the loopback), those on a home network first
std::vector<LocalAddress> local_addresses();

// A match waiting in the lobby (launcher::Online::lobby): its code, its host's name, and how long it has waited
struct OpenMatch {
    std::string game;  // (a collection's lobby: which game's, by id)
    std::string title; // (... and its name, for a game the collection hasn't)
    std::string version; // (the host's version: joining needs the same)
    std::string code;
    std::string name;
    int waiting = 0; // (seconds)
};
// The lobby's list, fetched from `url` with the system's curl (blocks: for another thread); false if there was no
// answer
bool fetch_lobby(const std::string& url, std::vector<OpenMatch>& matches);

// Which game a match's code is for (Collection::code_lookup): the HTTP status (0: no answer), and from a 200 the
// game's id and name, or from another the service's error
struct CodeLookup {
    int status = 0;
    std::string game, title, version, error;
};
CodeLookup lookup_code(const std::string& url);

// The waiting room (Collection::room): the other players whose Online page is open, each to invite to a match, and
// the invitations waiting for this player. A check-in says who this player is (their secret, from which the service
// makes their id), their name, the games they have, and whether others may invite them.
struct RoomPlayer {
    std::string id;   // (12 hex digits)
    std::string name;
    std::vector<std::string> games; // (by id: those they have a game file for)
};
struct Invitation {
    std::string code; // (the match's: joining it is accepting)
    std::string from; // (the host's id: for a block)
    std::string name; // (the host's)
    std::string game, title, version;
};
struct RoomCheckIn {
    std::string secret, name;
    std::vector<std::string> games;
    int protocol = 0;
    std::string version;
    bool invitable = true;
    std::vector<std::string> blocked; // (ids)
    bool lobby = false;               // (every game's open matches in the answer too: Room::matches)
};
struct Room {
    bool ok = false; // (an answer came)
    std::string id;  // this player's
    std::vector<RoomPlayer> players;
    std::vector<Invitation> invitations;
    bool listed = false; // (the lobby's list came with it: asked for (RoomCheckIn::lobby), and from a service that says it)
    std::vector<OpenMatch> matches;
};
// A check-in at `url` (blocks: for another thread)
Room room_check_in(const std::string& url, const RoomCheckIn& me);
// Leaving the room (`url` + "/leave"), or declining an invitation (`url` + "/decline"), sent and not waited for
void room_leave(const std::string& url, const std::string& secret);
void room_decline(const std::string& url, const std::string& secret, const std::string& code);
// A new secret for the waiting room (32 hex digits), kept in launcher.ini
std::string new_room_secret();

}  // namespace launcher
