// Quake II's session settings: the map commands, the clock and the state (q2_session.hpp).
//
// ge:summary Quake II's map commands run on a change, its game clock, its read-only state cvars
// ge:keywords cl_thirdperson cl_thirdperson_angle cl_thirdperson_range cam_dist chase_active
// ge:keywords third-person view_behind
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

void add_shared_session_cvars(con::Cvars& v, Q2Session& s) {
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
  // Every game's cfgs set it; a game hosted from the window reads its own (the server's cvars).
  v.add({.name = "developer",
         .fallback = "0",
         .help = "Whether developer lines reach the console.",
         .kind = con::CvarKind::integer,
         .min = 0,
         .values = "0: they are left out (the default); 1 or more: printed"});
  v.bind({.name = "in_grab",
          .fallback = "1",
          .flags = con::kArchive,
          .help = "Whether the game takes the mouse while it is in a window.",
          .kind = con::CvarKind::integer,
          .min = 0,
          .max = 2,
          .values = "1: it does (the default), as 2; 0: only while it covers the screen"},
         &s.grab);
  // Yourself seen from behind, in every game (Quake's chase_active, Quake II's cl_thirdperson):
  // a cheat, so a server that allows none keeps you in your own eyes.
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
  // The camera's distance behind the player is ours, `cam_dist`, the one a player watched in a
  // demo is seen from too (demo_playback.cpp registers it first, in this layer).
  s.dist = v.find("cam_dist");
  s.up = v.find("cam_up");
  v.add({.name = "net_clientport",
         .fallback = "-1",
         .help = "The network port your client sends from; a change applies at once.",
         .kind = con::CvarKind::integer,
         .min = -1,
         .max = 65535,
         .values = "-1: any free port (the default); 1..65535: that port"});
}

