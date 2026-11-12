// LAB `se-aimlead`: HOW WELL THE AIM HELP LEADS A MISSILE ONTO REAL PLAYERS
// (plugins/shooter/aim_help.hpp `aim_lead`, plugins/shooter/aim_path.hpp). Recorded matches
// (`--demo`: multiview recordings, a file or a folder of them) give every player's true path. At
// moments `--every` seconds apart, each living player is a target, and the `--shooters` nearest
// other living players `--near`..`--far` units away fire at him from where they stand: a rocket, a
// grenade and a nail (the nailgun's and the super nailgun's fly alike). The shooter aims at the
// target's middle; the aim help leads the missile by the target's motion read off his path so far
// by one rule (`PathGuess`; `last/arc@150~0.6` is the server's own, `lead_mover`), and the
// missile then flies against where the target REALLY went. A hit is the missile's flight passing
// through his box before it is spent (`kFlight`). Walls are not read: a pair across a wall still
// counts, which is fair to every rule alike; and a missile is spent once it is `kPast` units past
// him (a missed rocket goes on into the wall behind him, not round the map).
//
// The table and the charts give each rule's hit rate by weapon, and the median miss (the closest
// the missile came to his box); the best rule is the one the server should lead with. With no
// `--demo` the paths are written in code (a circle run, a strafing zig-zag, bunny hops, a player
// who stops and turns), so the smoke run and its digest need no recording.
//
// ge:summary Lab: missile lead rules scored by hit rate against players' real paths in recordings
// ge:keywords lab se-aimlead aim-lead lead prediction path window mean hit-rate rocket grenade
// ge:keywords nail mvd recording demo aim-help
// ge:skill shooter labs
// ge:test tests/shooter_aim_path_test.cpp
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "arena_chart.hpp"
#include "core/libm/libm.hpp"
#include "core/num_text.hpp"
#include "lab/chart.hpp"
#include "lab/lab.hpp"
#include "shooter/aim_help.hpp"
#include "shooter/aim_path.hpp"
#include "shooters_edge/client/demo.hpp"

namespace {

namespace gl = ge::lab;
namespace sh = ge::shooter;
namespace cl = shooters_edge::client;
namespace la = shooters_edge::labs;
using ge::core::num::to_fixed;

constexpr float kGravity = 800;
constexpr float kJump = 200;  // a player moving more than this in one frame was teleported
const sh::Vec3f kMins{-16, -16, -24}, kMaxs{16, 16, 32};
constexpr float kEye = 22;
constexpr float kRunning = 200;  // units/s across: a running target, tallied apart
constexpr float kPast = 64;      // a missile this far past him across the floor is spent, units

struct Weapon {
  const char* name;
  float speed, up, gravity, flight;  // units/s along the aim, units/s up, the fall, s it lasts
};
constexpr Weapon kWeapons[] = {
    {"rl", 1000, 0, 0, 3},
    {"gl", 600, 200, kGravity, 2.5f},
    {"ng/sng", 1000, 0, 0, 3},
};

struct Rule {
  sh::PathRule rule;
  std::string name;
  float reach = 0;  // s: the lead across stops this far ahead (0: never), `AimMover::reach`
};

// One stretch of a player's life: no death, no teleport.
using Segment = std::vector<sh::PathPoint>;
struct Player {
  std::vector<Segment> segments;
  int game = 0;  // the recording he played in: shooters are only his fellow players
};

// --- the paths ---

void add_point(Player& p, bool& open, float t, sh::Vec3f o, bool alive) {
  if (!alive) {
    open = false;
    return;
  }
  if (open && !p.segments.back().empty()) {
    const sh::Vec3f d = o - p.segments.back().back().origin;
    if (sh::length(d) > kJump) open = false;
    if (t <= p.segments.back().back().t) return;
  }
  if (!open) {
    p.segments.emplace_back();
    open = true;
  }
  p.segments.back().push_back({t, o});
}

bool read_demo(const std::string& path, std::vector<Player>& out, std::string& map,
               std::string& error) {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    error = path + " does not open";
    return false;
  }
  std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(in)), {});
  cl::Demo d;
  if (!cl::open_demo(std::move(bytes), d, error)) return false;
  if (d.state.format != cl::DemoFormat::multiview) {
    error = path + " is not a multiview recording (only those hold every player's path)";
    return false;
  }
  std::vector<Player> players(32);
  std::vector<bool> open(32, false);
  for (;;) {
    const cl::DemoRead r = cl::next_frame(d, error);
    if (r == cl::DemoRead::error) return false;
    if (r == cl::DemoRead::end) break;
    if (d.state.intermission) break;
    const auto t = static_cast<float>(d.state.time);
    const size_t slots = std::min<size_t>(d.state.players.size(), 32);
    for (size_t s = 0; s < slots; s++) {
      const cl::DemoEntity& e = d.state.players[s];
      const bool here = !d.state.names[s].empty() && !d.state.spectator[s] &&
                        (e.model != 0 || e.present) && !e.dead && !e.gib;
      bool o = open[s];
      add_point(players[s], o, t, e.origin, here);
      open[s] = o;
    }
  }
  if (map.empty()) map = d.state.map;
  for (Player& p : players)
    if (!p.segments.empty()) out.push_back(std::move(p));
  error.clear();
  return true;
}

