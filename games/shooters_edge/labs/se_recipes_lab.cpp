// LAB `se-recipes`: THE MOVES PEOPLE MAKE AGAIN AND AGAIN, read off a folder of recorded games as
// recipes a bot can play back (plugins/shooter/recipes.hpp, docs/goals/M18r.md § 0r). Every demo
// in `--dir` (or `--file`; by default a few short matches written in code on the fixture halls)
// is scanned with every player's track (client/demo_scan.hpp), each track read as hops on its
// map's hulls (shooter/hops.hpp) and cut into HOP STARTS (from a stand or a walk to the target
// speed by chained jumps).
//
// THE LEVELS: each player with `--least` seconds of track or more is graded by his median speed
// across (as se-rj does), the slowest third AMATEUR, the middle INTERMEDIATE, the fastest third
// EXPERT; each level's starts are tabled side by side: a player-minute, made, the time to the
// target, the entry's speed and whether he stood, the run before the first jump and its keys, the
// first takeoff's speed and the view's lag there.
//
// THE SPOTS: starts whose entries lie within one and a half `--cell` of each other's gathered
// (each into the first gathering whose mean entry is that near); one made `--gold` times or more,
// by two players or more, is a SPOT, a place people start from again and again: tabled with its
// xyz, its heading, its starts by level and made, and drawn on the map from above (every start a
// line from its entry to its end, green made and red stalled; the spots numbered). Every start is
// written to hop_starts.tsv (the recipes' rows to come).
//
// Deterministic: the digest holds the counts.
//
// ge:summary Lab: hop starts read off many demos: by level, keys, time to 400, the spots
// ge:keywords lab se-recipes recipe recipes hop-start time-to-400 stand-start copy-movement
// ge:keywords demos mvd keys levels spots entry-zone movement-skill
// ge:skill bots labs
// ge:test tests/shooter_recipes_test.cpp
#include <algorithm>
#include <array>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "core/num_text.hpp"
#include "core/parallel.hpp"
#include "lab/lab.hpp"
#include "shooter/hops.hpp"
#include "shooter/recipes.hpp"
#include "shooters_edge/client/demo.hpp"
#include "shooters_edge/client/demo_scan.hpp"
#include "shooters_edge/client/demo_write.hpp"
#include "shooters_edge/labs/move_study.hpp"
#include "shooters_edge/labs/wire_view.hpp"

