// LAB `se-people`: HOW PLAYERS SPEND A TEAM MATCH, read off multiview recordings (people's or our
// bots'), by the same rules for both, so a bot's numbers are set beside people's measured alike.
// Every `.mvd` in `--dir` (or `--file`) is read frame by frame (client/demo.hpp) and each player
// sampled alive every `--every` seconds of the match:
//
// - IN VIEW: an enemy alive within the bots' sight range (4096 units) with a clear line from the
//   player's eye to the enemy's head or middle through the map's world (hull 0), whichever way the
//   player looks: the share of the time a fight could be had;
// - FACED: one of them on the player's screen as the bots' view is (120 degrees across and 88.5
//   high, off the recorded view angles; bots/fighter.cpp): the share of the time a fight is in
//   front of him;
// - NEAR: an enemy alive within `--near` units, seen or not (where a player stands against the
//   other team);
// - THE LAUNCHER: had (its item bit), and had with rockets for it;
// - SPEED: across the floor, between two samples alive (a jump past 1000 units/s, a teleport, left
//   out).
//
// The map is the demos' own, from `--basedir`'s game folder; demos of another map are skipped.
// Without a demo it records a short match of our bots on the fixture halls (classic_team.hpp) and
// reads that, which is what `make labs-smoke` runs. Deterministic: the digest holds the counts.
//
// ge:summary Lab: people's and bots' matches measured alike: enemy in view, near, speed
// ge:keywords lab se-people people realism yardstick in-view line-of-sight engagement contact
// ge:keywords speed mvd multiview recordings real-matches bots-against-people positioning
// ge:skill bots labs
// ge:test tests/shooters_edge_team_test.cpp
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

#include "brush/write.hpp"
#include "core/num_text.hpp"
#include "core/parallel.hpp"
#include "lab/lab.hpp"
#include "shooter/hull.hpp"
#include "shooters_edge/bots/pilot_run.hpp"
#include "shooters_edge/client/demo.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/labs/move_study.hpp"
#include "shooters_edge/net/classic_team.hpp"