// Four players written in code, 77 frames a second for `seconds`.
std::vector<Player> written_paths(float seconds) {
  std::vector<Player> out(5);
  std::vector<bool> open(5, false);
  const float dt = 1.0f / 77;
  float hop_z = 0, hop_vz = 0;
  sh::Vec3f hop{0, -900, 24};
  float stop_heading = 0;
  sh::Vec3f stopper{-500, 300, 24};
  for (int i = 0; static_cast<float>(i) * dt < seconds; i++) {
    const float t = static_cast<float>(i) * dt;
    // A circle run, 320 units/s on a 300-unit circle.
    const double a = double(t) * 320 / 300;
    const sh::Vec3f circle{float(300 * ge::core::libm::cos(a)), float(300 * ge::core::libm::sin(a)),
                           24};
    // A zig-zag: 300 forward round a 700-unit loop, swaying 180 units in and out every 0.6 s.
    const float leg = t / 0.6f;
    const auto n = static_cast<int>(leg);
    const float side = (n % 2 == 0 ? (leg - float(n)) : 1 - (leg - float(n))) * 180;
    const double a2 = double(t) * 300 / 700;
    const sh::Vec3f zag{float((610 + side) * ge::core::libm::cos(a2)),
                        float((610 + side) * ge::core::libm::sin(a2)), 24};
    // Bunny hops: 420 across, turning slowly; a jump of 270 up each landing.
    hop_vz -= kGravity * dt;
    hop_z += hop_vz * dt;
    if (hop_z <= 0) {
      hop_z = 0;
      hop_vz = 270;
    }
    const double h = double(t) * 0.4;
    hop.x += float(420 * ge::core::libm::cos(h)) * dt;
    hop.y += float(420 * ge::core::libm::sin(h)) * dt;
    // A stopper: runs 1 s at 320, stands 0.5 s, turns 100 degrees, again.
    const float phase = t - 1.5f * float(int(t / 1.5f));
    if (phase < dt) stop_heading += 100;
    if (phase < 1) {
      const double r = double(stop_heading) * 3.141592653589793 / 180;
      stopper.x += float(320 * ge::core::libm::cos(r)) * dt;
      stopper.y += float(320 * ge::core::libm::sin(r)) * dt;
    }
    // A pacer: 320 straight along x, turning back every 4 s.
    const float lap = t - 8 * float(int(t / 8));
    const sh::Vec3f pacer{-640 + 320 * (lap < 4 ? lap : 8 - lap), 600, 24};
    const sh::Vec3f at[5] = {circle, zag, {hop.x, hop.y, 24 + hop_z}, stopper, pacer};
    for (size_t p = 0; p < 5; p++) {
      bool o = open[p];
      add_point(out[p], o, t, at[p], true);
      open[p] = o;
    }
  }
  return out;
}

// --- the rules ---

