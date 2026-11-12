// THE ROCKET JUMP HELPER ON A SERVER: who may use `+rjump` and `+rrjump` (shooter/rjump_assist.hpp)
// and the packets it rewrites, for every runner of the game (the classic rules in QC and the
// sequel's own), so the helper works alike in both games. Each runner says only whether its
// launcher can fire (`RjumpReadyFn`).
//
// THE SETTINGS (`add_rjump_settings`, told in the server info): `sv_rjump` allows the rocket
// straight down, `sv_rrjump` the reverse rocket jump, each 0 or 1, both 0 by default: a dedicated
// server offers them only when its server.cfg, an administrator or a vote turns them on (they are
// rules a vote may change for the map). The window's own server turns both on.
//
// THE GRANT: the server-only key `*rjump 1` in one player's user info lets him use both helpers
// where the settings say no (net/classic_admin.hpp's `rjump <id|name> on|off`, and `vote rjump
// <id|name> on|off`), as the easy jump's grant does; everyone sees it and is told.
//
// THE PLAYER IS TOLD, in a game's words: "The rocket jump helper is off on this server." when it
// is not allowed, and the runner's reason when it can never fire ("You have no rocket launcher.",
// "You have no rockets."). A request that waits and runs out is dropped quietly.
//
// THE PACKET: a request is read from the packet's newest command, the one the rules fire on; a
// waiting request rewrites the newest command of each packet until it fires or ends.
//
// ge:summary The rocket jump helper on a server: sv_rjump, sv_rrjump, the *rjump grant, packets
// ge:keywords rocket-jump rjump rrjump sv_rjump sv_rrjump grant accessibility rjump_packet
// ge:keywords add_rjump_settings rjump_allowed rocket-ready
// ge:skill shooter
// ge:test tests/shooters_edge_rjump_test.cpp
#pragma once

#include <string>

#include "shooter/rjump_assist.hpp"
#include "shooters_edge/server/rules_runner.hpp"

namespace shooters_edge::server {

inline constexpr const char* kRjumpGrantKey = "*rjump";  // the grant's user info key

// The setting that allows `kind` ("sv_rjump", "sv_rrjump").
const char* rjump_setting(sh::Rjump kind);
// `sv_rjump` and `sv_rrjump` registered in a runner's settings (0, told in the server info).
void add_rjump_settings(ge::console::Cvars& cvars);
// Whether `slot` may use `kind`: its setting on, or the player granted the helpers.
bool rjump_allowed(const RulesCommon& s, int slot, sh::Rjump kind);

// What a runner says of `slot`'s launcher for a waiting request; it may add to `cmd` what the shot
// needs (the launcher chosen), and with `never` it gives the player's reason in `why` ("" says
// nothing).
using RjumpReadyFn = sh::RjumpReady (*)(void* rules, int slot, sh::MoveCmd& cmd, std::string& why);

// `p`'s newest command through the helper: a request taken (refused with a line to the player
// when he may not), a waiting one fired, held back or ended by `ready`. `now` stamps the line.
// True when this packet carries the shot.
bool rjump_packet(RulesCommon& s, sh::Packet& p, void* rules, RjumpReadyFn ready, double now);

}  // namespace shooters_edge::server
