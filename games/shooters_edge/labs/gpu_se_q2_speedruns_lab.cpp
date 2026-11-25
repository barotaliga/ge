// LAB `se-q2-speedruns`: how do Quake II's fastest players clear each map?
// Reads a folder of recorded runs (`.dm2`, one level each from its start to its exit) and draws
// what the campaign bot's planner can learn from them: the route (the player's origin every 0.2 s
// of the game's clock), the items picked up in order (the bar's pickup line: keys, weapons, power
// ups), where a secret or a goal counted (the sound both play, heard as the player reaches it: an
// exit's button that ends a unit's goal plays it too), the boosts (the
// player rising faster than a jump or stairs lift him: a rocket or grenade jump), and the time to
// the exit (the recording's last frame, or the intermission's start when the unit ends there).
// A level played twice in a unit is a second file (`base2ii.dm2`): its run starts at the arrival.
//
// The table, a run a row: the file, the map, the time, the pickups and secrets counted, the
// boosts. A file whose first blocks come before the server data (a tool's stuffed "hand 3") reads
// too. Pictures: each run on the live map's plate. The attachment `routes.tsv`: every run's
// track, pickups, secrets and boosts as rows (the planner's data: `run map t x y z what name`).
//
// ge:summary Lab: recorded fast runs of Quake II's maps read: route, pickups, secrets, boosts, time
// ge:keywords lab se-q2-speedruns speedrun dm2 route-data any-percent boosts pickups
// ge:keywords rocket-jump exit-time campaign planner q2
// ge:skill shooter bots labs
// ge:test tests/shooters_edge_demo_q2_test.cpp
#include <algorithm>
#include <array>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

#include "core/num_text.hpp"
#include "lab/lab.hpp"
#include "shooters_edge/client/demo.hpp"
#include "shooters_edge/client/files.hpp"
#include "shooters_edge/livemap/live_map.hpp"
#include "shooters_edge/livemap/route.hpp"