// `text` as rules ("still,last,fade:0.5@250"); false with the word it could not read in `bad`.
bool parse_rules(const std::string& text, std::vector<Rule>& out, std::string& bad) {
  size_t at = 0;
  while (at <= text.size()) {
    const size_t end = std::min(text.find(',', at), text.size());
    const std::string word = text.substr(at, end - at);
    at = end + 1;
    if (word.empty()) continue;
    std::string head = word;
    float run = 0, window = 0;
    bool level = false, arc = false;
    float reach = 0;
    if (const size_t k = head.find('~'); k != std::string::npos) {
      reach = std::strtof(head.c_str() + k + 1, nullptr);
      head.resize(k);
    }
    if (const size_t k = head.find("/arc"); k != std::string::npos) {
      arc = true;
      head.erase(k, 4);
    }
    if (const size_t k = head.find("/level"); k != std::string::npos) {
      level = true;
      head.erase(k, 6);
    }
    if (const size_t k = head.find('@'); k != std::string::npos) {
      run = std::strtof(head.c_str() + k + 1, nullptr);
      head.resize(k);
    }
    std::string kind = head;
    if (const size_t k = head.find(':'); k != std::string::npos) {
      window = std::strtof(head.c_str() + k + 1, nullptr);
      kind.resize(k);
    }
    sh::PathGuess g;
    std::string name;
    if (kind == "still") {
      g = sh::PathGuess::still;
      name = "still";
    } else if (kind == "last") {
      g = sh::PathGuess::last;
      name = "last step";
    } else if (kind == "mean" && window > 0) {
      g = sh::PathGuess::mean;
      name = "mean " + to_fixed(window, 2) + " s";
    } else if (kind == "across" && window > 0) {
      g = sh::PathGuess::mean_flat;
      name = "mean across " + to_fixed(window, 2) + " s";
    } else if (kind == "fade" && window > 0) {
      g = sh::PathGuess::fading;
      name = "fading " + to_fixed(window, 2) + " s";
    } else if (kind == "smooth" && window > 0) {
      g = sh::PathGuess::smoothed;
      name = "smoothed " + to_fixed(window, 2) + " s";
    } else if (kind == "accel" && window > 0) {
      g = sh::PathGuess::speeding;
      name = "speeding " + to_fixed(window, 2) + " s";
    } else if (kind == "sway" && window > 0) {
      g = sh::PathGuess::sway_centre;
      name = "sway centre " + to_fixed(window, 2) + " s";
    } else if (kind == "turn" && window > 0) {
      g = sh::PathGuess::turning;
      name = "turning " + to_fixed(window, 2) + " s";
    } else {
      bad = word;
      return false;
    }
    if (run > 0) name += " over " + to_fixed(run, 0);
    if (level) name += " level";
    if (arc) name += " arc";
    if (reach > 0) name += " to " + to_fixed(reach, 2) + " s";
    out.push_back({{g, window, run, level, arc}, name, reach});
  }
  return !out.empty();
}

// --- the shots ---

// Where segment `s` holds time `t`, or null.
const Segment* holding(const Player& p, float t, float before, float after) {
  for (const Segment& s : p.segments)
    if (!s.empty() && s.front().t <= t - before && s.back().t >= t + after) return &s;
  return nullptr;
}

// Whether the flight `a`..`b` passes through the box `lo`..`hi`.
bool crosses(sh::Vec3f a, sh::Vec3f b, sh::Vec3f lo, sh::Vec3f hi) {
  float t0 = 0, t1 = 1;
  for (int k = 0; k < 3; k++) {
    const float d = b[k] - a[k];
    if (d == 0) {
      if (a[k] < lo[k] || a[k] > hi[k]) return false;
      continue;
    }
    float u = (lo[k] - a[k]) / d, v = (hi[k] - a[k]) / d;
    if (u > v) std::swap(u, v);
    t0 = std::max(t0, u);
    t1 = std::min(t1, v);
    if (t0 > t1) return false;
  }
  return true;
}

float box_distance(sh::Vec3f p, sh::Vec3f lo, sh::Vec3f hi) {
  sh::Vec3f d;
  for (int k = 0; k < 3; k++) d[k] = std::max({lo[k] - p[k], 0.0f, p[k] - hi[k]});
  return sh::length(d);
}

constexpr float kBands[] = {150, 400, 700, 1000, 1500, 1e9f};  // the shooter's distance, units
constexpr size_t kBandCount = std::size(kBands) - 1;

struct Tally {
  int shots = 0, hits = 0;
  int band_shots[kBandCount] = {}, band_hits[kBandCount] = {};
  int air_shots = 0, air_hits = 0;
  int run_shots = 0, run_hits = 0;  // the target running across at kRunning or more
  std::vector<float> misses;
};

