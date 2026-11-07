// LAB `se-rj`: THE ROCKET JUMPS PEOPLE MAKE, read off a folder of recorded games, to be copied by
// the bots. Every demo in `--dir` (or `--file`; by default a few short matches written in code on
// the fixture halls, three players each jumping their own way) is scanned with every player's
// track (client/demo_scan.hpp), each track read as hops on its map's hulls (shooter/hops.hpp) and
// then as rocket jumps by the rockets he fired (shooter/rocket_jumps.hpp: the kind by the view
// against the flight, the pitch, the run, the keys read off the move, the flight and landing).
//
// THE LEVELS: each player of a demo with `--least` seconds of track or more is graded by how fast
// he moves (his track's median speed across, hops.hpp), the slowest third AMATEUR, the middle
// INTERMEDIATE, the fastest third EXPERT, over every player read; each level's jumps are then
// tabled side by side (how often, which kinds, standing or run at, the pitches, the keys at the
// shot, the run's and the flight's key sequences), which is what a bot of that level copies.
//
// THE TRICKS: jumps whose takeoffs and landings lie within one and a half `--cell` of each other's
// gathered (each into the first gathering whose mean takeoff and landing are that near); one
// seen `--gold` times or more, by two players or more, is a TRICK, a rocket jump people make on
// that spot again and again: tabled with its xyz, how it is done (the kind most used, the pitch,
// the run), its jumps by level and its LEVEL (the lowest whose players made a fifth of them or
// more: a bot of that level learns it), and written to tricks.tsv for the nav (a rocket link
// to prove), beside jumps.tsv with every jump.
//
// What it shows: the tables; each map from above and at an angle with every jump as a line from
// takeoff to landing coloured by kind and its tricks thick and numbered. Deterministic: the
// digest holds the counts.
//
// ge:summary Lab: rocket jumps read off many demos: kinds, keys, levels, the maps' tricks
// ge:keywords lab se-rj rocket-jump rj flip tricks trick-spots demos mvd keys levels amateur
// ge:keywords intermediate expert movement-skill copy-movement best-movements analytics
// ge:skill bots labs
// ge:test tests/shooter_rocket_jumps_test.cpp
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "core/math.hpp"
#include "core/num.hpp"
#include "core/num_text.hpp"
#include "core/parallel.hpp"
#include "core/prng.hpp"
#include "lab/lab.hpp"
#include "shooter/hops.hpp"
#include "shooter/rocket_jumps.hpp"
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
constexpr float kNearAnother = 120;  // a rocket gone this near another player was a shot at him
constexpr const char* kLevelName[kLevels] = {"amateur", "intermediate", "expert"};
constexpr gl::Rgba kKindColour[3] = {{230, 120, 30, 255}, {200, 170, 20, 255}, {30, 140, 220, 255}};

struct Jump {
  int demo = 0, slot = 0, player = 0;  // the player: an index into the players read
  std::string map;
  sh::RocketJump j;
};

// An arrival at the asked spot (`--spot`): who, when, and the blasts and takeoff before it.
struct Arrival {
  std::string map, name, how;  // how: his own blasts by him before, in order ("G R": a grenade,
                               // then a rocket), "-" none
  double t = 0;
  sh::Vec3f from;      // where he left the ground last before arriving (the takeoff)
  float rise = 0;      // the most he rose at, units/s, over the window
  float before_s = 0;  // the first blast's time before the arrival, s
};

struct Player {
  int demo = 0, slot = 0;
  std::string name, map;
  double seconds = 0;
  float speed = 0;  // his median speed across, units/s
  int jumps = 0;
  int level = -1;  // -1: too short a track to grade
};

float median(std::vector<float> v) {
  if (v.empty()) return 0;
  std::sort(v.begin(), v.end());
  return v[v.size() / 2];
}

std::string most_of(const std::map<std::string, int>& count, int show = 3) {
  std::vector<std::pair<int, std::string>> v;
  int total = 0;
  for (const auto& [k, n] : count) {
    v.push_back({-n, k});
    total += n;
  }
  std::sort(v.begin(), v.end());
  std::string s;
  for (int i = 0; i < show && i < static_cast<int>(v.size()); i++)
    s += (i > 0 ? ", " : "") + v[static_cast<size_t>(i)].second + " " +
         to_fixed(100.0 * -v[static_cast<size_t>(i)].first / std::max(1, total), 0) + "%";
  return s.empty() ? "-" : s;
}

