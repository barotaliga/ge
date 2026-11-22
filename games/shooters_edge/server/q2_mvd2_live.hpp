// QUAKE II'S MULTIVIEW RECORDING FROM A LIVE SERVER: our rules' world each frame as the frames the
// multiview writer takes (q2_mvd2.hpp), for the server's own recorder and its relay stream
// (net/q2_mvd_server.hpp; porting/shooters-edge/q2-mvd-relay.md § 6).
//
// A LEVEL'S START: every settings string, the whole first frame, no observer (-1: our rules keep
// no recorder's dummy). A FRAME: the settings strings that changed, the players captured (a client
// in the game and not a spectator), every entity, the frame's prints to everyone, what went to one
// captured player alone (a print, a centred print, a layout; a stuffed line only when it plays a
// sound), the effects as protocol 34's messages to everyone and the sounds by entity. With
// `no_blend` the players' screen blend is left out, with `no_gun` the held weapon's model and
// frame. Nothing for one player of 2048 bytes or more goes in.
//
// ge:summary Quake II's multiview frames built from a live server's rules, each 10 Hz frame
// ge:keywords mvd2 live-recorder multiview capture unicast multicast sv_mvd_noblend sv_mvd_nogun
// ge:skill shooter replay net
// ge:test tests/shooters_edge_q2_mvd_server_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "classic/q2_move.hpp"
#include "shooters_edge/server/q2_mvd2.hpp"
#include "shooters_edge/server/rules_runner.hpp"

namespace shooters_edge::server {

struct Mvd2Capture {
  bool no_blend = false, no_gun = false;
  // The slots' move states (null: taken from the world's rows), by slot.
  const ge::classic::Q2State* moves[32] = {};
};

// What a live recording keeps between frames.
struct Mvd2Live {
  std::vector<std::string> sent;  // the settings strings written
  int64_t events_done = 0;        // the sink's events already taken
  std::string layouts[32];        // each captured player's layout written
};

// A level's start from the rules (and the live state set to it).
void q2_mvd2_level(const RulesCommon& c, const Q2WireOptions& opt, const Mvd2Capture& cap,
                   Mvd2Live& live, Mvd2Level& out);
// The next frame from the rules.
void q2_mvd2_frame(const RulesCommon& c, const Q2WireOptions& opt, const Mvd2Capture& cap,
                   Mvd2Live& live, Mvd2Frame& out);

}  // namespace shooters_edge::server
