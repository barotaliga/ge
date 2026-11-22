// Quake II's session settings: the map commands, the clock and the state (q2_session.hpp).
//
// ge:summary Quake II's map commands run on a change, its game clock, its read-only state cvars
// ge:keywords cl_thirdperson cl_thirdperson_angle cl_thirdperson_range third-person view_behind
// ge:keywords q2 cl_beginmapcmd cl_changemapcmd cl_disconnectcmd timescale fixedtime cl_paused
// ge:skill console shooter
// ge:test tests/shooters_edge_q2_session_test.cpp
#include "shooters_edge/client/q2_session.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <utility>

#include "brush/map.hpp"
#include "shooter/hull.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/net/qw_remote.hpp"
#include "shooters_edge/server/q2_wire.hpp"

namespace shooters_edge::client {

namespace con = ge::console;

namespace {

constexpr double kDeg = 3.14159265358979323846 / 180;

constexpr const char* kStateNames[4] = {"cl_running", "cl_paused", "sv_running", "sv_paused"};

void run(con::Console& c, const std::string& text) {
  if (text.find_first_not_of(" \t") != std::string::npos) c.append(text, con::Source::local);
}

}  // namespace

void add_map_command_cvars(con::Cvars& v, Q2Session& s) {
  const struct {
    const char* name;
    const char* help;
    const char* values;
    std::string* field;
  } commands[] = {
      {"cl_beginmapcmd", "Commands run each time you enter a map, such as record.",
       "\"\": none (the default); TEXT: the commands, ; between them", &s.begin_cmd},
      {"cl_changemapcmd", "Commands run each time the game leaves a map for the next.",
       "\"\": none (the default); TEXT: the commands, ; between them", &s.change_cmd},
      {"cl_disconnectcmd", "Commands run each time you leave a game.",
       "\"\": none (the default); TEXT: the commands, ; between them", &s.disconnect_cmd},
  };
  for (const auto& r : commands)
    v.bind({.name = r.name,
            .fallback = "",
            .flags = con::kArchive,
            .help = r.help,
            .kind = con::CvarKind::text,
            .values = r.values},
           r.field);
}

void add_q2_session_cvars(con::Cvars& v, Q2Session& s) {
  v.bind({.name = "timescale",
          .fallback = "1",
          .flags = con::kCheat,
          .help = "How fast a game you host runs.",
          .min = 0,
          .max = 1000,
          .values = "1: as it should (the default); 0..1000: that many times as fast, only while "
                    "the server allows cheats"},
         &s.timescale);
  v.bind({.name = "fixedtime",
          .fallback = "0",
          .flags = con::kCheat,
          .help = "How far a game you host moves each frame, in milliseconds.",
          .min = 0,
          .max = 1000,
          .values = "0: as far as the time that passed (the default); 1..1000: that many "
                    "milliseconds a frame, only while the server allows cheats"},
         &s.fixed_ms);
  v.bind({.name = "cl_autopause",
          .fallback = "1",
          .flags = con::kArchive,
          .help = "Whether a game you play alone waits while the console or a menu is up.",
          .kind = con::CvarKind::boolean,
          .values = "1: it waits (the default); 0: it runs on"},
         &s.autopause);
  v.bind({.name = "cl_thirdperson",
          .fallback = "0",
          .flags = con::kCheat,
          .help = "Whether you see yourself from behind, only while the server allows cheats.",
          .kind = con::CvarKind::boolean,
          .values = "0: through your own eyes (the default); 1: from behind you"},
         &s.third_person);
  v.bind({.name = "cl_thirdperson_angle",
          .fallback = "0",
          .flags = con::kArchive,
          .help = "Where round you the camera sits with cl_thirdperson 1, in degrees.",
          .min = -360,
          .max = 360,
          .values = "0: behind you (the default); 90: to your right; 180: in front, looking back "
                    "at you; -360..360: any angle between"},
         &s.third_angle);
  v.bind({.name = "cl_thirdperson_range",
          .fallback = "60",
          .flags = con::kArchive,
          .help = "How far from you the camera sits with cl_thirdperson 1.",
          .min = 0,
          .max = 1000,
          .values = "60: the default; 0..1000: that many units, nearer where a wall is in the way"},
         &s.third_range);
  v.bind({.name = "in_grab",
          .fallback = "1",
          .flags = con::kArchive,
          .help = "Whether the game takes the mouse while it is in a window.",
          .kind = con::CvarKind::integer,
          .min = 0,
          .max = 2,
          .values = "1: it does (the default), as 2; 0: only while it covers the screen"},
         &s.grab);
  v.bind(
      {.name = "ogg_remap_tracks",
       .fallback = "1",
       .flags = con::kArchive,
       .help = "Whether a mission pack's music tracks are found by the digital release's numbers.",
       .kind = con::CvarKind::boolean,
       .values = "1: by the digital release's numbers (the default); 0: by the disc's"},
      &s.remap_tracks);
  v.add(
      {.name = "cl_nodelta",
       .fallback = "0",
       .help = "Whether the server sends every frame whole, never as changes (read when you join).",
       .kind = con::CvarKind::boolean,
       .values = "0: changes (the default); 1: whole frames"});
  // What joining a server fetches (net/q2_downloads.hpp), read at each `connect`.
  v.add({.name = "allow_download",
         .fallback = "1",
         .flags = con::kArchive,
         .help = "Whether joining a server fetches the files you lack.",
         .kind = con::CvarKind::integer,
         .min = -1,
         .max = 1,
         .values = "1: it does (the default); 0: nothing fetched; -1: not even by download"});
  const struct {
    const char* name;
    const char* fallback;
    const char* help;
  } kinds[] = {
      {"allow_download_maps", "1", "Whether joining fetches a map you lack."},
      {"allow_download_players", "1", "Whether joining fetches the player models and skins."},
      {"allow_download_models", "1", "Whether joining fetches models and sprites."},
      {"allow_download_sounds", "1", "Whether joining fetches sounds."},
      {"allow_download_textures", "1", "Whether joining fetches wall textures and skies."},
      {"allow_download_pics", "1", "Whether joining fetches pictures."},
      {"allow_download_others", "0", "Whether joining fetches any other file."},
  };
  for (const auto& k : kinds)
    v.add({.name = k.name,
           .fallback = k.fallback,
           .flags = con::kArchive,
           .help = k.help,
           .kind = con::CvarKind::boolean,
           .values = k.fallback[0] == '1' ? "1: fetched (the default); 0: not"
                                          : "0: not fetched (the default); 1: fetched"});
  // Downloads over HTTP (net/q2_http.hpp), read at each `connect`; the server list's web lists
  // read `cl_http_blocking_timeout` (menu/servers_page.hpp).
  v.add({.name = "cl_http_downloads",
         .fallback = "1",
         .flags = con::kArchive,
         .help = "Whether joining fetches files from the web address a server names.",
         .kind = con::CvarKind::boolean,
         .values = "1: from the web first (the default); 0: from the server alone"});
  v.add({.name = "cl_http_filelists",
         .fallback = "1",
         .flags = con::kArchive,
         .help = "Whether a server's web lists of further files are fetched and followed.",
         .kind = con::CvarKind::boolean,
         .values = "1: followed (the default); 0: not"});
  v.add({.name = "cl_http_max_connections",
         .fallback = "2",
         .flags = con::kArchive,
         .help = "How many files are fetched from the web at once.",
         .kind = con::CvarKind::integer,
         .min = 1,
         .max = 4,
         .values = "2: two at once (the default); 1 to 4: that many"});
  v.add({.name = "cl_http_proxy",
         .fallback = "",
         .flags = con::kArchive,
         .help = "The web proxy downloads go through.",
         .kind = con::CvarKind::text,
         .values = "\"\": none (the default); TEXT: host:port"});
  v.add({.name = "cl_http_default_url",
         .fallback = "",
         .flags = con::kArchive,
         .help = "Your own web address files are fetched from when a server names none.",
         .kind = con::CvarKind::text,
         .values = "\"\": none (the default); TEXT: an address such as http://host/q2/"});
  v.add({.name = "cl_http_insecure",
         .fallback = "0",
         .flags = con::kArchive,
         .help = "Whether a secure web address's certificate goes unchecked; secure addresses "
                 "are not fetched here, so their files come from the server.",
         .kind = con::CvarKind::boolean,
         .values = "0: checked (the default); 1: not"});
  v.add({.name = "cl_http_debug",
         .fallback = "0",
         .help = "Whether each web download's address is said as it starts.",
         .kind = con::CvarKind::boolean,
         .values = "0: not said (the default); 1: said"});
  v.add({.name = "cl_http_blocking_timeout",
         .fallback = "15",
         .flags = con::kArchive,
         .help = "Seconds of silence before a web list of servers is given up.",
         .kind = con::CvarKind::number,
         .min = 1,
         .values = "15: fifteen seconds (the default); a number: that many"});
  const char* helps[4] = {
      "Whether the game is running: always 1.", "Whether the game you play is paused.",
      "Whether this window runs a server.", "Whether the server this window runs is paused."};
  for (int k = 0; k < 4; ++k)
    v.add({.name = kStateNames[k],
           .fallback = k == 0 ? "1" : "0",
           .flags = con::kReadOnly,
           .help = helps[k],
           .kind = con::CvarKind::boolean,
           .values = "0: no; 1: yes (set by the game alone)"});
  v.bind({.name = "cl_warn_on_fps_rounding",
          .fallback = "1",
          .flags = con::kArchive,
          .help = "Whether you are told when cl_maxfps is rounded to whole milliseconds.",
          .kind = con::CvarKind::boolean,
          .values = "1: told (the default); 0: not"},
         &s.warn_fps);
  s.state = -1;
  s.fps_seen = 0;
}

std::string q2_map_version(std::string_view stated, std::span<const uint8_t> file, bool server) {
  if (stated.empty() || file.empty()) return {};
  const int32_t ours = shooters_edge::server::q2_map_checksum(file);
  const std::string theirs(stated);
  if (static_cast<int32_t>(std::strtoll(theirs.c_str(), nullptr, 10)) == ours) return {};
  return std::string("Local map version differs from ") + (server ? "server: " : "demo: ") +
         std::to_string(ours) + " != " + theirs;
}

int q2_fps_rounded(int fps) {
  if (fps <= 0) return 0;
  const int held = std::clamp(fps, 10, 125);
  return 1000 / (1000 / held);
}

void q2_fps_warning(con::Console& c, Q2Session& s) {
  const con::Cvar* v = c.cvars().find("cl_maxfps");
  if (v == nullptr || v->revision == s.fps_seen) return;
  s.fps_seen = v->revision;
  const int asked = std::clamp(v->integer(), 0, 125);
  const int got = q2_fps_rounded(asked);
  if (!s.warn_fps || asked < 10 || got == asked) return;
  c.print("cl_maxfps value " + std::to_string(asked) + " is inexact and will be rounded to " +
          std::to_string(got) + ".");
  if (!std::exchange(s.fps_told, true))
    c.print("(Set cl_warn_on_fps_rounding to 0 to disable this warning.)");
}

double q2_game_dt(const Q2Session& s, double host_framerate, double wall) {
  const double step = host_framerate > 0 ? host_framerate
                      : s.fixed_ms > 0   ? s.fixed_ms / 1000
                                         : wall;
  return step * std::max(0.0, s.timescale);
}

void q2_session_step(con::Console& c, Q2Session& s, bool joined, std::string_view map, bool hosting,
                     bool paused) {
  if (!joined) {
    if (s.joined) run(c, s.disconnect_cmd);
    s.joined = false;
    s.map.clear();
  } else {
    if (s.joined && !s.map.empty() && map != s.map) run(c, s.change_cmd);
    if (!map.empty() && map != s.map) run(c, s.begin_cmd);
    s.joined = true;
    s.map = std::string(map);
    q2_fps_warning(c, s);  // in a game: a cl_maxfps changed since it was checked
  }
  const int state = 1 | (paused ? 2 : 0) | (hosting ? 4 : 0) | (hosting && paused ? 8 : 0);
  if (state == s.state) return;
  s.state = state;
  for (int k = 0; k < 4; ++k)
    if (c.cvars().find(kStateNames[k]) != nullptr)
      c.cvars().set(kStateNames[k], (state & (1 << k)) != 0 ? "1" : "0", con::Source::code);
}

void q2_session_frame(con::Console& c, Q2Session& s, const net::Remote& remote,
                      const Listen& listen, bool paused) {
  const LoadedMap* hosted = listen.server != nullptr ? listen.map.get() : nullptr;
  const bool hosting = hosted != nullptr;
  const std::string& map = remote.active       ? remote.demo.state.map
                           : hosted != nullptr ? hosted->name
                                               : s.map;
  q2_session_step(c, s, remote.active || hosting, remote.active || hosting ? map : "", hosting,
                  hosting && (paused || listen.server->paused));
}

double view_behind(ge::render::FrameView& view, double units, double angle_deg,
                   const ge::shooter::BrushModel* world) {
  if (!(units > 0)) return 0;
  constexpr double kWallGap = 4;  // units the camera keeps off a wall
  const double heading = view.heading - angle_deg * kDeg;
  // Back along the way it looks (engine axes: heading clockwise from +z, pitch up positive).
  const double back[3] = {-std::sin(heading) * std::cos(view.pitch), -std::sin(view.pitch),
                          -std::cos(heading) * std::cos(view.pitch)};
  const double unit = ge::brush::kUnitM;
  double reach = units;
  if (world != nullptr) {
    const ge::shooter::Hull& hull = world->hulls[0];
    if (hull.nodes != nullptr && hull.planes != nullptr) {
      // Map units: x, y the plan, z up (the engine's x, z and y).
      const ge::shooter::Vec3f from{float(view.eye[0] / unit), float(view.eye[2] / unit),
                                    float(view.eye[1] / unit)};
      const double far = units + kWallGap;
      const ge::shooter::Vec3f to{float(from.x + back[0] * far), float(from.y + back[2] * far),
                                  float(from.z + back[1] * far)};
      const ge::shooter::HullTrace tr = ge::shooter::trace_hull(hull, from, to);
      if (tr.startsolid) return 0;
      if (tr.fraction < 1) reach = std::max(0.0, double(tr.fraction) * far - kWallGap);
    }
  }
  for (int k = 0; k < 3; ++k) view.eye[k] += back[k] * reach * unit;
  view.heading = heading;
  return reach;
}

double q2_third_person(const Q2Session& s, ge::render::FrameView& view,
                       const ge::shooter::BrushModel* world) {
  return s.third_person ? view_behind(view, s.third_range, s.third_angle, world) : 0;
}

}  // namespace shooters_edge::client