void add_q2_session_cvars(con::Cvars& v, Q2Session& s) {
  // Its camera behind you nearer than ours (`cam_dist`), and its spelling of it.
  v.bind({.name = "cam_dist",
          .fallback = "60",
          .flags = con::kArchive,
          .help = "How far behind the player the camera sits when he is seen from behind, in "
                  "units.",
          .min = 0,
          .max = 1000,
          .values = "60: the default; 0..1000: that many units, nearer where a wall is in the way",
          .overlap = con::Overlap::same},
         &s.third_range);
  v.add_synonym("cl_thirdperson_range", "cam_dist");
  v.bind({.name = "timescale",
          .fallback = "1",
          .flags = con::kCheat,
          .help = "How fast a game you host runs.",
          .min = 0,
          .max = 1000,
          .values = "1: as it should (the default); 0..1000: that many times as fast, only while "
                    "the server allows cheats"},
         &s.timescale);
  // fixedtime: every game's host_framerate in milliseconds, a cheat under this name (THE CLOCK).
  v.add_scaled_synonym("fixedtime", "host_framerate", 1000, true);
  v.bind({.name = "cl_autopause",
          .fallback = "1",
          .flags = con::kArchive,
          .help = "Whether a game you play alone waits while the console or a menu is up.",
          .kind = con::CvarKind::boolean,
          .values = "1: it waits (the default); 0: it runs on"},
         &s.autopause);
  v.bind({.name = "in_enable",
          .fallback = "1",
          .help = "Whether the mouse turns your view; the keys work either way.",
          .kind = con::CvarKind::boolean,
          .values = "1: it does (the default); 0: the mouse is left alone"},
         &s.mouse);
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
  // The later protocols (net/q2_proto_in.hpp), read at each `connect`.
  v.add({.name = "cl_protocol",
         .fallback = "0",
         .help = "Which protocol joining a server speaks.",
         .kind = con::CvarKind::integer,
         .values = "0: the newest the server offers (the default); 36, 35: that one, or the next "
                   "one down the server offers; 34: the original"});
  v.add({.name = "net_maxmsglen",
         .fallback = "1390",
         .flags = con::kArchive,
         .help = "The largest packet a server speaking a later protocol may send you, in bytes.",
         .kind = con::CvarKind::integer,
         .min = 0,
         .max = 4086,
         .values = "512..4086: that many bytes (1390 the default); 0: the most, 4086"});
  v.add({.name = "net_chantype",
         .fallback = "1",
         .help = "Which packet format joining a server on protocol 36 asks for.",
         .kind = con::CvarKind::integer,
         .min = 0,
         .max = 1,
         .values = "1: the newer, which splits a big message over packets (the default); 0: the "
                   "original"});
  v.add({.name = "cl_batchcmds",
         .fallback = "1",
         .help = "Whether moves on protocol 36 go several to a packet.",
         .kind = con::CvarKind::boolean,
         .values = "1: they do (the default); 0: one packet a move, as the original"});
  v.add({.name = "cl_packetdup",
         .fallback = "1",
         .help =
             "How many earlier packets' moves each packet on protocol 36 carries again, against "
             "losses.",
         .kind = con::CvarKind::integer,
         .min = 0,
         .max = 3,
         .values = "0..3: that many (1 the default)"});
  // The packets a second, our `cl_c2spps` (net/qw_remote.cpp), its `cl_maxpackets` the same
  // setting: 0 one a move, else at most that many; its 30 by default, and room for more.
  v.add({.name = "cl_c2spps",
         .fallback = "30",
         .flags = con::kArchive,
         .help = "How many packets a second you send a server; the moves between ride in the next "
                 "one.",
         .kind = con::CvarKind::integer,
         .min = 0,
         .max = 1000,
         .values = "0: one a move; 10..1000: at most that many (30 the default; under 10 taken as "
                   "10)",
         .overlap = con::Overlap::same});
  v.add_synonym("cl_maxpackets", "cl_c2spps");
  v.add({.name = "cl_instantpacket",
         .fallback = "1",
         .help = "Whether pressing attack or use on protocol 36 sends a packet at once, past "
                 "cl_maxpackets.",
         .kind = con::CvarKind::boolean,
         .values = "1: it does (the default); 0: it waits its turn"});
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
  const double step = host_framerate > 0 ? host_framerate : wall;
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
                   const ge::shooter::BrushModel* world, std::span<const BehindMover> movers,
                   double up) {
  if (!(units > 0)) return 0;
  constexpr double kWallGap = 4;  // units the camera keeps off a wall
  const double unit = ge::brush::kUnitM;
  if (up > 0) {  // raised first, short of a ceiling (map units: z up, the engine's y)
    double lift = up;
    if (world != nullptr && world->hulls[0].nodes != nullptr && world->hulls[0].planes != nullptr) {
      const ge::shooter::Vec3f from{float(view.eye[0] / unit), float(view.eye[2] / unit),
                                    float(view.eye[1] / unit)};
      const ge::shooter::HullTrace tr = ge::shooter::trace_hull(
          world->hulls[0], from, {from.x, from.y, float(from.z + up + kWallGap)});
      if (tr.startsolid) return 0;
      if (tr.fraction < 1) lift = std::max(0.0, tr.fraction * (up + kWallGap) - kWallGap);
    }
    view.eye[1] += lift * unit;
  }
  const double heading = view.heading - angle_deg * kDeg;
  // Back along the way it looks (engine axes: heading clockwise from +z, pitch up positive).
  const double back[3] = {-std::sin(heading) * std::cos(view.pitch), -std::sin(view.pitch),
                          -std::cos(heading) * std::cos(view.pitch)};
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
      double fraction = tr.fraction;
      for (const BehindMover& m : movers) {  // a door or a platform where it stands
        if (m.model == nullptr) continue;
        const ge::shooter::Hull& mh = m.model->hulls[0];
        if (mh.nodes == nullptr || mh.planes == nullptr) continue;
        const ge::shooter::Vec3f off{float(m.offset[0]), float(m.offset[1]), float(m.offset[2])};
        const ge::shooter::HullTrace mt =
            ge::shooter::trace_hull(mh, {from.x - off.x, from.y - off.y, from.z - off.z},
                                    {to.x - off.x, to.y - off.y, to.z - off.z});
        if (mt.startsolid) continue;  // the eye inside it (riding it): it does not stop the camera
        fraction = std::min(fraction, double(mt.fraction));
      }
      if (fraction < 1) reach = std::max(0.0, fraction * far - kWallGap);
    }
  }
  for (int k = 0; k < 3; ++k) view.eye[k] += back[k] * reach * unit;
  view.heading = heading;
  return reach;
}

double own_figure_alpha(double behind) {
  if (!(behind > kOwnFigureUnits)) return 0;
  return std::min(1.0, (behind - kOwnFigureUnits) / (kOwnSolidUnits - kOwnFigureUnits));
}

double q2_third_person(const Q2Session& s, ge::render::FrameView& view,
                       const ge::shooter::BrushModel* world, std::span<const BehindMover> movers) {
  const double range = s.dist != nullptr ? s.dist->number : s.third_range;  // cam_dist
  const double up = s.up != nullptr ? s.up->number : 0;                     // cam_up
  return s.third_person ? view_behind(view, range, s.third_angle, world, movers, up) : 0;
}

}  // namespace shooters_edge::client