namespace {

namespace gl = ge::lab;
namespace sh = ge::shooter;
namespace cl = shooters_edge::client;
namespace cn = shooters_edge::net;
namespace la = shooters_edge::labs;
using ge::core::num::to_fixed;

constexpr float kSight = 4096;  // the bots' sight range (shooter/senses.hpp)
constexpr float kEye = 22;      // the eye over the origin
constexpr float kFastest = 1000;
constexpr float kFov = 120;        // the bots' view across, degrees (bots/fighter.cpp)
constexpr float kFovY = 88.5f;     // and its height
constexpr int32_t kLauncher = 32;  // the rocket launcher's item bit
constexpr double kFired_s = 0.3;   // a shot this near a sample counts as firing then

struct Player {
  std::string name, team;
  int64_t alive = 0, in_view = 0, in_cone = 0, near = 0, moved = 0, rl = 0, rl_loaded = 0;
  // The view: samples after a live one, those turning under 10 and over 360 degrees a second, the
  // turn summed; and of the samples with an enemy in plain view, those he fired within kFired_s.
  int64_t turned_n = 0, still = 0, fast = 0, fired_in_view = 0;
  double speed = 0, turn = 0;
};

struct Read {
  bool ok = false, other_map = false;
  std::string error, map;
  double seconds = 0;
  std::vector<Player> players;  // by slot, those who played
};

// A slot's team: its user info's `team`, else its shirt's colour (a recording without the key).
std::string team_of(const cl::DemoState& s, int i) {
  const std::string& t = s.teams[static_cast<size_t>(i)];
  return t.empty() ? "colour " + std::to_string(s.bottom[static_cast<size_t>(i)]) : t;
}

bool clear(const sh::Hull& h, sh::Vec3f a, sh::Vec3f b) {
  const sh::HullTrace tr = sh::trace_hull(h, a, b);
  return !tr.startsolid && tr.fraction >= 1;
}

void read_one(std::vector<uint8_t> bytes, const la::StudyMap& m, double every, float near,
              Read& out) {
  cl::Demo d;
  if (!cl::open_demo(std::move(bytes), d, out.error)) return;
  const sh::Hull& world = m.bsp.models[0].hulls[0];
  // The bots' screen (shooter/senses.hpp `sees_player`): each half angle as a cosine and sine.
  const sh::Vec3f wide = sh::angle_vectors({0, kFov * 0.5f, 0}).forward;
  const sh::Vec3f high = sh::angle_vectors({0, kFovY * 0.5f, 0}).forward;
  std::vector<Player> by(32);
  std::vector<sh::Vec3f> was(32);
  std::vector<uint8_t> was_alive(32, 0);
  std::vector<float> was_yaw(32, 0);
  std::vector<double> fired_at(32, -1e9);  // each player's last weapon sound
  double next = 0, t0 = -1;
  for (;;) {
    const cl::DemoRead rd = cl::next_frame(d, out.error);
    if (rd == cl::DemoRead::error) return;
    if (rd == cl::DemoRead::end) break;
    const cl::DemoState& s = d.state;
    if (!s.map.empty() && s.map != m.name) {
      out.other_map = true;
      out.map = s.map;
      out.ok = true;
      return;
    }
    if (s.intermission || s.players.size() < 32) continue;
    // Every frame's weapon sounds (the weapon channel, the player's own entity): his shots.
    for (const cl::DemoSound& snd : s.started)
      if (snd.channel == 1 && snd.entity >= 1 && snd.entity <= 32)
        fired_at[static_cast<size_t>(snd.entity - 1)] = s.time;
    if (t0 < 0) t0 = s.time;
    if (s.time < next) continue;
    next = s.time + every;
    out.seconds = s.time - t0;
    for (int i = 0; i < 32; i++) {
      const cl::DemoEntity& p = s.players[static_cast<size_t>(i)];
      const bool live = p.present && !p.dead && !s.names[static_cast<size_t>(i)].empty();
      if (!live) {
        was_alive[static_cast<size_t>(i)] = 0;
        continue;
      }
      Player& pl = by[static_cast<size_t>(i)];
      pl.name = s.names[static_cast<size_t>(i)];
      pl.team = team_of(s, i);
      pl.alive++;
      const sh::Vec3f eye{p.origin.x, p.origin.y, p.origin.z + kEye};
      bool view = false, cone = false, close = false;
      const sh::ViewAxes ax = sh::angle_vectors(p.angles);
      for (int j = 0; j < 32; j++) {
        const cl::DemoEntity& e = s.players[static_cast<size_t>(j)];
        if (j == i || !e.present || e.dead || s.names[static_cast<size_t>(j)].empty() ||
            team_of(s, j) == pl.team)
          continue;
        const sh::Vec3f to = e.origin - p.origin;
        const float d2 = to.x * to.x + to.y * to.y + to.z * to.z;
        close = close || d2 < near * near;
        if (cone || d2 > kSight * kSight) continue;
        const sh::Vec3f head{e.origin.x, e.origin.y, e.origin.z + kEye};
        if (!clear(world, eye, head) && !clear(world, eye, e.origin)) continue;
        view = true;
        const sh::Vec3f h = head - eye;
        const float ahead = sh::dot(ax.forward, h);
        cone = sh::dot(h, h) < 1 ||
               (ahead > 0 && std::fabs(sh::dot(ax.right, h)) * wide.x <= ahead * wide.y &&
                std::fabs(sh::dot(ax.up, h)) * high.x <= ahead * high.y);
      }
      pl.in_view += view;
      pl.fired_in_view += view && s.time - fired_at[static_cast<size_t>(i)] <= kFired_s;
      pl.in_cone += cone;
      if (static_cast<size_t>(i) < d.player_stats.size()) {
        const auto& st = d.player_stats[static_cast<size_t>(i)];
        const bool has = (st[cl::kStatItems] & kLauncher) != 0;
        pl.rl += has;
        pl.rl_loaded += has && st[cl::kStatRockets] > 0;
      }
      pl.near += close;
      if (was_alive[static_cast<size_t>(i)]) {
        const sh::Vec3f dv = p.origin - was[static_cast<size_t>(i)];
        const double v =
            std::sqrt(static_cast<double>(dv.x) * dv.x + static_cast<double>(dv.y) * dv.y) / every;
        if (v < kFastest) {
          pl.speed += v;
          pl.moved++;
        }
      }
      if (was_alive[static_cast<size_t>(i)]) {
        float dy = p.angles.y - was_yaw[static_cast<size_t>(i)];
        while (dy > 180) dy -= 360;
        while (dy < -180) dy += 360;
        const double rate = std::fabs(dy) / every;
        pl.turned_n++;
        pl.turn += rate;
        pl.still += rate < 10;
        pl.fast += rate > 360;
      }
      was_yaw[static_cast<size_t>(i)] = p.angles.y;
      was[static_cast<size_t>(i)] = p.origin;
      was_alive[static_cast<size_t>(i)] = 1;
    }
  }
  for (Player& p : by)
    if (p.alive > 0) out.players.push_back(std::move(p));
  out.ok = true;
}

// A short match of our bots on the fixture halls, recorded: what the lab reads without a demo.
bool bots_match(std::vector<uint8_t>& mvd, std::string& error) {
  std::vector<uint8_t> bytes;
  if (!ge::brush::write_bsp(ge::brush::sample_fixture(), ge::brush::Family::v29, &bytes, &error))
    return false;
  ge::progs::Progs progs;
  if (!cl::baseline_progs(progs, error)) return false;
  cn::TeamMatchOptions o;
  o.map = bytes;
  o.map_name = "halls";
  o.entities = cn::kTeamHallsEntities;
  o.progs = &progs;
  o.seed = 1;
  o.frames = static_cast<int>(std::lround(0.5 * 60 * 1000 / shooters_edge::bots::kPilotMsec));
  const cn::TeamMatchReport r = cn::run_team_match(o);
  if (!r.ok) {
    error = r.error;
    return false;
  }
  mvd = r.mvd;
  return true;
}

void run(gl::Lab& lab) {
  std::string error;
  const std::string dir = lab.arg("dir"), file = lab.arg("file");
  std::vector<std::string> paths;
  if (!file.empty()) paths.push_back(file);
  if (!dir.empty()) {
    std::error_code ec;
    for (const auto& e : std::filesystem::directory_iterator(dir, ec))
      if (e.is_regular_file() && e.path().extension() == ".mvd") paths.push_back(e.path().string());
    if (ec) {
      lab.fail("se-people: the folder " + dir + " does not list: " + ec.message(),
               "next: pass --dir=<a folder of .mvd recordings>");
      return;
    }
    std::sort(paths.begin() + (file.empty() ? 0 : 1), paths.end());
  }
  std::vector<uint8_t> made;
  if (paths.empty() && !bots_match(made, error)) {
    lab.fail("se-people: the bots' match: " + error, "next: run tests/shooters_edge_team_test.cpp");
    return;
  }
  la::StudyMap m;
  if (!la::load_study_map("", paths.empty() ? "" : lab.arg("basedir"), lab.arg("game"),
                          lab.arg("mapname"), m, error)) {
    lab.fail("se-people: " + error, "next: make lab LAB=se-people ARGS=--help; skill: bots");
    return;
  }
  const double every = std::max(0.02, lab.arg_num("every"));
  const auto near = static_cast<float>(lab.arg_num("near"));
  const size_t n = paths.empty() ? 1 : paths.size();
  std::vector<Read> reads(n);
  ge::core::parallel_bands(n, n, [&](size_t b, size_t e) {
    for (size_t i = b; i < e; i++)
      read_one(paths.empty() ? made : la::read_whole(paths[i]), m, every, near, reads[i]);
  });

  gl::LabTable per{
      "each recording: the share of the time alive with an enemy in plain view, one "
      "within --near, and the speed (units/s)",
      {"recording", "seconds", "players", "in view", "faced", "near", "speed"},
      {}};
  Player all;
  int used = 0, skipped = 0;
  double seconds = 0;
  for (size_t i = 0; i < n; i++) {
    const Read& r = reads[i];
    const std::string label =
        paths.empty() ? "bots on the halls" : std::filesystem::path(paths[i]).filename().string();
    if (!r.ok || r.other_map) {
      if (skipped++ < 3)
        lab.note("skipped " + label + ": " + (r.ok ? "the map is " + r.map : r.error));
      continue;
    }
    used++;
    seconds += r.seconds;
    Player sum;
    for (const Player& p : r.players) {
      sum.alive += p.alive;
      sum.in_view += p.in_view;
      sum.in_cone += p.in_cone;
      sum.rl += p.rl;
      sum.rl_loaded += p.rl_loaded;
      sum.near += p.near;
      sum.speed += p.speed;
      sum.moved += p.moved;
      sum.turned_n += p.turned_n;
      sum.still += p.still;
      sum.fast += p.fast;
      sum.turn += p.turn;
      sum.fired_in_view += p.fired_in_view;
    }
    all.alive += sum.alive;
    all.in_view += sum.in_view;
    all.in_cone += sum.in_cone;
    all.rl += sum.rl;
    all.rl_loaded += sum.rl_loaded;
    all.near += sum.near;
    all.speed += sum.speed;
    all.turned_n += sum.turned_n;
    all.still += sum.still;
    all.fast += sum.fast;
    all.turn += sum.turn;
    all.fired_in_view += sum.fired_in_view;
    all.moved += sum.moved;
    const auto share = [&](int64_t k) {
      return to_fixed(sum.alive > 0 ? static_cast<double>(k) / static_cast<double>(sum.alive) : 0,
                      3);
    };
    if (per.rows.size() < 40)
      per.rows.push_back(
          {label, to_fixed(r.seconds, 0), std::to_string(r.players.size()), share(sum.in_view),
           share(sum.in_cone), share(sum.near),
           to_fixed(sum.moved > 0 ? sum.speed / static_cast<double>(sum.moved) : 0, 0)});
  }
  lab.table(per);
  const auto of = [&](int64_t k) {
    return all.alive > 0 ? static_cast<double>(k) / static_cast<double>(all.alive) : 0;
  };
  lab.metric("people.recordings", used, "count");
  lab.metric("people.skipped", skipped, "count");
  lab.metric("people.seconds", seconds, "s");
  lab.metric("people.in-view", of(all.in_view), "share");
  lab.metric("people.faced", of(all.in_cone), "share");
  lab.metric("people.near", of(all.near), "share");
  lab.metric("people.rl-had", of(all.rl), "share");
  lab.metric("people.rl-loaded", of(all.rl_loaded), "share");
  lab.metric("people.speed", all.moved > 0 ? all.speed / static_cast<double>(all.moved) : 0,
             "units/s");
  const auto per_turn = [&](double k) {
    return all.turned_n > 0 ? k / static_cast<double>(all.turned_n) : 0;
  };
  lab.metric("people.view-still", per_turn(static_cast<double>(all.still)), "share");
  lab.metric("people.view-fast", per_turn(static_cast<double>(all.fast)), "share");
  lab.metric("people.view-turn", per_turn(all.turn), "degrees/s");
  lab.metric("people.fire-in-view",
             all.in_view > 0
                 ? static_cast<double>(all.fired_in_view) / static_cast<double>(all.in_view)
                 : 0,
             "share");
  if (used == 0)
    lab.fail("se-people: no recording of " + m.name + " read",
             "next: pass --mapname=<the recordings' map> with --basedir, or --dir of that map's");
}

const gl::LabSpec kSpec{
    "se-people",
    gl::LabKind::sim,
    "game:shooters_edge",
    "how players spend a team match, off multiview recordings: an enemy in plain view, one near, "
    "the speed (games/shooters_edge/labs/se_people_lab.cpp)",
    "bots",
    "How much of a team match do players spend with an enemy in plain view, near one, and how "
    "fast do they move: people's recordings and our bots' measured by the same rules?",
    {
        {"dir", "", "a folder of .mvd recordings of one map (people's, or se-team --save's)"},
        {"file", "", "one recording's path (with --dir: read too)"},
        {"basedir", "", "a folder holding the game folder the recordings' map is read from"},
        {"game", "id1", "the game folder under --basedir"},
        {"mapname", "e1m2", "with --basedir: the recordings' map"},
        {"every", "0.1", "a sample every this many seconds of the match"},
        {"near", "1000", "an enemy this near is near, units"},
    },
    "",
    run,
};
GE_LAB(kSpec);

}  // namespace