// THE WRITTEN MATCHES: on the fixture halls' hall (512 by 384, 256 high), three players, each
// with his own jump: "flipper" runs at 300 along +x, turns round into it and fires 55 down behind,
// flying 330 across and 380 up, turning back in the air; "runner" runs at 250 and fires 80 down at
// his feet, flying 250 across and 450 up; "stander" walks in at 80 and fires 88 down standing,
// 500 straight up. Each match moves them all a little (off the seed), so the jumps gather into
// tricks over a few matches.
std::vector<std::vector<uint8_t>> written_matches(int count) {
  std::vector<std::vector<uint8_t>> out;
  ge::core::Rng rng = ge::core::make_rng(91);
  constexpr float kDt = 0.013f, kGravity = 800, kStand = 24;
  constexpr int kFrames = 240;
  for (int m = 0; m < count; m++) {
    cl::MatchScript s;
    s.frame_s = kDt;
    s.frames = kFrames;
    const auto jitter = [&] { return static_cast<float>(ge::core::range(rng, -12.0, 12.0)); };
    struct Plan {
      const char* name;
      sh::Vec3f start;
      float run, shot_x, pitch, back;  // run speed, the shot's x, pitch, view off the run
      sh::Vec3f push;                  // the velocity after the blast
    };
    const Plan plans[3] = {{"flipper", {40, 60, kStand}, 300, 160, 55, 180, {330, 0, 380}},
                           {"runner", {40, 200, kStand}, 250, 150, 80, 0, {250, 0, 450}},
                           {"stander", {260, 320, kStand}, 80, 300, 88, 0, {0, 0, 500}}};
    for (const Plan& pl : plans) {
      cl::MatchPlayer p;
      p.name = pl.name;
      const float dx = jitter(), dy = jitter();
      sh::Vec3f at{pl.start.x + dx, pl.start.y + dy, kStand};
      const float shot_x = pl.shot_x + dx;
      sh::Vec3f v{pl.run, 0, 0};
      int shot = -1;
      float yaw = 0;
      for (int f = 0; f < kFrames; f++) {
        const float t = static_cast<float>(f) * kDt;
        float pitch = 0;
        if (shot < 0) {
          if (t >= 0.3f) at.x += pl.run * kDt;  // standing a moment, then the run
          // The turn into a flip over the last 0.3 s of the run.
          const float left = (shot_x - at.x) / std::max(1.0f, pl.run);
          if (pl.back > 0 && left < 0.3f) yaw = pl.back * (1 - std::max(0.0f, left) / 0.3f);
          if (at.x >= shot_x) {
            shot = f;
            pitch = pl.pitch;
            const float back = pl.back > 0 ? -1.0f : 1.0f;
            cl::MatchRocket rk;
            rk.from = f;
            rk.to = f + 2;
            rk.start = {at.x + back * 6, at.y, at.z + 14};
            rk.end = {at.x + back * (pl.back > 0 ? 30.0f : 6.0f), at.y, 8};
            s.rockets.push_back(rk);
          }
        } else if (f < shot + 2) {
          at.x += pl.run * kDt;  // the run goes on while the rocket flies
        } else if (at.z >= kStand) {
          // The flight from the blast, the view turned back over 0.3 s.
          if (f == shot + 2) v = pl.push;
          at = at + v * kDt;
          v.z -= kGravity * kDt;
          if (at.z < kStand) at.z = kStand, v = {0, 0, 0};
          if (pl.back > 0) yaw = std::max(0.0f, yaw - pl.back * kDt / 0.3f);
        }
        p.origins.push_back(at);
        p.angles.push_back({pitch, yaw, 0});
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

void run(gl::Lab& lab) {
  const double least_s = lab.arg_num("least");
  const double cell = std::max(16.0, lab.arg_num("cell"));
  const int gold = std::max(2, static_cast<int>(lab.arg_num("gold")));
  const int most = static_cast<int>(lab.arg_num("most"));

  std::vector<std::string> paths;
  const std::string dir = lab.arg("dir"), file = lab.arg("file");
  if (!file.empty()) paths.push_back(file);
  if (!dir.empty()) {
    std::error_code ec;
    for (const auto& e : std::filesystem::directory_iterator(dir, ec))
      if (e.is_regular_file()) paths.push_back(e.path().string());
    if (ec) {
      lab.fail("se-rj: the folder " + dir + " does not list: " + ec.message(),
               "next: pass --dir=<a folder of demos>");
      return;
    }
    std::sort(paths.begin() + (file.empty() ? 0 : 1), paths.end());
  }
  if (most > 0 && static_cast<int>(paths.size()) > most) paths.resize(static_cast<size_t>(most));
  const bool written = paths.empty();
  std::vector<std::vector<uint8_t>> made;
  if (written) made = written_matches(static_cast<int>(lab.arg_num("written")));
  const size_t demo_count = written ? made.size() : paths.size();

  // Each demo scanned on its own, side by side, then read in the demos' order.
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

  // The spot asked about: arrivals within `spot-r` across of it (and 48 up or down).
  sh::Vec3f spot;
  const bool asked = !lab.arg("spot").empty() && std::sscanf(lab.arg("spot").c_str(), "%f,%f,%f",
                                                             &spot.x, &spot.y, &spot.z) == 3;
  const float spot_r = static_cast<float>(lab.arg_num("spot-r"));
  const double window_s = lab.arg_num("spot-s");
  const float rise_least = static_cast<float>(lab.arg_num("spot-rise"));
  std::vector<Arrival> arrivals;
  Maps maps{lab.arg("basedir"), lab.arg("game"), {}};
  std::vector<Player> players;
  std::vector<Jump> jumps;
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
    // The rockets each player fired.
    std::array<std::vector<sh::RocketShot>, 32> shots;
    // A rocket gone near another player alive then was a shot at him.
    const auto at_another = [&](const cl::DemoFlight& f) {
      for (size_t k = 0; k < 32; k++) {
        if (static_cast<int>(k) == f.shooter || scan.tracks[k].empty()) continue;
        const std::vector<sh::TrackPoint>& tr = scan.tracks[k];
        const auto it = std::lower_bound(tr.begin(), tr.end(), f.t1,
                                         [](const sh::TrackPoint& p, double t) { return p.t < t; });
        if (it == tr.end() || it->dead || it->t - f.t1 > 0.05) continue;
        const sh::Vec3f d = it->origin - f.to;
        if (d.x * d.x + d.y * d.y + d.z * d.z < kNearAnother * kNearAnother) return true;
      }
      return false;
    };
    for (const cl::DemoFlight& f : scan.flights)
      if (!f.grenade && f.shooter >= 0 && f.shooter < 32)
        shots[static_cast<size_t>(f.shooter)].push_back({f.t0, f.t1, f.from, f.to, at_another(f)});
    std::vector<std::vector<sh::RocketJump>> found(32);
    std::vector<sh::HopReading> hops(32);
    ge::core::parallel_bands(32, 32, [&](size_t b, size_t e) {
      for (size_t k = b; k < e; k++) {
        const std::vector<sh::TrackPoint>& tr = scan.tracks[k];
        if (tr.size() < 8) continue;
        sh::read_hops(tr, *m->world, sh::HopSettings{}, hops[k]);
        sh::read_rocket_jumps(tr, hops[k], shots[k], sh::RocketJumpReadSettings{}, found[k]);
      }
    });
    for (size_t k = 0; k < 32; k++) {
      const std::vector<sh::TrackPoint>& tr = scan.tracks[k];
      if (tr.size() < 8) continue;
      Player p;
      p.demo = static_cast<int>(i);
      p.slot = static_cast<int>(k);
      p.name = scan.names[k].empty() ? "slot " + std::to_string(k) : scan.names[k];
      p.map = scan.map;
      p.seconds = hops[k].summary.seconds;
      p.speed = hops[k].summary.speed_p50;
      p.jumps = static_cast<int>(found[k].size());
      const int at = static_cast<int>(players.size());
      players.push_back(p);
      for (const sh::RocketJump& j : found[k])
        jumps.push_back({static_cast<int>(i), static_cast<int>(k), at, scan.map, j});
    }
    if (asked)
      for (size_t k = 0; k < 32; k++) {
        const std::vector<sh::TrackPoint>& tr = scan.tracks[k];
        double last_in = -1e9;
        for (size_t f = 0; f < tr.size(); f++) {
          const sh::TrackPoint& p = tr[f];
          const sh::Vec3f d = p.origin - spot;
          const bool in =
              !p.dead && d.x * d.x + d.y * d.y <= spot_r * spot_r && d.z <= 48 && d.z >= -48;
          if (!in) continue;
          const bool fresh = p.t - last_in > window_s;
          last_in = p.t;
          if (!fresh || f == 0) continue;
          Arrival a;
          a.map = scan.map;
          a.name = scan.names[k].empty() ? "slot " + std::to_string(k) : scan.names[k];
          a.t = p.t;
          // Back over the window: died or teleported in it (a step longer than 128) ends it.
          size_t first = f;
          while (first > 0 && p.t - tr[first - 1].t <= window_s && !tr[first - 1].dead) {
            const sh::Vec3f s = tr[first].origin - tr[first - 1].origin;
            if (s.x * s.x + s.y * s.y + s.z * s.z > 128 * 128) break;
            first--;
          }
          // The takeoff: the last frame on the ground before he rose into the spot (the
          // ground read off the hops), else the window's start.
          a.from = tr[first].origin;
          for (size_t g = f; g > first; g--) {
            const double dt = tr[g].t - tr[g - 1].t;
            const float vz =
                dt > 0 ? (tr[g].origin.z - tr[g - 1].origin.z) / static_cast<float>(dt) : 0.f;
            if (vz > a.rise) a.rise = vz;
          }
          // With `spot-rise`, only an arrival from below: the takeoff the last frame on the
          // ground that much lower (none in the window: not one).
          bool below = rise_least <= 0;
          for (size_t g = f; g-- > first;)
            if (g < hops[k].frames.size() && hops[k].frames[g].phase == sh::HopPhase::ground &&
                (rise_least <= 0 || tr[g].origin.z <= spot.z - rise_least)) {
              a.from = tr[g].origin;
              below = true;
              break;
            }
          if (!below) continue;
          // His own blasts that went within 160 of him in the window.
          for (const cl::DemoFlight& fl : scan.flights) {
            if (fl.shooter != static_cast<int>(k) || fl.t1 < tr[first].t || fl.t1 > p.t) continue;
            const auto it =
                std::lower_bound(tr.begin(), tr.end(), fl.t1,
                                 [](const sh::TrackPoint& q, double t) { return q.t < t; });
            if (it == tr.end()) continue;
            const sh::Vec3f e = it->origin - fl.to;
            if (e.x * e.x + e.y * e.y + e.z * e.z > 160 * 160) continue;
            if (a.how.empty()) a.before_s = static_cast<float>(p.t - fl.t1);
            a.how += a.how.empty() ? "" : " ";
            a.how += fl.grenade ? "G" : "R";
          }
          if (a.how.empty()) a.how = "-";
          arrivals.push_back(a);
        }
      }
    sc.scan.tracks = {};  // the tracks are read; free them
  }
  if (read == 0) {
    lab.fail("se-rj: no demo read with its map",
             "next: pass --dir= a folder of demos and --basedir= the game folder holding their "
             "maps (written matches need no map)");
    return;
  }

  // The levels: the graded players' speeds in thirds.
  std::vector<std::pair<float, int>> by_speed;
  for (size_t p = 0; p < players.size(); p++)
    if (players[p].seconds >= least_s) by_speed.push_back({players[p].speed, static_cast<int>(p)});
  std::sort(by_speed.begin(), by_speed.end());
  for (size_t r = 0; r < by_speed.size(); r++)
    players[static_cast<size_t>(by_speed[r].second)].level =
        static_cast<int>(r * kLevels / by_speed.size());
  const auto level_of = [&](const Jump& j) { return players[static_cast<size_t>(j.player)].level; };

  const In in = {{"demos", written ? std::to_string(demo_count) + " written in code" : dir + file},
                 {"read", std::to_string(read)},
                 {"least", lab.arg("least") + " s of track to be graded"},
                 {"cell", lab.arg("cell")},
                 {"gold", std::to_string(gold)}};

  // Every jump by kind, run at or standing.
  gl::LabTable kinds{
      "every rocket jump by kind (the view at the shot against the flight) and run at or standing",
      {"kind", "jumps", "share", "pitch", "run u/s", "after u/s", "rise u/s", "height", "across",
       "air s", "flip back s"},
      {}};
  for (int kind = 0; kind < 3; kind++)
    for (int standing = 0; standing < 2; standing++) {
      std::vector<float> pitch, run, after, rise, height, across, air, back;
      for (const Jump& j : jumps) {
        if (static_cast<int>(j.j.kind) != kind || j.j.standing != (standing == 1)) continue;
        pitch.push_back(j.j.pitch);
        run.push_back(j.j.run_speed);
        after.push_back(j.j.speed_after);
        rise.push_back(j.j.rise);
        height.push_back(j.j.height);
        across.push_back(j.j.across);
        air.push_back(j.j.air_s);
        if (j.j.turn_back_s >= 0) back.push_back(j.j.turn_back_s);
      }
      if (pitch.empty()) continue;
      kinds.rows.push_back(
          {std::string(sh::rocket_jump_kind_name(static_cast<sh::RocketJumpKind>(kind))) +
               (standing ? " standing" : " run at"),
           std::to_string(pitch.size()),
           to_fixed(100.0 * static_cast<double>(pitch.size()) / std::max<size_t>(1, jumps.size()),
                    0) +
               "%",
           to_fixed(median(pitch), 0), to_fixed(median(run), 0), to_fixed(median(after), 0),
           to_fixed(median(rise), 0), to_fixed(median(height), 0), to_fixed(median(across), 0),
           to_fixed(median(air), 2), back.empty() ? "-" : to_fixed(median(back), 2)});
    }

  // Each level side by side.
  gl::LabTable levels{"each level's rocket jumps: what a bot of that level copies",
                      {"level", "players", "minutes", "jumps", "a 10 min", "feet", "side", "flip",
                       "standing", "flip pitch", "feet pitch", "run u/s", "keys at the shot",
                       "the run's keys", "the flight's keys"},
                      {}};
  for (int lv = 0; lv < kLevels; lv++) {
    int np = 0, nj = 0, kind[3] = {}, standing = 0;
    double seconds = 0;
    std::vector<float> flip_pitch, feet_pitch, run;
    std::map<std::string, int> at_shot, run_keys, air_keys;
    for (const Player& p : players)
      if (p.level == lv) np++, seconds += p.seconds;
    for (const Jump& j : jumps) {
      if (level_of(j) != lv) continue;
      nj++;
      kind[static_cast<int>(j.j.kind)]++;
      standing += j.j.standing;
      (j.j.kind == sh::RocketJumpKind::flip ? flip_pitch : feet_pitch).push_back(j.j.pitch);
      if (!j.j.standing) run.push_back(j.j.run_speed);
      at_shot[sh::keys_name(j.j.keys_shot)]++;
      run_keys[sh::sequence_name(j.j.keys_run)]++;
      air_keys[sh::sequence_name(j.j.keys_air)]++;
    }
    const auto share = [&](int n) { return to_fixed(100.0 * n / std::max(1, nj), 0) + "%"; };
    levels.rows.push_back({kLevelName[lv], std::to_string(np), to_fixed(seconds / 60, 1),
                           std::to_string(nj), to_fixed(seconds > 0 ? nj * 600.0 / seconds : 0, 1),
                           share(kind[0]), share(kind[1]), share(kind[2]), share(standing),
                           flip_pitch.empty() ? "-" : to_fixed(median(flip_pitch), 0),
                           feet_pitch.empty() ? "-" : to_fixed(median(feet_pitch), 0),
                           run.empty() ? "-" : to_fixed(median(run), 0), most_of(at_shot),
                           most_of(run_keys, 2), most_of(air_keys, 2)});
    lab.metric("rj.level." + std::string(kLevelName[lv]) + ".jumps", nj, "count");
    lab.metric("rj.level." + std::string(kLevelName[lv]) + ".flip-share",
               nj > 0 ? static_cast<double>(kind[2]) / nj : 0, "share");
  }

  // The tricks: takeoffs and landings near each other again and again, by two players or more.
  // A jump joins the first trick (in the order made) whose mean takeoff and mean landing both lie
  // within `reach` of its own; else it starts one.
  struct Trick {
    std::string map;
    std::vector<int> jumps;
    sh::Vec3f from_sum, to_sum;
  };
  const float reach = static_cast<float>(cell * 1.5);
  const auto near = [&](sh::Vec3f a, sh::Vec3f b) {
    const sh::Vec3f d = a - b;
    return d.x * d.x + d.y * d.y + d.z * d.z <= reach * reach;
  };
  std::vector<Trick> all;
  for (size_t k = 0; k < jumps.size(); k++) {
    const Jump& j = jumps[k];
    if (j.j.land < 0) continue;
    Trick* in_one = nullptr;
    for (Trick& t : all) {
      const float inv = 1.0f / static_cast<float>(t.jumps.size());
      if (t.map == j.map && near(t.from_sum * inv, j.j.from) && near(t.to_sum * inv, j.j.to)) {
        in_one = &t;
        break;
      }
    }
    if (!in_one) {
      all.push_back({j.map, {}, {}, {}});
      in_one = &all.back();
    }
    in_one->jumps.push_back(static_cast<int>(k));
    in_one->from_sum = in_one->from_sum + j.j.from;
    in_one->to_sum = in_one->to_sum + j.j.to;
  }
  std::vector<Trick> tricks;
  for (const Trick& t : all) {
    std::vector<std::pair<int, int>> who;
    who.reserve(t.jumps.size());
    for (const int k : t.jumps)
      who.push_back({jumps[static_cast<size_t>(k)].demo, jumps[static_cast<size_t>(k)].slot});
    std::sort(who.begin(), who.end());
    who.erase(std::unique(who.begin(), who.end()), who.end());
    if (static_cast<int>(t.jumps.size()) >= gold && who.size() >= 2) tricks.push_back(t);
  }
  std::sort(tricks.begin(), tricks.end(), [](const Trick& a, const Trick& b) {
    return a.jumps.size() != b.jumps.size() ? a.jumps.size() > b.jumps.size() : a.jumps < b.jumps;
  });
  const int rows = static_cast<int>(lab.arg_num("rows"));
  gl::LabTable tt{"the tricks: rocket jumps made on the same spot again and again, most first",
                  {"#", "map", "from (x y z)", "to (x y z)", "jumps", "kind", "standing", "pitch",
                   "run u/s", "height", "across", "by level", "level"},
                  {}};
  std::string tsv =
      "map\tfrom_x\tfrom_y\tfrom_z\tto_x\tto_y\tto_z\tjumps\tkind\tstanding\tpitch\trun\t"
      "amateur\tintermediate\texpert\tlevel\n";
  const auto v3 = [](sh::Vec3f v) {
    return to_fixed(v.x, 0) + " " + to_fixed(v.y, 0) + " " + to_fixed(v.z, 0);
  };
  std::vector<std::pair<sh::Vec3f, sh::Vec3f>> trick_ends;
  for (size_t n = 0; n < tricks.size(); n++) {
    const Trick& t = tricks[n];
    sh::Vec3f from, to;
    std::vector<float> pitch, run, height, across;
    int kind[3] = {}, standing = 0, by_level[kLevels] = {}, graded = 0;
    for (const int k : t.jumps) {
      const Jump& j = jumps[static_cast<size_t>(k)];
      from = from + j.j.from;
      to = to + j.j.to;
      pitch.push_back(j.j.pitch);
      if (!j.j.standing) run.push_back(j.j.run_speed);
      height.push_back(j.j.height);
      across.push_back(j.j.across);
      kind[static_cast<int>(j.j.kind)]++;
      standing += j.j.standing;
      if (level_of(j) >= 0) by_level[level_of(j)]++, graded++;
    }
    // Its LEVEL: the lowest whose players made a fifth of its graded jumps or more (a bot of that
    // level learns it).
    int level = kLevels - 1;
    for (int lv = kLevels - 1; lv >= 0; lv--)
      if (graded > 0 && by_level[lv] * 5 >= graded) level = lv;
    const std::string counts = std::to_string(by_level[0]) + "/" + std::to_string(by_level[1]) +
                               "/" + std::to_string(by_level[2]);
    const float inv = 1.0f / static_cast<float>(t.jumps.size());
    from = from * inv;
    to = to * inv;
    trick_ends.push_back({from, to});
    const int top = kind[2] >= kind[0] && kind[2] >= kind[1] ? 2 : kind[1] > kind[0] ? 1 : 0;
    const std::string kname = sh::rocket_jump_kind_name(static_cast<sh::RocketJumpKind>(top));
    const std::string st =
        to_fixed(100.0 * standing / static_cast<double>(t.jumps.size()), 0) + "%";
    if (static_cast<int>(n) < rows)
      tt.rows.push_back({std::to_string(n + 1), t.map, v3(from), v3(to),
                         std::to_string(t.jumps.size()), kname, st, to_fixed(median(pitch), 0),
                         run.empty() ? "-" : to_fixed(median(run), 0), to_fixed(median(height), 0),
                         to_fixed(median(across), 0), counts, kLevelName[level]});
    tsv += t.map + "\t" + to_fixed(from.x, 0) + "\t" + to_fixed(from.y, 0) + "\t" +
           to_fixed(from.z, 0) + "\t" + to_fixed(to.x, 0) + "\t" + to_fixed(to.y, 0) + "\t" +
           to_fixed(to.z, 0) + "\t" + std::to_string(t.jumps.size()) + "\t" + kname + "\t" +
           to_fixed(static_cast<float>(standing) * inv, 2) + "\t" + to_fixed(median(pitch), 0) +
           "\t" + (run.empty() ? "0" : to_fixed(median(run), 0)) + "\t" +
           std::to_string(by_level[0]) + "\t" + std::to_string(by_level[1]) + "\t" +
           std::to_string(by_level[2]) + "\t" + kLevelName[level] + "\n";
  }

  // The best jumps: the fastest across among those landed.
  std::vector<int> best;
  for (size_t k = 0; k < jumps.size(); k++)
    if (jumps[k].j.land >= 0 && jumps[k].j.air_s > 0.2f) best.push_back(static_cast<int>(k));
  const auto pace = [&](int k) {
    const sh::RocketJump& j = jumps[static_cast<size_t>(k)].j;
    return j.across / j.air_s;
  };
  std::sort(best.begin(), best.end(),
            [&](int a, int b) { return pace(a) != pace(b) ? pace(a) > pace(b) : a < b; });
  gl::LabTable bt{"the best jumps: the fastest across landed (units a second of flight)",
                  {"map", "player", "level", "kind", "pitch", "run u/s", "the run's keys",
                   "at the shot", "the flight's keys", "height", "across", "air s", "pace"},
                  {}};
  for (int r = 0; r < rows && r < static_cast<int>(best.size()); r++) {
    const Jump& j = jumps[static_cast<size_t>(best[static_cast<size_t>(r)])];
    const Player& p = players[static_cast<size_t>(j.player)];
    bt.rows.push_back(
        {j.map, p.name, p.level >= 0 ? kLevelName[p.level] : "-",
         std::string(sh::rocket_jump_kind_name(j.j.kind)) + (j.j.standing ? " standing" : ""),
         to_fixed(j.j.pitch, 0), to_fixed(j.j.run_speed, 0), sh::sequence_name(j.j.keys_run),
         sh::keys_name(j.j.keys_shot), sh::sequence_name(j.j.keys_air), to_fixed(j.j.height, 0),
         to_fixed(j.j.across, 0), to_fixed(j.j.air_s, 2),
         to_fixed(pace(best[static_cast<size_t>(r)]), 0)});
  }

  std::string jtsv =
      "map\tplayer\tlevel\tkind\tstanding\tpitch\trun\tkeys_run\tkeys_shot\tkeys_air\t"
      "from_x\tfrom_y\tfrom_z\tto_x\tto_y\tto_z\theight\tacross\tair\n";
  for (const Jump& j : jumps) {
    const Player& p = players[static_cast<size_t>(j.player)];
    std::string name = p.name;
    for (char& c : name)
      if (c == ' ' || c == '\t') c = '_';
    jtsv += j.map + "\t" + name + "\t" + (p.level >= 0 ? kLevelName[p.level] : "-") + "\t" +
            sh::rocket_jump_kind_name(j.j.kind) + "\t" + (j.j.standing ? "1" : "0") + "\t" +
            to_fixed(j.j.pitch, 0) + "\t" + to_fixed(j.j.run_speed, 0) + "\t" +
            sh::sequence_name(j.j.keys_run) + "\t" + sh::keys_name(j.j.keys_shot) + "\t" +
            sh::sequence_name(j.j.keys_air) + "\t" + v3(j.j.from) + "\t" + v3(j.j.to) + "\t" +
            to_fixed(j.j.height, 0) + "\t" + to_fixed(j.j.across, 0) + "\t" +
            to_fixed(j.j.air_s, 2) + "\n";
  }
  for (char& c : jtsv)
    if (c == ' ') c = '\t';

  // Each map's picture: every jump from takeoff to landing, the tricks thick and numbered.
  std::vector<std::string> map_names;
  map_names.reserve(maps.by_name.size());
  for (const auto& [name, m] : maps.by_name) map_names.push_back(name);
  for (size_t mi = 0; mi < map_names.size() && mi < 3; mi++) {
    const std::string& name = map_names[mi];
    const la::StudyMap& m = *maps.by_name[name];
    const std::vector<la::WireEdge> edges = la::wire_edges(m.map);
    for (const double elevation : {90.0, 35.0}) {
      la::WireView v;
      v.elevation = elevation;
      v.yaw = elevation == 90 ? 0 : lab.arg_num("yaw");
      v.fit(la::edge_points(edges));
      gl::Picture p = la::wire_picture(edges, v);
      for (const Jump& j : jumps) {
        if (j.map != name) continue;
        double ax, ay, af, bx, by, bf;
        v.pixel(j.j.from, ax, ay, af);
        v.pixel(j.j.to, bx, by, bf);
        const gl::Rgba c = kKindColour[static_cast<int>(j.j.kind)];
        gl::draw_line(p, ax, ay, bx, by, {c.r, c.g, c.b, 140});
        gl::fill_rect(p, static_cast<int>(ax) - 1, static_cast<int>(ay) - 1, 3, 3, c);
      }
      for (size_t n = 0; n < tricks.size() && static_cast<int>(n) < rows; n++) {
        if (tricks[n].map != name) continue;
        double ax, ay, af, bx, by, bf;
        v.pixel(trick_ends[n].first, ax, ay, af);
        v.pixel(trick_ends[n].second, bx, by, bf);
        for (int o = -1; o <= 1; o++) {
          gl::draw_line(p, ax + o, ay, bx + o, by, {20, 20, 20, 255});
          gl::draw_line(p, ax, ay + o, bx, by + o, {20, 20, 20, 255});
        }
        const std::string s = std::to_string(n + 1);
        gl::fill_rect(p, static_cast<int>(bx) + 3, static_cast<int>(by) - 6, gl::text_width(s) + 4,
                      11, {255, 255, 255, 230});
        gl::draw_text(p, static_cast<int>(bx) + 5, static_cast<int>(by) - 5, s, {20, 20, 20, 255});
      }
      int x = 10;
      for (int k = 0; k < 3; k++) {
        const std::string s = sh::rocket_jump_kind_name(static_cast<sh::RocketJumpKind>(k));
        gl::fill_rect(p, x, 8, 10, 10, kKindColour[k]);
        gl::draw_text(p, x + 14, 9, s, {40, 40, 40, 255});
        x += 14 + gl::text_width(s) + 14;
      }
      gl::draw_text(p, x, 9, "black: the tricks, numbered as in their table", {40, 40, 40, 255});
      lab.shot({"jumps-" + name + (elevation == 90 ? "-above" : "-angle"),
                "EVERY ROCKET JUMP ON " + name + (elevation == 90 ? " FROM ABOVE" : " AT AN ANGLE"),
                "The map as a wireframe and every rocket jump read off the recordings as a line "
                "from where the player stood at the shot to where he landed, coloured by its kind "
                "(the view at the shot against the flight: at the feet, to a side, the flip), a "
                "square at the takeoff; the tricks (the same takeoff and landing again and again) "
                "thick and black, numbered as in their table.",
                "the lines gather on the few ledges and items a rocket jump reaches; the tricks "
                "are the ones a bot should learn first",
                in},
               p);
    }
  }

  int flips = 0;
  for (const Jump& j : jumps) flips += j.j.kind == sh::RocketJumpKind::flip;
  lab.metric("rj.demos", read, "count");
  lab.metric("rj.players", static_cast<double>(players.size()), "count");
  lab.metric("rj.jumps", static_cast<double>(jumps.size()), "count");
  lab.metric("rj.flip-share",
             jumps.empty() ? 0 : static_cast<double>(flips) / static_cast<double>(jumps.size()),
             "share");
  lab.metric("rj.tricks", static_cast<double>(tricks.size()), "count");
  lab.table(kinds);
  lab.table(levels);
  lab.table(tt);
  lab.table(bt);
  if (asked) {
    // The arrivals at the spot, by how (his own blasts before) and each one.
    std::map<std::string, int> by_how;
    for (const Arrival& a : arrivals) by_how[a.how]++;
    gl::LabTable ht{"how people arrived at the spot (" + lab.arg("spot") +
                        "): their own blasts "
                        "by them in the " +
                        lab.arg("spot-s") + " s before (R a rocket, G a grenade)",
                    {"blasts", "arrivals"},
                    {}};
    for (const auto& [how, n] : by_how) ht.rows.push_back({how, std::to_string(n)});
    gl::LabTable at{
        "each arrival at the spot",
        {"map", "player", "t s", "blasts", "first before s", "takeoff (x y z)", "most rise u/s"},
        {}};
    for (const Arrival& a : arrivals)
      at.rows.push_back(
          {a.map, a.name, to_fixed(a.t, 1), a.how, a.how == "-" ? "-" : to_fixed(a.before_s, 2),
           to_fixed(a.from.x, 0) + " " + to_fixed(a.from.y, 0) + " " + to_fixed(a.from.z, 0),
           to_fixed(a.rise, 0)});
    lab.metric("rj.spot.arrivals", static_cast<double>(arrivals.size()), "count");
    lab.table(ht);
    lab.table(at);
  }
  lab.attach("tricks.tsv", std::vector<uint8_t>(tsv.begin(), tsv.end()));
  lab.attach("jumps.tsv", std::vector<uint8_t>(jtsv.begin(), jtsv.end()));
}

const gl::LabSpec kSpec{
    "se-rj",
    gl::LabKind::sim,
    "game:shooters_edge",
    "plugins/shooter/rocket_jumps.hpp over many demos (games/shooters_edge/labs/se_rj_lab.cpp, "
    "client/demo_scan.hpp, wire_view.hpp)",
    "bots",
    "How do people rocket jump, read off their recorded games: which kinds, at which pitch, run "
    "at or standing, with which keys, by players of each level, and which spots of each map are "
    "rocket-jumped again and again (the tricks a bot should learn)?",
    {
        {"dir", "", "a folder of demos (any format, plain or gzip); other files skipped"},
        {"file", "", "one demo's path (with --dir: read too)"},
        {"written", "4", "with neither: how many scripted matches to write in code"},
        {"most", "0", "read at most this many demos of --dir (0: all)"},
        {"basedir", "",
         "a folder holding the game folder the demos' maps are read from (empty: "
         "the fixture halls, for the written matches)"},
        {"game", "id1", "the game folder under --basedir"},
        {"least", "60", "a player is graded with this many seconds of track or more"},
        {"cell", "64",
         "jumps gather into a trick with takeoffs and landings within 1.5 of this, units"},
        {"gold", "3", "a gathering jumped this often or more, by two players or more, is a trick"},
        {"rows", "20", "the tricks and best jumps tabled"},
        {"yaw", "30", "the angled views' turn about z, degrees"},
        {"spot", "",
         "x,y,z: how people got there (every arrival, his own blasts before, the takeoff)"},
        {"spot-r", "48", "an arrival is within this across of --spot, units"},
        {"spot-s", "6", "the blasts and the takeoff looked for this long before, s"},
        {"spot-rise", "0",
         "only arrivals from this much lower or more (the takeoff the last ground there), units"},
    },
    "--least=1",
    run,
};
GE_LAB(kSpec);

}  // namespace
