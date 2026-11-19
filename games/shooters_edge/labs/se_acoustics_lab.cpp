// LAB `se-acoustics`: the room round the ear (ce_acoustics) on a classic map. The listener stands
// at each of the map's spawn points in turn and games/shooters_edge/client/room_ear's probe answers
// the room the synth is given there: the tail's RT60, its level, the outdoor echo's share and the
// early reflections. With ce_acoustics off every spot is the dry room (no tail, no taps).
//
// What it shows: a chart of RT60 and the echo's share at each spawn, and a table of every answer.
// Its checks: every spot answers a finite room, the rooms are not all alike (the probe reads the
// map, not a constant), and a spot under an open sky keeps more echo than the most closed one.
// With no --basedir it probes the weather fixture's hall and yard.
//
// ge:summary Lab: the room acoustics (ce_acoustics) probed at a classic map's spawn points
// ge:keywords lab ce_acoustics acoustics reverb rt60 room-probe spawn se-acoustics
// ge:skill audio labs
// ge:test tests/shooters_edge_room_ear_test.cpp
#include <cstdio>
#include <span>
#include <string>
#include <vector>

#include "brush/map.hpp"
#include "brush/read.hpp"
#include "brush/write.hpp"
#include "lab/chart.hpp"
#include "lab/lab.hpp"
#include "shooters_edge/client/files.hpp"
#include "shooters_edge/client/room_ear.hpp"

