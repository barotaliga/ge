// QUAKE II'S SESSION SETTINGS: what its own cvars do round the game being played, while it is the
// game loaded (client/cvar_layers.cpp registers them in its layer; another game's window keeps the
// defaults below and never runs the commands).
//
// THE MAP COMMANDS: `cl_beginmapcmd` runs on entering each map, `cl_changemapcmd` as a map is left
// for the next, `cl_disconnectcmd` on leaving the game (each empty by default: nothing run). They
// are how a player records every map (`cl_beginmapcmd "record demo"`). A game is joined while a
// server is joined or this window hosts one; its map is the one the server names.
//
// THE CLOCK: `timescale` speeds or slows a game this window runs (2 twice as fast) and `fixedtime`
// moves it a fixed number of milliseconds each frame; both are cheats (only while sv_cheats is 1),
// and `host_framerate` (seconds a frame) goes first when set.
//
// `cl_autopause` 1 (the default, as every other game does) holds a game played alone while the
// console or a menu is up; 0 lets it run on.
//
// THE STATE, read-only: `cl_running` 1 (the client is up), `cl_paused` 1 while the game this
// window runs is paused, `sv_running` 1 while it runs a server, `sv_paused` 1 while that is paused.
//
// ge:summary Quake II's session settings: the map commands, the game clock, the state cvars
// ge:keywords q2 cl_beginmapcmd cl_changemapcmd cl_disconnectcmd auto-record map-change timescale
// ge:keywords fixedtime cl_running cl_paused sv_running sv_paused read-only q2_session_step
// ge:keywords cl_autopause
// ge:skill console shooter
// ge:test tests/shooters_edge_q2_session_test.cpp
#pragma once

#include <string>
#include <string_view>

#include "console/console.hpp"

namespace shooters_edge::net {
struct Remote;
}  // namespace shooters_edge::net

namespace shooters_edge::client {

struct Listen;

struct Q2Session {
  std::string begin_cmd;       // cl_beginmapcmd
  std::string change_cmd;      // cl_changemapcmd
  std::string disconnect_cmd;  // cl_disconnectcmd
  double timescale = 1;        // timescale: the game's speed
  double fixed_ms = 0;         // fixedtime: milliseconds a frame, 0 the real time
  bool autopause = true;       // cl_autopause: a game played alone waits under the menu
  // What the last frame saw: in a game, its map ("" while it loads), the state cvars' values.
  bool joined = false;
  std::string map;
  int state = -1;  // the read-only cvars' bits as last set (-1: never)
};

// Quake II's session cvars on `v`, bound to `s` (the read-only ones added, set by the frames).
void add_q2_session_cvars(ge::console::Cvars& v, Q2Session& s);

// The game's step for one frame of `wall` seconds: host_framerate's seconds when set, else
// fixedtime's milliseconds when set, else `wall`; times timescale.
double q2_game_dt(const Q2Session& s, double host_framerate, double wall);

// One frame: the map commands run on what changed since the last (left the game: the disconnect
// command; another map: the change command; a map entered: the begin command), and the state cvars
// set where they are registered. `map` is "" while one loads.
void q2_session_step(ge::console::Console& con, Q2Session& s, bool joined, std::string_view map,
                     bool hosting, bool paused);
// The same off the window's server joined and the game it hosts (`paused`: held this frame, as
// by cl_autopause, or its server paused).
void q2_session_frame(ge::console::Console& con, Q2Session& s, const net::Remote& remote,
                      const Listen& listen, bool paused);

}  // namespace shooters_edge::client
