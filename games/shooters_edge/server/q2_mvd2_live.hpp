// QUAKE II'S MULTIVIEW RECORDING FROM A LIVE SERVER: our rules' world each frame as the frames the
// multiview writer takes (q2_mvd2.hpp), for the server's own recorder and its relay stream
// (net/q2_mvd_server.hpp; porting/shooters-edge/q2-mvd-relay.md § 6).
//
// A LEVEL'S START: every settings string, the whole first frame, the observer (the recorder's
// dummy spectator's slot, -1 none) and the stream's flags (`kMvfNoMsgs` when a dummy holds the
// messages). A FRAME: the settings strings that changed, the players captured, every entity, the
// frame's prints to everyone, what went to one captured player alone (a print, a centred print, a
// layout; a stuffed line only when it plays a sound), the effects as protocol 34's messages to
// everyone and the sounds by entity. With `no_blend` the players' screen blend is left out, with
// `no_gun` the held weapon's model and frame. Nothing for one player of 2048 bytes or more goes in.
//
// WHO IS CAPTURED (`flags`, sv_mvd_capture_flags, 5 the default), in order: a slot not in the
// game, no; with bit 1 a bot (a slot with no connection), no; a field of view of 0, no; the dummy,
// yes; a spectator, no; a player drawn as nothing (no model, effects, sound or event) with bit 2,
// no; one drawn with bit 4, yes; frozen or not predicted (chasing someone), no; else yes.
// WITH A DUMMY a layout goes in only for it, and with `no_msgs` (sv_mvd_nomsgs) a print to one
// player only for it; what the game stuffs to the dummy is kept for its commands when
// `dummy_stuff` (sv_mvd_allow_stufftext above 0).
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
  int flags = 5;             // sv_mvd_capture_flags
  int dummy = -1;            // the recorder's dummy spectator's slot (-1: none)
  bool no_msgs = true;       // sv_mvd_nomsgs
  bool dummy_stuff = false;  // the game's stuffed lines to the dummy kept
  // The slots' move states (null: taken from the world's rows), by slot.
  const ge::classic::Q2State* moves[32] = {};
};

// What a live recording keeps between frames.
struct Mvd2Live {
  std::vector<std::string> sent;         // the settings strings written
  int64_t events_done = 0;               // the sink's events already taken
  std::string layouts[32];               // each captured player's layout written
  bool dummy_layout = false;             // the dummy's scoreboard was up or written this frame
  std::vector<std::string> dummy_lines;  // the lines the game stuffed to the dummy, to run
};

// A level's start from the rules (and the live state set to it).
void q2_mvd2_level(const RulesCommon& c, const Q2WireOptions& opt, const Mvd2Capture& cap,
                   Mvd2Live& live, Mvd2Level& out);
// The next frame from the rules.
void q2_mvd2_frame(const RulesCommon& c, const Q2WireOptions& opt, const Mvd2Capture& cap,
                   Mvd2Live& live, Mvd2Frame& out);

}  // namespace shooters_edge::server