namespace {

namespace gl = ge::lab;
namespace sh = ge::shooter;
namespace cl = shooters_edge::client;
namespace la = shooters_edge::labs;
using ge::core::num::to_fixed;
using In = std::vector<std::pair<std::string, std::string>>;

constexpr int kLevels = 3;
constexpr const char* kLevelName[kLevels] = {"amateur", "intermediate", "expert"};
constexpr float kDeg = static_cast<float>(180.0 / ge::core::kPi);

struct Player {
  std::string name, map;
  double seconds = 0;
  float speed = 0;  // his median speed across, units/s
  int level = -1;   // -1: too short a track to grade
};

struct Start {
  int demo = 0, player = 0;
  std::string map;
  sh::HopStart s;
};

float quantile(std::vector<float> v, float q) {
  if (v.empty()) return 0;
  std::sort(v.begin(), v.end());
  return v[static_cast<size_t>(q * static_cast<float>(v.size() - 1))];
}

double share(int part, int whole) { return whole > 0 ? static_cast<double>(part) / whole : 0; }

// The keys' class: forward with a side (a circle start), forward alone, a side alone, other.
int keys_class(uint8_t k) {
  const bool f = (k & sh::kKeyForward) != 0, side = (k & (sh::kKeyLeft | sh::kKeyRight)) != 0;
  if ((k & sh::kKeyBack) != 0) return 3;
  if (f && side) return 0;
  if (f) return 1;
  if (side) return 2;
  return 3;
}
constexpr const char* kKeysClass[4] = {"F+side", "F", "side", "other"};

// The scripted matches the lab reads with no demo given: on the fixture halls' hall, a player
// stands, runs and hops round a circle gaining speed (made), one hops without gaining (stalled),
// one only runs (no start).
std::vector<std::vector<uint8_t>> written_matches(int count) {
  std::vector<std::vector<uint8_t>> out;
  constexpr float kDt = 0.013f, kStand = 24, kRadius = 140;
  constexpr int kFrames = 330, kFlight = 52;
  for (int m = 0; m < count; m++) {
    cl::MatchScript s;
    s.frame_s = kDt;
    s.frames = kFrames;
    const float gains[3] = {80, 0, -1};
    const char* names[3] = {"hopper", "staller", "runner"};
    for (int k = 0; k < 3; k++) {
      cl::MatchPlayer p;
      p.name = names[k];
      const float cx = 256, cy = 192, phase = static_cast<float>(m * 3 + k) * 0.7f;
      float along = 0, speed = 0;
      int air = 0;  // frames into the current hop (0: on the ground)
      for (int f = 0; f < kFrames; f++) {
        const float t = static_cast<float>(f) * kDt;
        if (t > 0.3f && speed < 320 && air == 0) speed = std::min(320.f, speed + 1100 * kDt);
        if (gains[k] >= 0 && speed >= 320 && air == 0) air = 1;  // jump on reaching a run
        float z = kStand;
        if (air > 0) {
          const float st = static_cast<float>(air) * kDt;
          const float rise = 400.f * kFlight * kDt;
          z = air == kFlight ? kStand : kStand + rise * st - 400 * st * st;
          speed += gains[k] * kDt;
          air = air == kFlight ? 0 : air + 1;  // a frame on the ground, then the next jump
        }
        along += speed * kDt;
        const float a = phase + along / kRadius;
        const float c = static_cast<float>(ge::core::libm::cos(static_cast<double>(a)));
        const float sn = static_cast<float>(ge::core::libm::sin(static_cast<double>(a)));
        p.origins.push_back({cx + kRadius * c, cy + kRadius * sn, z});
        p.angles.push_back({0, (a + static_cast<float>(ge::core::kPi) / 2) * kDeg, 0});
      }
      s.players.push_back(p);
    }
    out.push_back(cl::write_multiview_match(s));
  }
  return out;
}

// A map read once for every demo of it.
struct Maps {
  std::string basedir, game;
  std::map<std::string, std::unique_ptr<la::StudyMap>> by_name;
  la::StudyMap* get(const std::string& name, std::string& error) {
    auto it = by_name.find(name);
    if (it != by_name.end()) return it->second.get();
    auto m = std::make_unique<la::StudyMap>();
    const bool fixture = basedir.empty();
    if (!la::load_study_map("", fixture ? "" : basedir, game, name, *m, error)) return nullptr;
    if (fixture && name != m->name) {
      error = "the demos' map " + name + " is not the fixture halls";
      return nullptr;
    }
    la::StudyMap* raw = m.get();
    by_name[name] = std::move(m);
    return raw;
  }
};

// The numbers of a set of starts.
struct Tally {
  int starts = 0, made = 0, stood = 0;
  std::array<int, 4> keys{};
  std::vector<float> t_target, low_target, speed_in, run_s, speed_off, lag, top;
  void add(const sh::HopStart& s) {
    starts++;
    made += s.made ? 1 : 0;
    stood += s.stood ? 1 : 0;
    keys[static_cast<size_t>(keys_class(s.run_keys))]++;
    if (s.made) t_target.push_back(s.to_target_s), low_target.push_back(s.low_to_target_s);
    speed_in.push_back(s.speed_in);
    run_s.push_back(s.run_s);
    speed_off.push_back(s.speed_off);
    lag.push_back(s.lag_off < 0 ? -s.lag_off : s.lag_off);
    top.push_back(s.top);
  }
};

void run(gl::Lab& lab) {
  const double least_s = lab.arg_num("least");
  const double cell = std::max(16.0, lab.arg_num("cell"));
  const int gold = std::max(2, static_cast<int>(lab.arg_num("gold")));
  const int most = static_cast<int>(lab.arg_num("most"));
  const int rows = static_cast<int>(lab.arg_num("rows"));
  sh::HopStartSettings set;
  set.target = static_cast<float>(lab.arg_num("target"));
  set.within_s = lab.arg_num("within");
  set.slow = static_cast<float>(lab.arg_num("slow"));

  std::vector<std::string> paths;
  const std::string dir = lab.arg("dir"), file = lab.arg("file");
  if (!file.empty()) paths.push_back(file);
  if (!dir.empty()) {
    std::error_code ec;
    for (const auto& e : std::filesystem::directory_iterator(dir, ec))
      if (e.is_regular_file() && e.path().extension() == ".mvd") paths.push_back(e.path().string());
    if (ec) {
      lab.fail("se-recipes: the folder " + dir + " does not list: " + ec.message(),
               "next: pass --dir=<a folder of .mvd recordings>");
      return;
    }
    std::sort(paths.begin() + (file.empty() ? 0 : 1), paths.end());
  }
  if (most > 0 && static_cast<int>(paths.size()) > most) paths.resize(static_cast<size_t>(most));
  const bool written = paths.empty();
  std::vector<std::vector<uint8_t>> made;
  if (written) made = written_matches(static_cast<int>(lab.arg_num("written")));
  const size_t demo_count = written ? made.size() : paths.size();

  struct Scanned {
    bool ok = false;
    std::string error;
    cl::DemoScan scan;
  };
  std::vector<Scanned> scans(demo_count);
  ge::core::parallel_bands(demo_count, demo_count, [&](size_t b, size_t e) {
    for (size_t i = b; i < e; i++) {
      cl::Demo d;
      Scanned& o = scans[i];
      o.ok = cl::open_demo(written ? made[i] : la::read_whole(paths[i]), d, o.error) &&
             cl::scan_demo(d, o.scan, o.error, true);
    }
  });

  Maps maps{lab.arg("basedir"), lab.arg("game"), {}};
  std::vector<Player> players;
  std::vector<Start> starts;
  int read = 0, broken = 0, no_map = 0;
  for (size_t i = 0; i < demo_count; i++) {
    Scanned& sc = scans[i];
    if (!sc.ok) {
      if (broken++ < 3)
        lab.note("skipped " + (written ? "match " + std::to_string(i) : paths[i]) + ": " +
                 sc.error);
      continue;
    }
    const cl::DemoScan& scan = sc.scan;
    std::string error;
    la::StudyMap* m = maps.get(scan.map, error);
    if (!m) {
      if (no_map++ < 3) lab.note("skipped a demo of " + scan.map + ": " + error);
      continue;
    }
    read++;
    std::vector<sh::HopReading> hops(32);
    std::vector<std::vector<sh::HopStart>> found(32);
    ge::core::parallel_bands(32, 32, [&](size_t b, size_t e) {
      for (size_t k = b; k < e; k++) {
        const std::vector<sh::TrackPoint>& tr = scan.tracks[k];
        if (tr.size() < 8) continue;
        sh::read_hops(tr, *m->world, sh::HopSettings{}, hops[k]);
        sh::read_hop_starts(tr, hops[k], set, found[k]);
      }
    });
    for (size_t k = 0; k < 32; k++) {
      if (scan.tracks[k].size() < 8) continue;
      Player p;
      p.name = scan.names[k].empty() ? "slot " + std::to_string(k) : scan.names[k];
      p.map = scan.map;
      p.seconds = hops[k].summary.seconds;
      p.speed = hops[k].summary.speed_p50;
      const int at = static_cast<int>(players.size());
      players.push_back(p);
      for (sh::HopStart& s : found[k]) starts.push_back({static_cast<int>(i), at, scan.map, s});
    }
  }
  if (read == 0) {
    lab.fail("se-recipes: no demo read (" + std::to_string(broken) + " broken, " +
                 std::to_string(no_map) + " on a map not found)",
             "next: pass --basedir=<the folder holding the game folder> with --dir");
    return;
  }

  // The levels: thirds by median speed over the players graded.
  std::vector<std::pair<float, size_t>> graded;
  for (size_t k = 0; k < players.size(); k++)
    if (players[k].seconds >= least_s) graded.push_back({players[k].speed, k});
  std::sort(graded.begin(), graded.end());
  for (size_t r = 0; r < graded.size(); r++)
    players[graded[r].second].level =
        static_cast<int>(r * kLevels / std::max<size_t>(1, graded.size()));
  std::array<double, kLevels + 1> minutes{};
  for (const Player& p : players) {
    minutes[kLevels] += p.seconds / 60;
    if (p.level >= 0) minutes[static_cast<size_t>(p.level)] += p.seconds / 60;
  }
  std::array<Tally, kLevels + 1> tally;  // by level, then every start
  for (const Start& s : starts) {
    tally[kLevels].add(s.s);
    const int lv = players[static_cast<size_t>(s.player)].level;
    if (lv >= 0) tally[static_cast<size_t>(lv)].add(s.s);
  }

  gl::LabTable lt{"hop starts by level (from a stand or a walk under " + to_fixed(set.slow, 0) +
                      " to " + to_fixed(set.target, 0) + " u/s by chained jumps)",
                  {"level", "players", "minutes", "starts", "a minute", "made %", "to target p25 s",
                   "p50 s", "entry u/s p50", "stood %", "run s p50", "takeoff u/s p50",
                   "lag p50 deg", "F+side %", "F %", "side %", "top p50"},
                  {}};
  for (int lv = 0; lv <= kLevels; lv++) {
    const Tally& t = tally[static_cast<size_t>(lv)];
    int count = 0;
    for (const Player& p : players)
      if (lv == kLevels || p.level == lv) count++;
    const double mins = minutes[static_cast<size_t>(lv)];
    lt.rows.push_back(
        {lv == kLevels ? "every player" : kLevelName[lv], std::to_string(count), to_fixed(mins, 1),
         std::to_string(t.starts), to_fixed(mins > 0 ? t.starts / mins : 0, 2),
         to_fixed(100 * share(t.made, t.starts), 0), to_fixed(quantile(t.t_target, 0.25f), 2),
         to_fixed(quantile(t.t_target, 0.5f), 2), to_fixed(quantile(t.speed_in, 0.5f), 0),
         to_fixed(100 * share(t.stood, t.starts), 0), to_fixed(quantile(t.run_s, 0.5f), 2),
         to_fixed(quantile(t.speed_off, 0.5f), 0), to_fixed(quantile(t.lag, 0.5f), 0),
         to_fixed(100 * share(t.keys[0], t.starts), 0),
         to_fixed(100 * share(t.keys[1], t.starts), 0),
         to_fixed(100 * share(t.keys[2], t.starts), 0), to_fixed(quantile(t.top, 0.5f), 0)});
    const std::string key = std::string("hs.") + (lv == kLevels ? "all" : kLevelName[lv]) + ".";
    lab.metric(key + "starts", t.starts, "count");
    lab.metric(key + "per-min", mins > 0 ? t.starts / mins : 0, "1/min");
    lab.metric(key + "made-share", share(t.made, t.starts), "share");
    lab.metric(key + "to-target-p50", quantile(t.t_target, 0.5f), "s");
    lab.metric(key + "low-to-target-p50", quantile(t.low_target, 0.5f), "s");
    lab.metric(key + "entry-p50", quantile(t.speed_in, 0.5f), "u/s");
    lab.metric(key + "stood-share", share(t.stood, t.starts), "share");
    lab.metric(key + "run-s-p50", quantile(t.run_s, 0.5f), "s");
    lab.metric(key + "takeoff-p50", quantile(t.speed_off, 0.5f), "u/s");
    lab.metric(key + "lag-p50", quantile(t.lag, 0.5f), "deg");
    for (int c = 0; c < 4; c++)
      lab.metric(key + "keys." + kKeysClass[c] + "-share",
                 share(t.keys[static_cast<size_t>(c)], t.starts), "share");
  }
  lab.table(lt);

  // The time to the target by the entry: from a stand, from a walk.
  {
    gl::LabTable tt{"the time to " + to_fixed(set.target, 0) +
                        " by the slowest before the first jump, made starts (s)",
                    {"entered", "made", "of", "p10", "p25", "p50", "p75", "from the low p50",
                     "takeoff u/s p50", "run s p50"},
                    {}};
    const float edges[4] = {0, set.stood, 150, set.slow};
    const char* names[3] = {"from a stand", "from a slow walk", "from a walk"};
    for (int band = 0; band < 3; band++) {
      const auto in_band = [&](float v, int b) {
        return v >= edges[b] && (b == 2 ? v < 1e9f : v < edges[b + 1]);
      };
      std::vector<float> run, from_low;
      std::vector<float> ts, off;
      int of = 0;
      for (const Start& s : starts) {
        if (!in_band(s.s.low, band)) continue;
        of++;
        if (s.s.made) ts.push_back(s.s.to_target_s), from_low.push_back(s.s.low_to_target_s);
        off.push_back(s.s.speed_off);
        run.push_back(s.s.run_s);
      }
      tt.rows.push_back({names[band], std::to_string(ts.size()), std::to_string(of),
                         to_fixed(quantile(ts, 0.1f), 2), to_fixed(quantile(ts, 0.25f), 2),
                         to_fixed(quantile(ts, 0.5f), 2), to_fixed(quantile(ts, 0.75f), 2),
                         to_fixed(quantile(from_low, 0.5f), 2), to_fixed(quantile(off, 0.5f), 0),
                         to_fixed(quantile(run, 0.5f), 2)});
      const std::string key = std::string("hs.band") + std::to_string(band) + ".";
      lab.metric(key + "starts", of, "count");
      lab.metric(key + "made-share", share(static_cast<int>(ts.size()), of), "share");
      lab.metric(key + "to-target-p50", quantile(ts, 0.5f), "s");
      lab.metric(key + "run-s-p50", quantile(run, 0.5f), "s");
      lab.metric(key + "low-to-target-p50", quantile(from_low, 0.5f), "s");
    }
    lab.table(tt);
  }

  // The spots: entries gathered by place.
  struct Spot {
    std::string map;
    double x = 0, y = 0, z = 0, hx = 0, hy = 0;
    std::vector<size_t> members;
  };
  std::vector<Spot> spots;
  const double near2 = (1.5 * cell) * (1.5 * cell);
  for (size_t i = 0; i < starts.size(); i++) {
    const Start& s = starts[i];
    Spot* into = nullptr;
    for (Spot& g : spots) {
      if (g.map != s.map) continue;
      const double n = static_cast<double>(g.members.size());
      const double dx = g.x / n - s.s.feet.x, dy = g.y / n - s.s.feet.y, dz = g.z / n - s.s.feet.z;
      if (dx * dx + dy * dy < near2 && dz * dz < 48 * 48) {
        into = &g;
        break;
      }
    }
    if (!into) {
      spots.push_back({s.map, 0, 0, 0, 0, 0, {}});
      into = &spots.back();
    }
    into->x += s.s.feet.x, into->y += s.s.feet.y, into->z += s.s.feet.z;
    const double h = static_cast<double>(s.s.heading) / kDeg;
    into->hx += ge::core::libm::cos(h), into->hy += ge::core::libm::sin(h);
    into->members.push_back(i);
  }
  std::vector<size_t> kept;
  for (size_t g = 0; g < spots.size(); g++) {
    std::vector<int> who;
    for (size_t i : spots[g].members) who.push_back(starts[i].player);
    std::vector<std::string> names;
    for (int w : who) names.push_back(players[static_cast<size_t>(w)].name);
    std::sort(names.begin(), names.end());
    names.erase(std::unique(names.begin(), names.end()), names.end());
    if (static_cast<int>(spots[g].members.size()) >= gold && names.size() >= 2) kept.push_back(g);
  }
  std::sort(kept.begin(), kept.end(), [&](size_t a, size_t b) {
    return spots[a].members.size() != spots[b].members.size()
               ? spots[a].members.size() > spots[b].members.size()
               : a < b;
  });
  gl::LabTable st{
      "the spots: entries within " + to_fixed(1.5 * cell, 0) + " of each other, started " +
          std::to_string(gold) + " times or more by two players or more",
      {"#", "map", "entry (x y z)", "heading", "starts", "players", "amateur", "intermediate",
       "expert", "made %", "stood %", "to target p50 s", "takeoff u/s p50"},
      {}};
  for (size_t r = 0; r < kept.size() && static_cast<int>(r) < rows; r++) {
    const Spot& g = spots[kept[r]];
    const double n = static_cast<double>(g.members.size());
    std::array<int, kLevels> by{};
    Tally t;
    std::vector<std::string> names;
    for (size_t i : g.members) {
      const Player& p = players[static_cast<size_t>(starts[i].player)];
      if (p.level >= 0) by[static_cast<size_t>(p.level)]++;
      names.push_back(p.name);
      t.add(starts[i].s);
    }
    std::sort(names.begin(), names.end());
    names.erase(std::unique(names.begin(), names.end()), names.end());
    const double heading = ge::core::libm::atan2(g.hy, g.hx) * kDeg;
    st.rows.push_back(
        {std::to_string(r + 1), g.map,
         to_fixed(g.x / n, 0) + " " + to_fixed(g.y / n, 0) + " " + to_fixed(g.z / n, 0),
         to_fixed(heading, 0), std::to_string(g.members.size()), std::to_string(names.size()),
         std::to_string(by[0]), std::to_string(by[1]), std::to_string(by[2]),
         to_fixed(100 * share(t.made, t.starts), 0), to_fixed(100 * share(t.stood, t.starts), 0),
         to_fixed(quantile(t.t_target, 0.5f), 2), to_fixed(quantile(t.speed_off, 0.5f), 0)});
  }
  lab.table(st);
  int in_spots = 0;
  for (size_t g : kept) in_spots += static_cast<int>(spots[g].members.size());
  lab.metric("hs.demos", read, "count");
  lab.metric("hs.players", static_cast<double>(players.size()), "count");
  lab.metric("hs.spots", static_cast<double>(kept.size()), "count");
  lab.metric("hs.in-spots-share", share(in_spots, static_cast<int>(starts.size())), "share");

  // The pictures: each map from above, every start from its entry to its end.
  const In in = {{"dir", dir}, {"cell", to_fixed(cell, 0)}, {"target", to_fixed(set.target, 0)}};
  int drawn = 0;
  for (const auto& [name, mp] : maps.by_name) {
    if (drawn++ >= 3) break;
    const std::vector<la::WireEdge> edges = la::wire_edges(mp->map);
    la::WireView v;
    v.fit(la::edge_points(edges));
    gl::Picture p = la::wire_picture(edges, v);
    for (const Start& s : starts) {
      if (s.map != name || s.s.frames.empty()) continue;
      const gl::Rgba c = s.s.made ? gl::Rgba{40, 160, 60, 150} : gl::Rgba{210, 50, 40, 150};
      double px = 0, py = 0, pf = 0;
      v.pixel(s.s.frames.front().feet, px, py, pf);
      gl::fill_rect(p, static_cast<int>(px) - 1, static_cast<int>(py) - 1, 3, 3, c);
      for (size_t f = 1; f < s.s.frames.size(); f++) {
        double qx, qy, qf;
        v.pixel(s.s.frames[f].feet, qx, qy, qf);
        gl::draw_line(p, px, py, qx, qy, c);
        px = qx, py = qy;
      }
    }
    for (size_t r = 0; r < kept.size() && static_cast<int>(r) < rows; r++) {
      const Spot& g = spots[kept[r]];
      if (g.map != name) continue;
      const double n = static_cast<double>(g.members.size());
      double ax, ay, af;
      v.pixel(
          {static_cast<float>(g.x / n), static_cast<float>(g.y / n), static_cast<float>(g.z / n)},
          ax, ay, af);
      const std::string label = std::to_string(r + 1);
      gl::fill_rect(p, static_cast<int>(ax) + 3, static_cast<int>(ay) - 6,
                    gl::text_width(label) + 4, 11, {255, 255, 255, 230});
      gl::draw_text(p, static_cast<int>(ax) + 5, static_cast<int>(ay) - 5, label,
                    {20, 20, 20, 255});
    }
    gl::draw_text(p, 10, 9,
                  "green: a hop start made to " + to_fixed(set.target, 0) +
                      "; red: one that stalled; numbers: the spots, as in their table",
                  {40, 40, 40, 255});
    lab.shot({"starts-" + name, "EVERY HOP START ON " + name + " FROM ABOVE",
              "The map as a wireframe and every hop start read off the recordings as its path "
              "from the entry (a square: the last slow frame on the ground) to its third chained "
              "takeoff or the target speed, green when it reached the target in time, red when "
              "it stalled; the spots people start from again and again numbered.",
              "starts gather where people stop: spawns, items, the foot of stairs and lifts; the "
              "spots are the zones a bot settles in to copy them",
              in},
             p);
  }

  // Every start as a row.
  std::string tsv =
      "map\tdemo\tplayer\tlevel\tt\tx\ty\tz\tspeed_in\tstood\tyaw_in\theading\trun_s\tspeed_off"
      "\tlow\tlag_off\trun_keys\ttakeoffs\tmade\tto_target_s\tspeed_out\ttop\tframes\n";
  for (const Start& s : starts) {
    const Player& p = players[static_cast<size_t>(s.player)];
    const sh::HopStart& h = s.s;
    tsv += s.map + "\t" + std::to_string(s.demo) + "\t" + std::to_string(s.player) + "\t" +
           (p.level >= 0 ? kLevelName[p.level] : "-") + "\t" + to_fixed(h.t, 2) + "\t" +
           to_fixed(h.feet.x, 1) + "\t" + to_fixed(h.feet.y, 1) + "\t" + to_fixed(h.feet.z, 1) +
           "\t" + to_fixed(h.speed_in, 0) + "\t" + (h.stood ? "1" : "0") + "\t" +
           to_fixed(h.yaw_in, 1) + "\t" + to_fixed(h.heading, 1) + "\t" + to_fixed(h.run_s, 3) +
           "\t" + to_fixed(h.speed_off, 0) + "\t" + to_fixed(h.low, 0) + "\t" +
           to_fixed(h.lag_off, 1) + "\t" + sh::keys_name(h.run_keys) + "\t" +
           std::to_string(h.takeoffs) + "\t" + (h.made ? "1" : "0") + "\t" +
           to_fixed(h.to_target_s, 3) + "\t" + to_fixed(h.low_to_target_s, 3) + "\t" +
           to_fixed(h.speed_out, 0) + "\t" + to_fixed(h.top, 0) + "\t" +
           std::to_string(h.frames.size()) + "\n";
  }
  lab.attach("hop_starts.tsv", std::vector<uint8_t>(tsv.begin(), tsv.end()));
  // Every start's every frame (`--frames=<tsv>`): the recipes' frames, for the questions the
  // tables do not answer.
  if (!lab.arg("frames").empty()) {
    std::ofstream fr(lab.arg("frames"));
    fr << "start\tlevel\tt\tx\ty\tz\tvx\tvy\tvz\tyaw\tpitch\tkeys\tjump\tground\n";
    for (size_t i = 0; i < starts.size(); i++) {
      const int lv = players[static_cast<size_t>(starts[i].player)].level;
      for (const sh::RecipeFrame& f : starts[i].s.frames)
        fr << i << '\t' << (lv >= 0 ? kLevelName[lv] : "-") << '\t' << to_fixed(f.t, 3) << '\t'
           << to_fixed(f.feet.x, 1) << '\t' << to_fixed(f.feet.y, 1) << '\t'
           << to_fixed(f.feet.z, 1) << '\t' << to_fixed(f.velocity.x, 1) << '\t'
           << to_fixed(f.velocity.y, 1) << '\t' << to_fixed(f.velocity.z, 1) << '\t'
           << to_fixed(f.yaw, 2) << '\t' << to_fixed(f.pitch, 2) << '\t' << sh::keys_name(f.keys)
           << '\t' << (f.jump ? 1 : 0) << '\t' << (f.ground ? 1 : 0) << '\n';
    }
  }
}

const gl::LabSpec kSpec{
    "se-recipes",
    gl::LabKind::sim,
    "game:shooters_edge",
    "plugins/shooter/recipes.hpp over many demos (games/shooters_edge/labs/se_recipes_lab.cpp, "
    "client/demo_scan.hpp, wire_view.hpp)",
    "bots",
    "Which moves do people make again and again that a bot could copy, read off their recorded "
    "games: how do they start hopping from a stand or a walk (how often, how fast to 400, with "
    "which keys, by level), and from which spots?",
    {
        {"dir", "", "a folder of .mvd demos"},
        {"file", "", "one demo's path (with --dir: read too); a bots' match from se-team --save"},
        {"written", "2", "with neither: how many scripted matches to write in code"},
        {"most", "0", "read at most this many demos of --dir (0: all)"},
        {"basedir", "",
         "a folder holding the game folder the demos' maps are read from (empty: the fixture "
         "halls, for the written matches)"},
        {"game", "id1", "the game folder under --basedir"},
        {"least", "60", "a player is graded with this many seconds of track or more"},
        {"target", "400", "the speed a hop start reaches, units/s"},
        {"within", "2.5", "within this of its entry, s"},
        {"slow", "250", "the entry: the last ground frame slower than this, units/s"},
        {"cell", "96", "starts gather into a spot with entries within 1.5 of this, units"},
        {"gold", "4", "a gathering started this often or more, by two players or more, is a spot"},
        {"rows", "20", "the spots tabled and numbered"},
        {"frames", "",
         "a TSV path: every start's every frame as a row (feet, velocity, view, keys)"},
    },
    "--least=1",
    run,
};
GE_LAB(kSpec);

}  // namespace
