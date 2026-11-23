// QUAKE II'S SESSION SETTINGS: what its own cvars do round the game being played, while it is the
// game loaded (client/cvar_layers.cpp registers them in its layer; another game's window keeps the
// defaults below), and the map commands every classic game shares.
//
// THE MAP COMMANDS, in every game (the classic layer, `add_shared_session_cvars`; Quake II's client
// brought them): `cl_beginmapcmd` runs on entering each map, `cl_changemapcmd` as a map is left
// for the next, `cl_disconnectcmd` on leaving the game (each empty by default: nothing run). They
// are how a player records every map (`cl_beginmapcmd "record demo"`). A game is joined while a
// server is joined or this window hosts one; its map is the one the server names. They keep their
// text when another game is loaded.
//
// THE CLOCK: `timescale` speeds or slows a game this window runs (2 twice as fast) and `fixedtime`
// moves it a fixed number of milliseconds each frame; both are cheats (only while sv_cheats is 1),
// and `host_framerate` (seconds a frame) goes first when set.
//
// `cl_autopause` 1 (the default, as every other game does) holds a game played alone while the
// console or a menu is up; 0 lets it run on.
//
// THE MOVE RATE ROUNDED: moves go whole milliseconds apart, so a `cl_maxfps` that is not 1000 over
// a whole number of them (120: 8 ms, so 125) is rounded up; set in a game, the player is told
// ("cl_maxfps value 120 is inexact and will be rounded to 125.") unless `cl_warn_on_fps_rounding`
// is 0, and the first time how to turn that off. 0 (the most) never warns.
//
// THE STATE, read-only: `cl_running` 1 (the client is up), `cl_paused` 1 while the game this
// window runs is paused, `sv_running` 1 while it runs a server, `sv_paused` 1 while that is paused.
//
// THE THIRD PERSON, every game's and a cheat: `cl_thirdperson` 1 (Quake's `chase_active`) puts the
// player's own view `cam_dist` units (Quake II's 60, ours 100; its `cl_thirdperson_range`, Quake's
// `chase_back`, the distance a watched player is seen from too) behind his eye, turned
// `cl_thirdperson_angle` degrees round him the way a yaw turns (0 behind, 180 in front looking back
// at him), looking the way he would from there; it stops short of the world's walls and of its
// doors and platforms where they stand, and his own figure, drawn while the camera is off him,
// fades as a wall pushes the camera in (`own_figure_alpha`: solid from kOwnSolidUnits, gone within
// kOwnFigureUnits). Joined or hosted, never while he watches another.
//
// THE MAP'S VERSION: a level that states its map's checksum (configstring 31) is checked against
// the map on disk as it loads (`q2_map_version`): a server's different map drops the game, a
// recording's only warns and plays on.
//
// THE MOUSE: `in_grab`, every game's (the classic layer; QuakeWorld's `in_grab_windowed_mouse`),
// 0 never takes the mouse while the game is in a window (only covering the screen); 1 and 2 take
// it as ours does (`q2_takes_mouse`). Quake II's own `in_enable` 0 never takes it, so the mouse
// never turns the view; the keys work either way.
//
// THE CLIENT'S PORT: `net_clientport`, every game's (QuakeWorld's `cl_net_clientport`), the port
// the client sends from (-1 any free one; net/qw_remote.cpp opens the socket again when it moves).
//
// ge:summary Quake II's session settings: the map commands, the game clock, the state cvars
// ge:keywords q2 cl_beginmapcmd cl_changemapcmd cl_disconnectcmd auto-record map-change timescale
// ge:keywords fixedtime cl_running cl_paused sv_running sv_paused read-only q2_session_step
// ge:keywords cl_warn_on_fps_rounding fps-rounding inexact map-version map-checksum
// ge:keywords cl_autopause cl_thirdperson cl_thirdperson_angle cl_thirdperson_range
// ge:keywords third-person chase-camera view_behind in_grab in_enable q2_takes_mouse
// ge:skill console shooter
// ge:test tests/shooters_edge_q2_session_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>