// One missile fired at `target` (his segment `seg`) at `t0` from `eye` led by `lead`: hit, miss.
bool fire(const Weapon& w, sh::Vec3f eye, const Segment& seg, float t0, const Rule& lead,
          float& miss, bool& airborne, bool& running) {
  std::vector<sh::PathPoint> past;
  for (const sh::PathPoint& p : seg)
    if (p.t <= t0 && p.t >= t0 - 2) past.push_back(p);
  const sh::PathMotion m = sh::read_path(past, lead.rule);
  const sh::PathMotion seen = sh::read_path(past, {sh::PathGuess::last, 0});
  airborne = seen.airborne;
  running =
      seen.velocity.x * seen.velocity.x + seen.velocity.y * seen.velocity.y >= kRunning * kRunning;
  const sh::Vec3f now = past.back().origin;
  const sh::Vec3f from = now + m.shift;
  const sh::AimMover mover{.ent = 1,
                           .absmin = from + kMins,
                           .absmax = from + kMaxs,
                           .velocity = m.velocity,
                           .airborne = m.airborne,
                           .floor = m.floor_z + kMins.z,
                           .fade = m.fade,
                           .turn = m.turn,
                           .accel = m.accel,
                           .top = sh::kPathTopSpeed,
                           .reach = lead.reach};
  const sh::Vec3f centre = now + (kMins + kMaxs) * 0.5f;
  sh::Vec3f dir = centre - eye;
  sh::normalize(dir);
  sh::LeadShot shot{eye, dir * w.speed + sh::Vec3f{0, 0, w.up}, w.gravity, w.flight};
  const sh::LeadBend b =
      sh::aim_lead(shot, dir, std::span<const sh::AimMover>(&mover, 1), {1e5f, 1e5f}, kGravity);
  const sh::Vec3f v = b.bent ? b.velocity : shot.velocity;
  miss = 1e9f;
  const float step = 1.0f / 128;
  sh::Vec3f prev = eye;
  const int steps = static_cast<int>(w.flight / step);
  for (int i = 1; i <= steps; i++) {
    const float t = static_cast<float>(i) * step;
    sh::Vec3f p = sh::mad(eye, t, v);
    p.z -= 0.5f * w.gravity * t * t;
    const sh::Vec3f there = sh::path_at(seg, t0 + t);
    const sh::Vec3f lo = there + kMins, hi = there + kMaxs;
    if (crosses(prev, p, lo, hi)) {
      miss = 0;
      return true;
    }
    miss = std::min(miss, box_distance(p, lo, hi));
    if (t0 + t > seg.back().t) break;
    // Past him across the floor by more than his reach: it went on into the wall behind him (no
    // map is read, so a missile is spent once it has passed him).
    sh::Vec3f flat = p - there;
    flat.z = 0;
    sh::Vec3f along{v.x, v.y, 0};
    if (sh::normalize(along) > 0 && sh::dot(flat, along) > kPast) break;
    prev = p;
  }
  return false;
}

double rate(int hits, int shots) { return shots > 0 ? 100.0 * hits / shots : 0; }

float median(std::vector<float> v) {
  if (v.empty()) return 0;
  std::nth_element(v.begin(), v.begin() + static_cast<ptrdiff_t>(v.size() / 2), v.end());
  return v[v.size() / 2];
}