namespace {

namespace gl = ge::lab;
namespace lv = shooters_edge::livemap;
namespace cl = shooters_edge::client;
using ge::core::num::to_fixed;
using In = std::vector<std::pair<std::string, std::string>>;
using P3 = std::array<float, 3>;

constexpr double kTrackStep_s = 0.2;  // the route's point spacing on the game's clock
constexpr double kBoostWindow_s = 0.3;
constexpr float kBoostRise = 150;  // units in the window: a jump lifts ~40, stairs at a run ~95
constexpr float kTeleport = 400;   // a step this long between frames is a teleporter
constexpr int kStatPickup = 8;     // the bar's stat that names the item just picked up
// The settings string the item names start at: the classic protocol's and the extended one's.
constexpr int kItemsClassic = 1056, kItemsExtended = 12606;

struct Mark {
  double t = 0;
  P3 at{};
  std::string name;
};

struct Run {
  std::string file, map;
  double end_s = -1;  // the last frame or the intermission, game clock from the first frame
  bool intermission = false;
  std::vector<P3> track;
  std::vector<Mark> pickups, secrets, boosts;
  std::string error;
};

P3 at(const cl::DemoState& s) {
  if (static_cast<size_t>(s.player) < s.players.size()) {
    const auto& o = s.players[static_cast<size_t>(s.player)].origin;
    return {o.x, o.y, o.z};
  }
  return {s.eye.x, s.eye.y, s.eye.z};
}

std::string item_name(const cl::DemoState& s, int stat) {
  const int base = s.protocol >= 3434 && s.protocol <= 3436 ? kItemsExtended : kItemsClassic;
  const int i = stat - base;
  if (i <= 0 || static_cast<size_t>(i) >= s.item_names.size()) return "item " + std::to_string(i);
  return s.item_names[static_cast<size_t>(i)];
}

Run read_run(const std::filesystem::path& path, cl::DemoState* last_state) {
  Run r;
  r.file = path.filename().string();
  std::vector<uint8_t> bytes;
  if (FILE* f = std::fopen(path.string().c_str(), "rb")) {
    std::array<uint8_t, 65536> buf;
    size_t n = 0;
    while ((n = std::fread(buf.data(), 1, buf.size(), f)) > 0)
      bytes.insert(bytes.end(), buf.begin(), buf.begin() + static_cast<long>(n));
    std::fclose(f);
  }
  cl::Demo d;
  if (!cl::open_demo(std::move(bytes), d, r.error)) return r;
  double t0 = -1, next_point = 0, last_t = -1, last_boost = -10;
  int last_pickup = 0;
  P3 last{};
  std::vector<std::pair<double, P3>> recent;  // the origin over the boost window
  for (;;) {
    std::string error;
    const cl::DemoRead rd = cl::next_frame(d, error);
    if (rd == cl::DemoRead::error) {
      r.error = error;
      break;
    }
    if (rd == cl::DemoRead::end) break;
    const cl::DemoState& s = d.state;
    if (s.map.empty()) continue;
    if (r.map.empty()) r.map = s.map;
    if (s.map != r.map) break;  // the next level: this run is over
    if (t0 < 0) t0 = s.time;
    const double t = s.time - t0;
    const P3 p = at(s);
    if (*last_state = s; s.intermission) {
      r.end_s = t;
      r.intermission = true;
      break;
    }
    r.end_s = t;
    const int pick = s.q2_stats[kStatPickup];
    if (pick != 0 && pick != last_pickup) r.pickups.push_back({t, p, item_name(s, pick)});
    last_pickup = pick;
    for (const cl::DemoSound& o : s.started)
      if (o.sound > 0 && static_cast<size_t>(o.sound) < s.sounds.size() &&
          s.sounds[static_cast<size_t>(o.sound)].find("secret") != std::string::npos)
        r.secrets.push_back({t, p, s.sounds[static_cast<size_t>(o.sound)]});
    if (t <= last_t) continue;  // a frame on the same tic
    const float dx = p[0] - last[0], dy = p[1] - last[1], dz = p[2] - last[2];
    if (last_t >= 0 && dx * dx + dy * dy + dz * dz > kTeleport * kTeleport) recent.clear();
    last_t = t;
    last = p;
    recent.push_back({t, p});
    while (!recent.empty() && recent.front().first < t - kBoostWindow_s)
      recent.erase(recent.begin());
    if (!recent.empty() && p[2] - recent.front().second[2] > kBoostRise && t - last_boost > 1) {
      r.boosts.push_back({recent.front().first, recent.front().second, "boost"});
      last_boost = t;
    }
    if (t + 1e-6 >= next_point) {
      r.track.push_back(p);
      next_point += kTrackStep_s;
    }
  }
  // The first frame can hold the origin before the player is placed: drop a first point that a
  // teleporter's step lies from the next.
  if (r.track.size() > 1) {
    const P3 a = r.track[0], b = r.track[1];
    const float dx = b[0] - a[0], dy = b[1] - a[1];
    if (dx * dx + dy * dy > kTeleport * kTeleport) r.track.erase(r.track.begin());
  }
  return r;
}

void run(gl::Lab& lab) {
  const std::string dir = lab.arg("demos");
  if (dir.empty()) {
    lab.note("no --demos folder: nothing to read (the smoke run)");
    lab.metric("runs", 0, "count");
    return;
  }
  std::vector<std::filesystem::path> files;
  std::error_code ec;
  const std::string maps = lab.arg("maps");
  for (const auto& e : std::filesystem::directory_iterator(dir, ec)) {
    if (e.path().extension().string() != ".dm2") continue;
    const std::string stem = e.path().stem().string();
    bool keep = maps.empty();
    for (size_t a = 0; !keep && a <= maps.size();) {
      size_t b = maps.find(',', a);
      if (b == std::string::npos) b = maps.size();
      if (b > a && stem == maps.substr(a, b - a)) keep = true;
      a = b + 1;
    }
    if (keep) files.push_back(e.path());
  }
  if (ec) {
    lab.fail("se-q2-speedruns: the folder " + dir + " does not read: " + ec.message(),
             "next: pass --demos=<a folder of .dm2 files>");
    return;
  }
  std::sort(files.begin(), files.end());
  std::vector<Run> runs;
  std::vector<cl::DemoState> states;
  for (const auto& f : files) {
    cl::DemoState st;
    runs.push_back(read_run(f, &st));
    states.push_back(std::move(st));
  }
  const In in = {{"demos", "the folder given"}, {"maps", maps.empty() ? "every map" : maps}};
  gl::LabTable t{
      "each recorded run",
      {"file", "map", "time s", "end", "pickups", "secrets+goals", "boosts", "points", "read"},
      {}};
  std::string tsv = "run\tmap\tt\tx\ty\tz\twhat\tname\n";
  const auto row = [&](const Run& r, double tt, const P3& p, const char* what,
                       const std::string& name) {
    tsv += r.file + "\t" + r.map + "\t" + to_fixed(tt, 2) + "\t" + to_fixed(p[0], 0) + "\t" +
           to_fixed(p[1], 0) + "\t" + to_fixed(p[2], 0) + "\t" + what + "\t" + name + "\n";
  };
  int read = 0;
  for (const Run& r : runs) {
    read += r.error.empty() ? 1 : 0;
    t.rows.push_back({r.file, r.map, r.end_s >= 0 ? to_fixed(r.end_s, 1) : "-",
                      r.intermission ? "intermission" : "last frame",
                      std::to_string(r.pickups.size()), std::to_string(r.secrets.size()),
                      std::to_string(r.boosts.size()), std::to_string(r.track.size()),
                      r.error.empty() ? "yes" : r.error});
    for (size_t i = 0; i < r.track.size(); i++)
      row(r, static_cast<double>(i) * kTrackStep_s, r.track[i], "track", "");
    for (const Mark& m : r.pickups) row(r, m.t, m.at, "pickup", m.name);
    for (const Mark& m : r.secrets) row(r, m.t, m.at, "secret", m.name);
    for (const Mark& m : r.boosts) row(r, m.t, m.at, "boost", "");
  }
  lab.table(t);
  lab.metric("runs", static_cast<double>(runs.size()), "count");
  lab.metric("runs.read", read, "count");
  lab.attach("routes.tsv", std::vector<uint8_t>(tsv.begin(), tsv.end()));
  const std::string game = lab.arg("game");
  if (game.empty() || lab.arg("pictures") == "0") return;
  const size_t cut = game.find_last_of('/');
  cl::SearchPath search;
  std::string error;
  if (!cl::open_game_dir(cut == std::string::npos ? "." : game.substr(0, cut),
                         cut == std::string::npos ? game : game.substr(cut + 1), search, error)) {
    lab.fail("se-q2-speedruns: " + error, "next: pass --game=<install>/baseq2; skill: shooter");
    return;
  }
  for (size_t i = 0; i < runs.size(); i++) {
    const Run& r = runs[i];
    if (r.track.empty()) continue;
    std::vector<uint8_t> bsp_store;
    const std::span<const uint8_t> bsp = cl::find_file(search, "maps/" + r.map + ".bsp", bsp_store);
    if (bsp.empty()) {
      lab.note(r.file + ": maps/" + r.map + ".bsp is not in the game's files");
      continue;
    }
    std::vector<uint8_t> store;
    const std::span<const uint8_t> pal = lv::recording_palette(search, states[i], store);
    lv::Plate plate;
    if (!lv::build_plate(bsp, r.map, pal, 1400, &plate, &error, &search)) {
      lab.note(r.file + ": no plate: " + error);
      continue;
    }
    lv::RouteMarks marks;
    marks.track = r.track;
    for (const Mark& m : r.pickups) marks.kills.push_back(m.at);  // crosses: what he took
    for (const Mark& m : r.secrets) marks.secrets.push_back(m.at);
    for (const Mark& m : r.boosts) marks.exits.push_back(m.at);  // rings: where a boost lifted him
    const ge::ui::Image img = lv::draw_route(plate, marks);
    gl::Picture p;
    p.width = img.width;
    p.height = img.height;
    p.rgba = img.rgba;
    std::string stem = r.file.substr(0, r.file.find('.'));
    std::replace(stem.begin(), stem.end(), '_', '-');
    lab.shot({"run-" + stem,
              "RUN " + r.map + " (" + r.file + "), " +
                  (r.end_s >= 0 ? to_fixed(r.end_s, 1) + " S" : std::string("NO END")) +
                  ", PICKUPS " + std::to_string(r.pickups.size()) + ", SECRETS AND GOALS " +
                  std::to_string(r.secrets.size()) + ", BOOSTS " + std::to_string(r.boosts.size()),
              "A recorded fast run on the live map's plate: the route every 0.2 s (blue at the "
              "start to red at the end), the items picked up (crosses), where a secret or a "
              "goal counted (gold diamonds) and where a boost lifted him (gold rings: a rocket or "
              "grenade "
              "jump).",
              "the route ends at the exit and every boost sits where a ledge is otherwise out of "
              "reach",
              in},
             p);
  }
}

const gl::LabSpec kSpec{
    "se-q2-speedruns",
    gl::LabKind::scenario,
    "game:shooters_edge",
    "games/shooters_edge/client/demo.hpp read for routes: Quake II's recorded fast runs as the "
    "campaign planner's data",
    "bots",
    "How do the fastest recorded runs of Quake II clear each map: in what order, by which "
    "boosts, and in how long?",
    {
        {"demos", "", "a folder of recorded runs (.dm2; none: the smoke run reads nothing)"},
        {"game", "", "the game folder (<install>/baseq2): the plates the routes go on"},
        {"maps", "", "a comma list of file stems to keep (base1,base2ii; none: every file)"},
        {"pictures", "1", "0: the table and routes.tsv alone"},
    },
    "",
    run,
};
GE_LAB(kSpec);

}  // namespace