#include "console/console.hpp"
#include "render/view.hpp"
#include "shooter/brush_hull.hpp"

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
  bool third_person = false;   // cl_thirdperson (a cheat)
  double third_angle = 0;      // cl_thirdperson_angle, degrees round the player
  double third_range = 100;    // cam_dist, units behind him (Quake II 60), when `dist` is none
  const ge::console::Cvar* dist = nullptr;  // cam_dist, where the window registered it
  bool warn_fps = true;                     // cl_warn_on_fps_rounding
  uint32_t fps_seen = 0;                    // cl_maxfps's revision when it was last checked
  bool fps_told = false;                    // how to turn the warning off said once
  int grab = 1;                             // in_grab: 0 never in a window, 1 and 2 as ours
  bool mouse = true;                        // in_enable: 0 the mouse never taken, so it never looks
  bool remap_tracks = false;  // ogg_remap_tracks (1 once Quake II registers it): take_in.hpp
  // What the last frame saw: in a game, its map ("" while it loads), the state cvars' values.
  bool joined = false;
  std::string map;
  int state = -1;  // the read-only cvars' bits as last set (-1: never)
};

// The map commands, `in_grab` and `net_clientport` on `v`, bound to `s`'s, for every game (the
// caller holds the classic layer).
void add_shared_session_cvars(ge::console::Cvars& v, Q2Session& s);
// Quake II's session cvars on `v`, bound to `s` (the read-only ones added, set by the frames).
void add_q2_session_cvars(ge::console::Cvars& v, Q2Session& s);

// "Local map version differs from server: <ours> != <stated>" (`server`; "from demo" else) when
// the map file `file`'s checksum is not the level's `stated` one; "" when they match or it
// states none.
std::string q2_map_version(std::string_view stated, std::span<const uint8_t> file, bool server);

// The moves a second `cl_maxfps` `fps` gives (held to 10..125, whole milliseconds apart); 0: 0.
int q2_fps_rounded(int fps);
// The warning above, once per change of `cl_maxfps` while in a game (q2_session_step calls it).
void q2_fps_warning(ge::console::Console& con, Q2Session& s);

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

// Closer than this (units) the camera is in the player's own head: his figure is not drawn.
inline constexpr double kOwnFigureUnits = 20;
// From this far (units) his figure is drawn solid; nearer it fades out to kOwnFigureUnits.
inline constexpr double kOwnSolidUnits = 60;
// The share of the player's own figure drawn with the camera `behind` units back (0..1).
double own_figure_alpha(double behind);

// `view` moved `units` back from its eye, its heading turned `angle_deg` the way a map yaw turns
// (counter-clockwise from above) first; traced against `world`'s point hull (map units), stopping
// short of a wall. The distance it went back, units (0: `units` not above 0, or a wall at once).
// `movers` (the doors and platforms where they stand this frame, each its brush model and its
// offset, map units) stop it too.
struct BehindMover {
  const ge::shooter::BrushModel* model = nullptr;
  double offset[3] = {0, 0, 0};  // map units
};
double view_behind(ge::render::FrameView& view, double units, double angle_deg,
                   const ge::shooter::BrushModel* world, std::span<const BehindMover> movers = {});
// cl_thirdperson on `view`: view_behind by the range and the angle while it is on; the distance
// the camera went back (0: off).
double q2_third_person(const Q2Session& s, ge::render::FrameView& view,
                       const ge::shooter::BrushModel* world,
                       std::span<const BehindMover> movers = {});

// Whether the look may take the mouse under `in_grab` (never in a window at 0) and `in_enable`
// (never at 0).
inline bool q2_takes_mouse(const Q2Session& s, bool fullscreen) {
  return s.mouse && (s.grab != 0 || fullscreen);
}

}  // namespace shooters_edge::client