void run(gl::Lab& lab) {
  std::vector<Player> players;
  std::string map;
  size_t files_read = 0;
  std::vector<std::string> files;
  const std::string demo = lab.arg("demo");
  if (!demo.empty()) {
    std::error_code ec;
    if (std::filesystem::is_directory(demo, ec)) {
      for (const auto& e : std::filesystem::directory_iterator(demo, ec))
        if (e.path().extension() == ".mvd") files.push_back(e.path().string());
      std::sort(files.begin(), files.end());
    } else {
      files.push_back(demo);
    }
  }
  for (const std::string& f : files) {
    std::string error, m;
    std::vector<Player> got;
    if (!read_demo(f, got, m, error)) {
      lab.fail("se-aimlead: " + error,
               "next: pass --demo=<a multiview recording or a folder of them>");
      return;
    }
    std::string line = f;
    line += ": " + std::to_string(got.size()) + " players on ";
    line += m;
    lab.note(line);
    if (map.empty()) map = m;
    // Each recording's players are matched only among themselves, on its own clock (a clock
    // shifted far away would lose its float's fine steps: at 2e6 s they are 1/4 s apart).
    const int game = static_cast<int>(files_read++);
    for (Player& p : got) {
      p.game = game;
      players.push_back(std::move(p));
    }
  }
  if (files.empty()) players = written_paths(static_cast<float>(lab.arg_num("seconds")));

  std::vector<Rule> rules;
  std::string bad;
  if (!parse_rules(lab.arg("rules"), rules, bad)) {
    lab.fail(
        "se-aimlead: the rule \"" + bad + "\" is not one this lab knows",
        "next: write --rules as still, last, mean:<s>, across:<s>, fade:<s>, turn:<s>, smooth:<s>, "
        "accel:<s>, sway:<s>, each with an "
        "optional /level (no height read), /arc (the air by gravity), @<units/s> running threshold "
        "and ~<s> reach");
    return;
  }
  const size_t nw = std::size(kWeapons);
  std::vector<std::vector<Tally>> tally(nw, std::vector<Tally>(rules.size()));

  const auto every = static_cast<float>(lab.arg_num("every"));
  const auto near = static_cast<float>(lab.arg_num("near"));
  const auto far = static_cast<float>(lab.arg_num("far"));
  const auto shooters = static_cast<size_t>(lab.arg_num("shooters"));
  int moments = 0;
  for (size_t g = 0; g < players.size(); g++) {
    for (const Segment& seg : players[g].segments) {
      if (seg.size() < 4) continue;
      const float first = seg.front().t + 1, last = seg.back().t - 1.5f;
      for (int step = 0; first + static_cast<float>(step) * every <= last; step++) {
        const float t0 = first + static_cast<float>(step) * every;
        const sh::Vec3f at = sh::path_at(seg, t0);
        std::vector<std::pair<float, sh::Vec3f>> eyes;
        for (size_t s = 0; s < players.size(); s++) {
          if (s == g || players[s].game != players[g].game) continue;
          const Segment* ss = holding(players[s], t0, 0, 0);
          if (ss == nullptr) continue;
          const sh::Vec3f eye = sh::path_at(*ss, t0) + sh::Vec3f{0, 0, kEye};
          const float d = sh::length(eye - at);
          if (d >= near && d <= far) eyes.emplace_back(d, eye);
        }
        std::sort(eyes.begin(), eyes.end(),
                  [](const auto& a, const auto& b) { return a.first < b.first; });
        if (eyes.size() > shooters) eyes.resize(shooters);
        if (!eyes.empty()) moments++;
        for (const auto& e : eyes)
          for (size_t w = 0; w < nw; w++)
            for (size_t r = 0; r < rules.size(); r++) {
              float miss = 0;
              bool air = false, run = false;
              const bool hit = fire(kWeapons[w], e.second, seg, t0, rules[r], miss, air, run);
              Tally& k = tally[w][r];
              k.shots++;
              k.hits += hit ? 1 : 0;
              k.air_shots += air ? 1 : 0;
              k.air_hits += air && hit ? 1 : 0;
              k.run_shots += run ? 1 : 0;
              k.run_hits += run && hit ? 1 : 0;
              k.misses.push_back(miss);
              for (size_t i = 0; i < kBandCount; i++)
                if (e.first >= kBands[i] && e.first < kBands[i + 1]) {
                  k.band_shots[i]++;
                  k.band_hits[i] += hit ? 1 : 0;
                }
            }
      }
    }
  }
  if (tally[0][0].shots == 0) {
    lab.fail("se-aimlead: no shot was fired (no two players alive near enough at once)",
             "next: widen --far, or pass a recording with more players");
    return;
  }
  lab.metric("paths.players", static_cast<double>(players.size()), "players");
  lab.metric("shots.moments", moments, "moments");
  gl::LabTable t{
      "hit rate by weapon and rule (a running target and one in the air: their own rates)",
      {"weapon", "rule", "shots", "hit %", "running hit %", "in the air hit %", "median miss"},
      {}};
  for (size_t w = 0; w < nw; w++) {
    size_t best = 0;
    for (size_t r = 0; r < rules.size(); r++) {
      const Tally& k = tally[w][r];
      if (rate(k.hits, k.shots) > rate(tally[w][best].hits, tally[w][best].shots)) best = r;
      t.rows.push_back({kWeapons[w].name, rules[r].name, std::to_string(k.shots),
                        to_fixed(rate(k.hits, k.shots), 1),
                        to_fixed(rate(k.run_hits, k.run_shots), 1),
                        to_fixed(rate(k.air_hits, k.air_shots), 1), to_fixed(median(k.misses), 0)});
    }
    const std::string key = std::string(w == 2 ? "nails" : kWeapons[w].name);
    lab.metric(key + ".still_hit_pct", rate(tally[w][0].hits, tally[w][0].shots), "%");
    lab.metric(key + ".last_hit_pct", rate(tally[w][1].hits, tally[w][1].shots), "%");
    lab.metric(key + ".best_hit_pct", rate(tally[w][best].hits, tally[w][best].shots), "%");
    lab.note(std::string(kWeapons[w].name) + ": the best rule is " + rules[best].name + ", " +
             to_fixed(rate(tally[w][best].hits, tally[w][best].shots), 1) + "% (last step " +
             to_fixed(rate(tally[w][1].hits, tally[w][1].shots), 1) + "%, still " +
             to_fixed(rate(tally[w][0].hits, tally[w][0].shots), 1) + "%)");
  }
  lab.table(std::move(t));
  gl::LabTable bt{
      "hit rate by the shooter's distance (units): no lead, last step and the best rule",
      {"weapon", "rule"},
      {}};
  for (size_t i = 0; i < kBandCount; i++)
    bt.columns.push_back(to_fixed(kBands[i], 0) + "-" +
                         (kBands[i + 1] > 1e8f ? std::string("") : to_fixed(kBands[i + 1], 0)));
  for (size_t w = 0; w < nw; w++) {
    size_t best = 0;
    for (size_t r = 0; r < rules.size(); r++)
      if (rate(tally[w][r].hits, tally[w][r].shots) >
          rate(tally[w][best].hits, tally[w][best].shots))
        best = r;
    for (const size_t r : {size_t(0), size_t(1), best}) {
      std::vector<std::string> row = {kWeapons[w].name, rules[r].name};
      for (size_t i = 0; i < kBandCount; i++)
        row.push_back(to_fixed(rate(tally[w][r].band_hits[i], tally[w][r].band_shots[i]), 1) +
                      " (" + std::to_string(tally[w][r].band_shots[i]) + ")");
      bt.rows.push_back(std::move(row));
    }
  }
  lab.table(std::move(bt));

  const std::vector<std::pair<std::string, std::string>> in = {
      {"demo", demo.empty() ? "(paths written in code)" : demo},
      {"map", map},
      {"every", lab.arg("every")},
      {"shooters", lab.arg("shooters")}};
  gl::LineChart c;
  c.frame = {"LEADING A MISSILE ONTO REAL PLAYERS",
             std::to_string(tally[0][0].shots) + " shots a rule and a weapon, at real paths",
             "the rule (its number in the table)", "hits (%)"};
  c.spikes = false;
  c.points = true;
  for (size_t w = 0; w < nw; w++) {
    gl::Trace tr{kWeapons[w].name, {}, {}};
    for (size_t r = 0; r < rules.size(); r++) {
      tr.x.push_back(double(r + 1));
      tr.y.push_back(rate(tally[w][r].hits, tally[w][r].shots));
    }
    c.traces.push_back(std::move(tr));
  }
  for (size_t r = 0; r < rules.size(); r++) c.marks.push_back({double(r + 1), rules[r].name});
  la::shoot(lab, "lead", gl::line_chart(c),
            "Each living player shot at by the nearest others from where they stood, the "
            "missile led by each rule and flown against where he really went; a dot a rule and "
            "a weapon.",
            "the rule standing highest for each weapon, against still (no lead)", in);
}