namespace {

namespace gl = ge::lab;
namespace br = ge::brush;
namespace cl = shooters_edge::client;

std::string fmt(double v, int digits) {
  char b[32];
  std::snprintf(b, sizeof b, "%.*f", digits, v);
  return b;
}

constexpr double kEarU = 22;  // the eye above a spawn's origin, map units
constexpr size_t kMost = 16;  // spawns probed at most

struct Spot {
  std::string name;
  double u[3];  // map units, z up
};

bool load(gl::Lab& lab, br::Map& map, std::vector<Spot>& spots, std::string& from) {
  std::string err;
  std::vector<uint8_t> bytes;
  const std::string basedir = lab.arg("basedir");
  if (basedir.empty()) {
    if (!br::write_bsp(br::weather_fixture(), br::Family::v38, &bytes, &err)) {
      lab.fail("se-acoustics: the weather fixture does not write: " + err,
               "next: run brush_write_test. Skill: brush.");
      return false;
    }
    from = "the weather fixture";
    spots = {{"hall", {320, -200, 32}}, {"yard", {320, 800, 32}}};
  } else {
    cl::SearchPath files;
    if (!cl::open_game_dir(basedir, lab.arg("game"), files, err)) {
      lab.fail("se-acoustics: " + err, "next: pass --basedir=<the folder holding the game's dir>");
      return false;
    }
    std::vector<uint8_t> store;
    const std::string name = lab.arg("mapname");
    const std::span<const uint8_t> f = cl::find_file(files, "maps/" + name + ".bsp", store);
    if (f.empty()) {
      lab.fail("se-acoustics: no maps/" + name + ".bsp in " + basedir + "/" + lab.arg("game"),
               "next: name a map the game folder holds (--mapname)");
      return false;
    }
    bytes.assign(f.begin(), f.end());
    from = name;
  }
  if (!br::read_bsp(bytes, &map, &err)) {
    lab.fail("se-acoustics: the map does not read: " + err, "next: run the bsp lab on it");
    return false;
  }
  if (!spots.empty()) return true;
  for (const br::Entity& e : map.entities) {
    const std::string& cls = e.get("classname");
    if (cls != "info_player_deathmatch" && cls != "info_player_start") continue;
    Spot s{cls == "info_player_start" ? "start" : "dm", {}};
    if (std::sscanf(e.get("origin").c_str(), "%lf %lf %lf", &s.u[0], &s.u[1], &s.u[2]) != 3)
      continue;
    s.u[2] += kEarU;
    s.name += std::to_string(spots.size() + 1);
    spots.push_back(s);
    if (spots.size() == kMost) break;
  }
  if (spots.empty()) {
    lab.fail("se-acoustics: " + from + " has no spawn point",
             "next: pick a map with info_player_start or info_player_deathmatch");
    return false;
  }
  return true;
}

void run(gl::Lab& lab) {
  br::Map map;
  std::vector<Spot> spots;
  std::string from;
  if (!load(lab, map, spots, from)) return;

  gl::LabTable table{
      "THE ROOM AT EACH SPAWN (ce_acoustics on; off is dry everywhere)",
      {"spot", "origin (units)", "RT60 (s)", "tail level", "echo share", "taps", "first tap (ms)"},
      {}};
  gl::Trace rt{"RT60 (s)", {}, {}}, echo{"outdoor echo share", {}, {}};
  double lo = 1e9, hi = -1e9, closed_echo = 0, open_echo = 0;
  bool finite = true;
  const double right[3] = {1, 0, 0};
  for (size_t i = 0; i < spots.size(); ++i) {
    const Spot& s = spots[i];
    const double ear[3] = {s.u[0] * br::kUnitM, s.u[2] * br::kUnitM, s.u[1] * br::kUnitM};
    const ge::audio::RoomSound r = cl::bsp_room(map, ear, right);
    if (!(r.decay_s >= 0 && r.decay_s < 60)) finite = false;
    if (r.decay_s < lo) lo = r.decay_s, open_echo = r.echo;
    if (r.decay_s > hi) hi = r.decay_s, closed_echo = r.echo;
    rt.x.push_back(double(i + 1));
    rt.y.push_back(r.decay_s);
    echo.x.push_back(double(i + 1));
    echo.y.push_back(r.echo);
    table.rows.push_back({s.name,
                          fmt(s.u[0], 0) + " " + fmt(s.u[1], 0) + " " + fmt(s.u[2] - kEarU, 0),
                          fmt(r.decay_s, 2), fmt(r.wet, 2), fmt(r.echo, 2), std::to_string(r.taps),
                          r.taps > 0 ? fmt(r.tap[0].delay_ms, 1) : std::string("-")});
  }
  lab.table(table);
  lab.metric("rooms.spots", double(spots.size()), "spots");
  lab.metric("rooms.rt60-least", lo, "s");
  lab.metric("rooms.rt60-most", hi, "s");

  gl::LineChart line;
  line.frame = {"THE ROOM AT EACH SPAWN", from + ", ce_acoustics on", "spawn", "s / share"};
  line.traces = {rt, echo};
  line.points = true;
  line.spikes = false;
  const gl::Chart chart = gl::line_chart(line);
  lab.shot({"rooms",
            chart.title,
            "The probe's answer at each spawn point of " + from +
                ": the tail's RT60 (how long a shot rings) and the outdoor echo's share (1 under "
                "an open sky, 0 in a closed room). With ce_acoustics off every spot is dry.",
            "the spots differing: halls ringing longer, open spots keeping the echo",
            {{"map", from}}},
           gl::to_picture(chart.plot));

  if (!finite) {
    lab.fail("a spot's RT60 is not a finite time under a minute",
             "next: bsp_room (games/shooters_edge/client/room_ear.cpp). Skill: audio.");
  }
  if (spots.size() > 1 && hi - lo < 0.05) {
    lab.fail("every spot answers the same room (RT60 " + fmt(lo, 2) +
                 " s): the probe reads "
                 "nothing of the map",
             "next: bsp_ray (games/shooters_edge/client/room_ear.cpp). Skill: audio.");
  }
  if (spots.size() > 1 && open_echo < closed_echo) {
    lab.fail("the shortest room keeps less outdoor echo than the longest",
             "next: probe_room's open share (engine/audio/room.cpp). Skill: audio.");
  }
}

const gl::LabSpec kSpec{
    "se-acoustics",
    gl::LabKind::sim,
    "game:shooters_edge",
    "Shooter's Edge's room acoustics (games/shooters_edge/client/room_ear, engine/audio/room)",
    "audio",
    "Does the room the synth is given follow the map round the listener: halls ringing, corridors "
    "short, open sky dying away?",
    {
        {"basedir", "", "the folder holding the game's folder; empty: the weather fixture"},
        {"game", "id1", "the game folder under --basedir (baseq2 for the sequel)"},
        {"mapname", "e1m1", "the map probed"},
    },
    "",
    run,
};
GE_LAB(kSpec);

}  // namespace
