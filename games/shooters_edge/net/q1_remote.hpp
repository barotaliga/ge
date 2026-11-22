// THE WINDOW JOINED TO A QUAKE SERVER OF THE SINGLE-PLAYER PROTOCOL (version 15, a "NetQuake"
// server): `connect <host>:26000` in the console, the port Quake's own servers listen on. The
// transport and its signon are porting/shooters-edge/netgame-transport.md's, the same facts the
// scripted watcher (q1_netgame_watcher.hpp) is written from; this end is driven by the window's
// hands and reads every message straight into the window's drawn recording (`Remote::demo`, a
// single-player recording's state), so the window draws the game it plays as it draws a Quake demo.
//
// THE CONNECT: the control packet ("QUAKE", net version 3) up to three times, 2.5 s apart; a
// reject is said with its reason, silence as "No Response". The accept names the port the rest of
// the game runs on (often the same one); only datagrams from there are read after it.
// THE SIGNON: each `signonnum` answered as Quake's client does: 1 "prespawn"; 2 the player's name
// (`name "<name>"`), colours (`color <top> <bottom>`) and "spawn "; 3 "begin". A stuffed
// "reconnect" (the level changing) starts it again on the same connection.
// PLAYING: a `move` a tick once the first entity update has come (the first two after each
// level's start kept back, as Quake's client does): the server's last time, the view's three
// angles as one byte each, the forward, side and up speeds, the buttons (1 attack, 2 jump) and the
// impulse, sent once. The server's `setangle` turns the view. A line for the server goes as a
// string command: the word, a space, then the rest as typed (a word alone ends in a newline).
// LEAVING: the disconnect byte once, unreliably, and the connection closed.
//
// ge:summary The window joined to a Quake server of protocol 15: connect, signon, moves, leaving
// ge:keywords netquake protocol-15 single-player-protocol remote-client connect join-server
// ge:keywords nq_remote_start nq_remote_step signonnum prespawn spawn begin reconnect clc-move
// ge:keywords stringcmd disconnect-byte no-response port-26000
// ge:skill net shooter
// ge:test tests/shooters_edge_q1_remote_test.cpp
#pragma once

#include <string>
#include <vector>

#include "shooters_edge/net/qw_remote.hpp"

namespace shooters_edge::net {

// Joins `r.server` over `r.transport` as Quake's own client would, with the player's name and
// colours from `cfg`.
void nq_remote_start(Remote& r, const ClientConfig& cfg);
// One window frame of `wall_s` s (qw_remote.hpp's `remote_step` for such a server).
RemoteStep nq_remote_step(Remote& r, double wall_s, std::vector<std::string>& said);

}  // namespace shooters_edge::net