const gl::LabSpec kSpec{
    "se-aimlead",
    gl::LabKind::sim,
    "game:shooters_edge",
    "plugins/shooter/aim_path (the lead's motion rules) under aim_help's aim_lead",
    "shooter",
    "Which way of reading a player's motion off his path leads a rocket, a grenade or a nail onto "
    "real players most often?",
    {
        {"demo", "", "a multiview recording or a folder of them (empty: paths written in code)"},
        {"seconds", "40", "with no --demo: the written paths' length, s"},
        {"every", "0.5", "s between the moments shots are fired"},
        {"shooters", "3", "the nearest other players who fire at each target"},
        {"near", "150", "the nearest a shooter stands, units"},
        {"far", "1500", "the farthest a shooter stands, units"},
        {"rules", "still,last,mean:0.1,mean:1,smooth:0.2,fade:0.5,last/level,last/arc@150~0.6",
         "the rules: still, last, mean:<s>, across:<s>, fade:<s>, turn:<s>, smooth:<s>, accel:<s>, "
         "sway:<s>; each with /level (no height), /arc (the air by gravity), @<units/s> a running "
         "threshold, ~<s> how far ahead the run is foreseen"},
    },
    "--seconds=12 --every=1",
    run,
};
GE_LAB(kSpec);

}  // namespace
