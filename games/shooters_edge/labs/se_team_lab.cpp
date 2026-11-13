// LAB `se-team`: does a team game of bots play like a team game? Two teams of four fighters
// (classic_team.hpp: a client each over our server, the team's plan bots/team.hpp) play the
// baseline mod with `teamplay 1` for `--minutes`, recorded as a multiview demo. By default on the
// fixture halls (plugins/brush, a map written in code); `--basedir` with `--map` plays a map of a
// game folder's instead (the classic small team map is `e1m2` of the shareware pack).
//
// Pictured: the map from above (the nav graph's floor shaded by height) with every player's trail
// in his team's colour, the items and the two key areas; each player's role through the match as a
// strip (hold, rush, carry, free, dead); and each team's frags against time. The table gives each
// player's frags, deaths, shots, shots held for a teammate, backpacks, his share of time in each
// role and the weapon he held most; the teams' rows their totals, rushes and quads. The recording
// is attached (`match.mvd`); `--save` writes it to a path as well, packed as it is recorded when
// the path ends in `z` (`.mvdz`: net/classic_record.hpp).
//
// ITEM CONTROL (`team.lying.<item>`): each key item's share of the time it lay on its spot, and of
// that time by what the bots made of it (classic_team.hpp `kLyingWhy`): nobody alive believed it
// there, nobody wanted it, the one who wanted it was fighting, one on his way (far, within 400
// units, standing on it). People take the yellow armour within 4 s of its showing (se-people).
//
// THE SET (`--seeds=1-6`, `--variants=...`): the A/B a bot change is judged by, every variant on
// every seed in parallel and one table of means (below, `run_set`).
//
// ge:summary Lab: a recorded 4on4 of bots on teamplay: roles, key areas, frags by team, the MVD
// ge:keywords lab se-team team-game 4on4 teamplay roles hold rush carry quad key-area mvd record
// ge:keywords item-control lying untaken armour-control
// ge:skill shooter bots labs
// ge:test tests/shooters_edge_team_test.cpp
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <string_view>
#include <thread>
#include <tuple>
#include <utility>
#include <vector>

#include "brush/write.hpp"
#include "classicenhance/profile.hpp"
#include "core/num_text.hpp"
#include "lab/chart.hpp"
#include "lab/lab.hpp"
#include "lab/raster.hpp"
#include "shooter/nav.hpp"
#include "shooter/trace.hpp"
#include "shooters_edge/bots/fighter.hpp"
#include "shooters_edge/bots/knowledge.hpp"
#include "shooters_edge/bots/pilot_run.hpp"
#include "shooters_edge/bots/team.hpp"
#include "shooters_edge/client/files.hpp"
#include "shooters_edge/client/game_adapter.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/labs/move_study.hpp"
#include "shooters_edge/labs/why_log.hpp"
#include "shooters_edge/net/classic_team.hpp"
#include "shooters_edge/server/rules_runner.hpp"

namespace {

namespace gl = ge::lab;
namespace sh = ge::shooter;
namespace cn = shooters_edge::net;
namespace bt = shooters_edge::bots;
namespace cl = shooters_edge::client;
using ge::core::num::to_fixed;
using In = std::vector<std::pair<std::string, std::string>>;

// A file this run writes (--trace, --why, --save) gets its folder made first: a set's paths name a
// folder a variant (`{variant}`) that nothing else creates.
void make_folder_for(const std::string& path) {
  std::error_code ec;
  const auto dir = std::filesystem::path(path).parent_path();
  if (!dir.empty()) std::filesystem::create_directories(dir, ec);
}

const gl::Rgba kTeam[2] = {{200, 50, 40, 255}, {40, 90, 210, 255}};
const gl::Rgba kRole[6] = {{150, 160, 170, 255},  // free
                           {40, 150, 70, 255},    // hold
                           {240, 160, 20, 255},   // rush
                           {150, 60, 200, 255},   // carry
                           {220, 60, 90, 255},    // push
                           {40, 40, 40, 255}};    // dead
const gl::Rgba kInk{30, 30, 30, 255};

// The map scaled to fit `size` pixels across its longer side.
struct View {
  sh::Vec3f lo, hi;
  double scale = 1;
  int margin = 28;
  double px(double x) const { return margin + (x - lo.x) * scale; }
  double py(double y) const { return margin + (hi.y - y) * scale; }
};

// THE HOP, measured on every command of every bot, by skill (`team.hop.<skill>.*`): its share of
// the time alive, its speed's median, the time alive past 340 u/s across; and every FLIGHT (a
// hop's landing jump through the hopping commands after it, five or more): the takeoff speed,
// the air's gain (the last hopping command's speed less the takeoff's), a crash (80 u/s or more
// lost), a takeoff past 360, chained (the next command jumps again off a landing) or the
// stretch ended. People hop 36 % of their time; the bots' gap jumps need 420-500 at takeoff.
// WHY AND WHERE THEY STAND STILL: the share of every bot's commands that wait for a mover in the
// way, have no route, or walk the route under 20 u/s, and the top spots of each (64-unit cells,
// with the goal and the fight mode: feed one to se-spot). THE VIEW BY WHAT TURNED IT: each view
// writer's (aim, look, calm, air, swim, pilot and the game's overrides: shooter/bot.hpp `view_by`)
// share of the time and its reversals a second (the yaw's turn flipping between two commands,
// both past 60 degrees a second; people 0.09 a second in se-people's `hand.reversals-per-s`).
constexpr int kStillKinds = 3;
constexpr const char* kStillNames[kStillKinds] = {"mover", "no-route", "walk"};

struct HopSkill {
  int64_t alive = 0, hop = 0, fast = 0;
  int flights = 0, crashes = 0, takeoffs_360 = 0, chained = 0;
  std::vector<float> hop_speed, gain;
};
struct Watch {
  // FACING (se-people's rule, every 4th command alive): an enemy in a clear line within the bots'
  // sight anywhere round (`seen`), and one of them on the 120-degree screen (`faced`), by the
  // bot's mode and what wrote its view: the time an enemy in plain sight lies off the screen.
  std::map<std::string, std::array<int64_t, 2>> facing;
  // The time off the screen by where the nearest enemy in sight lies (beside, behind, over or
  // under), how far, and whether he faces the bot (it in his own screen: coming at it).
  std::map<std::string, int64_t> facing_off;
  std::ofstream* out = nullptr;                // the trace, when asked for
  shooters_edge::labs::WhyLog* log = nullptr;  // the decision log, when asked for (--why)
  int every = 4;
  std::array<int, 64> count{};
  struct Flight {
    bool on = false;
    float v0 = 0, last = 0;
    int n = 0;
  };
  std::array<Flight, 64> flight{};
  std::array<HopSkill, 4> skill{};  // by sh::AimSkill
  // Why and where they stand (`team.still.*`): a mover in the way, no route, walking under 20 u/s.
  int64_t commands = 0;
  std::array<int64_t, kStillKinds> still{};
  std::array<std::map<std::tuple<int, int, int, std::string, std::string>, int>, kStillKinds> spots;
  // The view's turn by what turned it (`team.view.*`): each bot's last yaw and rate.
  struct View {
    double t = -1;
    float yaw = 0, rate = 0;
  };
  std::array<View, 64> view{};
  // Each bot's pilot mode over the match, a row a change (the time, the mode's name): the
  // flights read off the recording afterwards are put down to the mode at their takeoff.
  std::array<std::vector<std::pair<double, const char*>>, 64> modes;
  // The bumps (`team.bumps.*`): a speed lost in one command, 120 u/s or more (a wall, a corner,
  // a player met; the move brakes at most ~40 a command), hard when it ends under 100 u/s.
  // Each is put down to what took the speed (bump_by: a wall, a mover, a player, open) and its
  // spot kept (64-unit cells, the pilot's mode): the corners a look ahead must see.
  std::array<float, 64> last_speed{};
  std::array<sh::Vec3f, 64> last_vel{};
  std::array<float, 64> last_health{};
  std::array<bool, 64> moved{};
  int64_t bumps = 0, hard_bumps = 0, alive_commands = 0;
  // Each side's (red 0, blue 1) commands alive and its bumps by walls and by players, for an A/B
  // by team (`--sides=1`): which side the feature under test slowed or sped.
  std::array<int64_t, 2> side_alive{}, side_wall{}, side_player{};
  std::map<std::string, int64_t> bumps_by_mode, bumps_by;
  std::map<std::tuple<int, int, int, std::string, std::string>, int> bump_spots;
  double t_last = 0;
  std::map<std::string, std::pair<double, int64_t>> writers;  // time, reversals
};

// What took a bumped player's speed: the box swept 16 units on along its velocity before the bump
// meets the world (`wall`), another player (`player`) or a brush entity (`mover`: a door, a plat);
// nothing there is `open` (a landing, water, a push). A bump in a command that took health is put
// down to the hit's knockback (`hit`) before this is asked.
const char* bump_by(const sh::Server& sv, int ent, sh::Vec3f origin, sh::Vec3f vel) {
  const float len = std::sqrt(vel.x * vel.x + vel.y * vel.y);
  if (len < 1) return "open";
  const sh::Vec3f to{origin.x + vel.x * 16 / len, origin.y + vel.y * 16 / len, origin.z};
  const sh::HullTrace tr =
      sh::trace_move(sv, origin, {-16, -16, -24}, {16, 16, 32}, to, sh::kTraceNormal, ent);
  // Another player's box within 8 units of this one's (the move clips against players its own
  // way: the server's sweep does not always meet them).
  for (int c = 1; c <= static_cast<int>(sv.clients.size()); c++) {
    if (c == ent || static_cast<size_t>(c) >= sv.edicts.size()) continue;
    const sh::Edict& o = sv.edicts[static_cast<size_t>(c)];
    if (o.free || o.health <= 0) continue;
    const float dx = o.origin.x - origin.x, dy = o.origin.y - origin.y, dz = o.origin.z - origin.z;
    if (std::fabs(dx) < 40 && std::fabs(dy) < 40 && std::fabs(dz) < 64) return "player";
  }
  if (tr.fraction >= 1 && !tr.startsolid) return "open";
  if (tr.ent <= 0) return tr.plane.normal.z >= 0.7f ? "open" : "wall";
  if (tr.ent <= static_cast<int>(sv.clients.size())) return "player";
  return "mover";
}

void watch_hop(Watch& w, int k, sh::AimSkill skill, bool alive, bool hop, const char* because,
               float speed) {
  HopSkill& s = w.skill[static_cast<size_t>(skill)];
  if (alive) {
    s.alive++;
    if (speed > 340) s.fast++;
    if (hop) {
      s.hop++;
      s.hop_speed.push_back(speed);
    }
  }
  const std::string_view why = because != nullptr ? because : "";
  const bool landing = hop && why.starts_with("a landing");
  Watch::Flight& f = w.flight[static_cast<size_t>(k)];
  if (f.on && hop && why.starts_with("hopping")) {
    f.last = speed;
    f.n++;
    return;
  }
  if (f.on) {
    if (f.n >= 5) {
      const float gain = f.last - f.v0;
      s.flights++;
      s.gain.push_back(gain);
      if (gain < -80) s.crashes++;
      if (f.v0 > 360) s.takeoffs_360++;
      if (landing) s.chained++;
    }
    f.on = false;
  }
  if (landing) f = {true, speed, speed, 0};
}

float median(std::vector<float> v) {
  if (v.empty()) return 0;
  std::sort(v.begin(), v.end());
  return v[v.size() / 2];
}

void watch_still(Watch& w, const shooters_edge::bots::Fighter& f, const sh::Pilot& p, float speed,
                 float x, float y, float z) {
  w.commands++;
  const std::string_view why = p.because != nullptr ? p.because : "";
  int kind = -1;
  if (why == "a mover in the way")
    kind = 0;
  else if (why == "no route")
    kind = 1;
  else if (p.mode == sh::PilotMode::walk && speed < 20)
    kind = 2;
  if (kind < 0) return;
  w.still[static_cast<size_t>(kind)]++;
  const auto cell = [](float v, int g) {
    return static_cast<int>(std::lround(v / static_cast<float>(g))) * g;
  };
  const char* goal = f.goal_item >= 0 && static_cast<size_t>(f.goal_item) < f.items.size()
                         ? shooters_edge::bots::item_call(f.items[static_cast<size_t>(f.goal_item)])
                         : "-";
  w.spots[static_cast<size_t>(kind)][{cell(x, 64), cell(y, 64), cell(z, 32), goal,
                                      shooters_edge::bots::fight_mode_name(f.mode)}]++;
}

void watch_view(Watch& w, int k, double t, float yaw, const char* by) {
  Watch::View& v = w.view[static_cast<size_t>(k)];
  const double dt = t - v.t;
  if (v.t < 0 || dt <= 0 || dt > 0.05) {
    v = {t, yaw, 0};
    return;
  }
  float d = yaw - v.yaw;
  while (d > 180) d -= 360;
  while (d < -180) d += 360;
  const auto rate = static_cast<float>(-d / dt);
  auto& wr = w.writers[by != nullptr ? by : "-"];
  wr.first += dt;
  if (std::fabs(rate) > 60 && std::fabs(v.rate) > 60 && rate * v.rate < 0) wr.second++;
  v = {t, yaw, rate};
}

void facing_metrics(gl::Lab& lab, const Watch& w) {
  int64_t seen = 0, faced = 0;
  for (const auto& [key, row] : w.facing) {
    seen += row[0];
    faced += row[1];
  }
  if (seen == 0) return;
  lab.metric("team.facing.faced", static_cast<double>(faced) / static_cast<double>(seen), "share");
  // Each mode and view writer: its share of the time an enemy is in sight, and of the time one
  // is in sight off the screen.
  int64_t off = 0;
  for (const auto& [key, n] : w.facing_off) off += n;
  for (const auto& [key, n] : w.facing_off)
    lab.metric("team.facing-off." + key, static_cast<double>(n) / static_cast<double>(std::max<int64_t>(off, 1)),
               "share");
  for (const auto& [key, row] : w.facing) {
    lab.metric("team.facing." + key + ".seen", static_cast<double>(row[0]) / static_cast<double>(seen),
               "share");
    if (seen > faced)
      lab.metric("team.facing." + key + ".off",
                 static_cast<double>(row[0] - row[1]) / static_cast<double>(seen - faced), "share");
  }
}

void still_view_metrics(gl::Lab& lab, const Watch& w) {
  // A bot-minute is 77 * 60 of its commands alive.
  const double bot_min = std::max(1.0, static_cast<double>(w.alive_commands) / (77.0 * 60));
  lab.metric("team.bumps.per-bot-min", static_cast<double>(w.bumps) / bot_min, "per-min");
  lab.metric("team.bumps.hard-per-bot-min", static_cast<double>(w.hard_bumps) / bot_min, "per-min");
  for (const auto& [mode, n] : w.bumps_by_mode)
    lab.metric("team.bumps." + mode + "-per-bot-min", static_cast<double>(n) / bot_min, "per-min");
  for (const auto& [by, n] : w.bumps_by)
    lab.metric("team.bumps.by-" + by + "-per-bot-min", static_cast<double>(n) / bot_min, "per-min");
  for (size_t side = 0; side < 2 && lab.arg("sides") == "1"; side++) {
    const double side_min = std::max(1.0, static_cast<double>(w.side_alive[side]) / (77.0 * 60));
    const std::string k = std::string("team.side.") + (side == 0 ? "red." : "blue.");
    lab.metric(k + "bumps.by-wall-per-bot-min", static_cast<double>(w.side_wall[side]) / side_min,
               "per-min");
    lab.metric(k + "bumps.by-player-per-bot-min",
               static_cast<double>(w.side_player[side]) / side_min, "per-min");
  }
  {
    gl::LabTable bt{"where they bump (a bot-minute's count; 64-unit cells)",
                    {"by", "per bot-min", "at", "mode"},
                    {}};
    std::vector<std::pair<int, std::tuple<int, int, int, std::string, std::string>>> top;
    top.reserve(w.bump_spots.size());
    for (const auto& [spot, c] : w.bump_spots) top.push_back({-c, spot});
    std::sort(top.begin(), top.end());
    for (size_t i = 0; i < top.size() && i < 10; i++) {
      const auto& [x, y, z, by, mode] = top[i].second;
      bt.rows.push_back({by, to_fixed(-top[i].first / bot_min, 2),
                         std::to_string(x) + " " + std::to_string(y) + " " + std::to_string(z),
                         mode});
    }
    lab.table(bt);
  }
  const double n = static_cast<double>(std::max<int64_t>(1, w.commands));
  gl::LabTable st{"where they stand still (share of every command; 64-unit cells)",
                  {"why", "share %", "at", "goal", "mode"},
                  {}};
  for (int k = 0; k < kStillKinds; k++) {
    lab.metric(std::string("team.still.") + kStillNames[k] + "-share",
               static_cast<double>(w.still[static_cast<size_t>(k)]) / n, "share");
    std::vector<std::pair<int, std::tuple<int, int, int, std::string, std::string>>> top;
    for (const auto& [spot, c] : w.spots[static_cast<size_t>(k)]) top.push_back({-c, spot});
    std::sort(top.begin(), top.end());
    for (size_t i = 0; i < top.size() && i < 5; i++) {
      const auto& [x, y, z, goal, mode] = top[i].second;
      st.rows.push_back({kStillNames[k], to_fixed(100.0 * -top[i].first / n, 1),
                         std::to_string(x) + " " + std::to_string(y) + " " + std::to_string(z),
                         goal, mode});
    }
  }
  lab.table(st);
  double all_t = 0;
  int64_t all_r = 0;
  for (const auto& [by, tr] : w.writers) {
    all_t += tr.first;
    all_r += tr.second;
  }
  gl::LabTable vt{"the view by what turned it", {"writer", "time %", "reversals/s"}, {}};
  for (const auto& [by, tr] : w.writers) {
    const double share = all_t > 0 ? tr.first / all_t : 0;
    const double rev = tr.first > 0 ? static_cast<double>(tr.second) / tr.first : 0;
    vt.rows.push_back({by, to_fixed(100 * share, 1), to_fixed(rev, 2)});
    lab.metric("team.view." + by + ".share", share, "share");
    lab.metric("team.view." + by + ".reversals-per-s", rev, "per-s");
  }
  lab.table(vt);
  lab.metric("team.view.reversals-per-s", all_t > 0 ? static_cast<double>(all_r) / all_t : 0,
             "per-s");
}

void hop_metrics(gl::Lab& lab, const Watch& w) {
  for (const sh::AimSkill k :
       {sh::AimSkill::expert, sh::AimSkill::average, sh::AimSkill::novice, sh::AimSkill::perfect}) {
    const HopSkill& s = w.skill[static_cast<size_t>(k)];
    if (s.alive == 0) continue;
    const std::string key = std::string("team.hop.") + sh::aim_skill_name(k) + ".";
    const auto share = [](int64_t n, int64_t of) {
      return of > 0 ? static_cast<double>(n) / static_cast<double>(of) : 0.0;
    };
    lab.metric(key + "share", share(s.hop, s.alive), "share");
    lab.metric(key + "speed-p50", median(s.hop_speed), "units/s");
    lab.metric(key + "fast-share", share(s.fast, s.alive), "share");
    lab.metric(key + "flights", s.flights, "count");
    lab.metric(key + "crash-share", share(s.crashes, s.flights), "share");
    lab.metric(key + "gain-p50", median(s.gain), "units/s");
    lab.metric(key + "takeoffs-360", s.takeoffs_360, "count");
    lab.metric(key + "chained-share", share(s.chained, s.flights), "share");
  }
}

// EVERY FLIGHT BY WHAT FLEW IT (`team.air.s380.<mode>.*`, a real map's match only): the match's
// own recording read as se-moves reads people's (move_study.hpp, shooter/hops.hpp: each player's
// track off the server's frames, its hops traced on the map's hulls), each flight of 0.3-1 s
// taking off at 380-460 u/s put down to its bot's pilot mode at the takeoff: how many, the air's
// gain (landing less takeoff speed) and the crashes (80 u/s or more lost). People hold their
// speed there (gain -1 to +5 on dm2); the mode says which of the bots' ways loses it.
void flights_by_mode(gl::Lab& lab, const Watch& w, const cn::TeamMatchReport& r,
                     const std::string& basedir, const std::string& game, const std::string& map,
                     bool packed) {
  if (basedir.empty() || r.mvd.empty() || packed) return;
  namespace la = shooters_edge::labs;
  std::string error;
  la::StudyMap m;
  la::DemoTracks dt;
  if (!la::load_study_map("", basedir, game, map, m, error) ||
      !la::read_demo_tracks(r.mvd, dt, error)) {
    lab.note("flights by mode: not read: " + error);
    return;
  }
  // The recording's clock against the fighters' (the server's): both end with the match.
  const double offset = w.t_last - dt.seconds;
  lab.note("flights by mode: the recording's clock + " + to_fixed(offset, 2) + " s = the server's");
  std::map<std::string, std::vector<float>> gain;
  const sh::HopSettings hs;
  for (size_t slot = 0; slot < dt.tracks.size(); slot++) {
    size_t k = r.players.size();
    for (size_t j = 0; j < r.players.size(); j++)
      if (r.players[j].name == dt.names[slot]) k = j;
    if (k >= r.players.size() || k >= w.modes.size() || dt.tracks[slot].size() < 3) continue;
    sh::HopReading hr;
    sh::read_hops(dt.tracks[slot], *m.world, hs, hr);
    const auto& ml = w.modes[k];
    for (const sh::Hop& h : hr.hops) {
      if (h.air_s < 0.3f || h.air_s > 1.0f || h.speed_off < 380 || h.speed_off >= 460) continue;
      const double t = hr.frames[static_cast<size_t>(h.takeoff)].t + offset;
      const char* mode = "-";
      for (const auto& [at, name] : ml) {
        if (at > t) break;
        mode = name;
      }
      gain[mode].push_back(h.speed_land - h.speed_off);
    }
  }
  gl::LabTable at{"flights taking off at 380-460 u/s (0.3-1 s up), by the pilot's mode then",
                  {"mode", "flights", "gain p50", "crash %"},
                  {}};
  for (const auto& [mode, g] : gain) {
    int crashes = 0;
    for (const float x : g) crashes += x < -80 ? 1 : 0;
    const double crash = static_cast<double>(crashes) / static_cast<double>(g.size());
    at.rows.push_back(
        {mode, std::to_string(g.size()), to_fixed(median(g), 1), to_fixed(100 * crash, 1)});
    lab.metric("team.air.s380." + mode + ".flights", static_cast<double>(g.size()), "count");
    lab.metric("team.air.s380." + mode + ".gain-p50", median(g), "units/s");
    lab.metric("team.air.s380." + mode + ".crash-share", crash, "share");
  }
  lab.table(at);
}

// The pilot's link toward its route's next node, by kind ("walk", "jump"...), or "-".
const char* link_of(const sh::Pilot& p) {
  if (p.g == nullptr || p.k + 1 >= p.route.size()) return "-";
  const sh::NavNode& a = p.g->nodes[static_cast<size_t>(p.route[p.k])];
  for (int32_t q = 0; q < a.links; q++) {
    const sh::NavLink& l = p.g->links[static_cast<size_t>(a.first_link) + static_cast<size_t>(q)];
    if (l.to == p.route[p.k + 1]) return sh::nav_kind_name(l.kind);
  }
  return "-";
}

// The kind of the link after the current one (`end`: the next node is the route's last).
const char* link2_of(const sh::Pilot& p) {
  if (p.g == nullptr || p.k + 1 >= p.route.size()) return "-";
  if (p.k + 2 >= p.route.size()) return "end";
  const sh::NavNode& b = p.g->nodes[static_cast<size_t>(p.route[p.k + 1])];
  for (int32_t q = 0; q < b.links; q++) {
    const sh::NavLink& l = p.g->links[static_cast<size_t>(b.first_link) + static_cast<size_t>(q)];
    if (l.to == p.route[p.k + 2]) return sh::nav_kind_name(l.kind);
  }
  return "-";
}

// The decision log written (`--why`): the bots' names, the deaths as entries beside the rest.
void write_why(gl::Lab& lab, shooters_edge::labs::WhyLog& log, const cn::TeamMatchReport& r) {
  for (size_t k = 0; k < r.players.size() && k < log.names.size(); k++)
    log.names[k] = r.players[k].name;
  std::vector<shooters_edge::labs::WhyEntry> deaths;
  for (const std::string& l : r.deaths) {
    // t, player, at, liquid, mode, link, ... (classic_team.cpp)
    std::vector<std::string> f;
    for (size_t i = 0, j; i <= l.size() && f.size() < 6; i = j + 1) {
      j = std::min(l.find('\t', i), l.size());
      f.push_back(l.substr(i, j - i));
    }
    if (f.size() < 6) continue;
    deaths.push_back({std::strtod(f[0].c_str(), nullptr), f[1], "death", "died", f[3], f[4],
                      std::nan(""), "at=" + f[2] + " link=" + f[5]});
  }
  make_folder_for(lab.arg("why"));
  if (!shooters_edge::labs::write_why_log(lab.arg("why"), log, deaths)) {
    lab.fail("se-team: cannot write --why=" + lab.arg("why"),
             "next: pass a path this user may write");
    return;
  }
  lab.note("se-team: the decision log is " + lab.arg("why") + " (" +
           std::to_string(log.entries.size() + deaths.size()) +
           " entries); read it with: labs se-why --file=" + lab.arg("why") +
           " --bot=<name> --at=<m:ss>");
}

void run(gl::Lab& lab) {
  const auto seed = static_cast<uint32_t>(lab.arg_num("seed"));
  const double minutes = lab.arg_num("minutes");
  const std::string basedir = lab.arg("basedir"), map_name = lab.arg("map");
  if (minutes <= 0) {
    lab.fail("se-team: --minutes must be over 0", "next: make lab LAB=se-team ARGS=--help");
    return;
  }
  std::string error;
  std::vector<uint8_t> bytes;
  std::string name = "halls";
  ge::shooter::Locs locs;  // the place names: --locs's file, else the game's (its adapter's)
  if (!lab.arg("locs").empty()) {
    std::FILE* lf = std::fopen(lab.arg("locs").c_str(), "rb");
    if (!lf) {
      lab.fail("se-team: cannot read " + lab.arg("locs"),
               "next: pass --locs=<a .loc file that exists>, or leave it out to name places off "
               "the items");
      return;
    }
    std::string text;
    char buf[4096];
    size_t got = 0;
    while ((got = std::fread(buf, 1, sizeof buf, lf)) > 0) text.append(buf, got);
    std::fclose(lf);
    if (ge::shooter::parse_locs(text, locs) > 0) locs.from = lab.arg("locs");
  }
  if (!basedir.empty()) {
    cl::SearchPath files;
    if (!cl::open_game_dir(basedir, lab.arg("game"), files, error)) {
      lab.fail(error, "Pass --basedir=<the folder that holds the game folder>. Skill: shooter.");
      return;
    }
    name = map_name.empty() ? "e1m2" : map_name;
    std::vector<uint8_t> store;
    const std::span<const uint8_t> f = cl::find_file(files, "maps/" + name + ".bsp", store);
    if (f.empty()) {
      lab.fail("se-team: no maps/" + name + ".bsp in the game folder",
               "Name a map the game folder holds (--map). Skill: shooter.");
      return;
    }
    bytes.assign(f.begin(), f.end());
    // The map's place names, when the game's files hold them (the first game's locs/<map>.loc).
    const cl::GameAdapter& game = cl::classic_adapter();
    if (locs.points.empty()) game.open_locations(game, files, name, locs);
  } else if (!ge::brush::write_bsp(ge::brush::sample_fixture(), ge::brush::Family::v29, &bytes,
                                   &error)) {
    lab.fail("se-team: the fixture halls: " + error, "next: run tests/brush_write_test.cpp");
    return;
  }
  ge::progs::Progs progs;
  if (!cl::baseline_progs(progs, error)) {
    lab.fail("se-team: the baseline mod: " + error,
             "next: run tests/shooters_edge_baseline_mod_test.cpp");
    return;
  }
  const In in = {{"map", name}, {"minutes", lab.arg("minutes")}, {"seed", lab.arg("seed")}};
  cn::TeamMatchOptions o;
  o.map = bytes;
  o.map_name = name;
  if (basedir.empty()) o.entities = cn::kTeamHallsEntities;
  o.locs = std::move(locs);
  o.progs = &progs;
  o.seed = seed;
  o.client_fed = lab.arg_num("client-fed") != 0;
  o.latency_ms = lab.arg_num("latency");
  o.antilag = lab.arg_num("antilag") != 0;
  o.lead_lag = lab.arg_num("lead") != 0;
  {
    // A weapon drill: its name in the hit table, the mod's bit for it (classic_hits.hpp's order).
    const std::string drill = lab.arg("drill");
    const char* names[] = {"scattergun", "double", "spiker", "heavy", "grenades", "rockets", "arc"};
    for (int k = 0; k < 7; k++)
      if (drill == names[k]) o.drill = 1 << k;
    if (!drill.empty() && o.drill == 0) {
      lab.fail("se-team: no weapon " + drill + " to drill",
               "next: --drill=scattergun, double, spiker, heavy, grenades, rockets or arc.");
      return;
    }
  }
  // The skills (`--skills`): one word for every player ("expert": people's level today, most
  // players being experts), or one a player, red's then blue's, comma separated.
  {
    const std::string list = lab.arg("skills");
    std::vector<sh::AimSkill> got;
    size_t at = 0;
    while (!list.empty() && at <= list.size()) {
      const size_t end = std::min(list.find(',', at), list.size());
      const std::string w = list.substr(at, end - at);
      bool found = false;
      for (const sh::AimSkill k : {sh::AimSkill::novice, sh::AimSkill::average,
                                   sh::AimSkill::expert, sh::AimSkill::perfect})
        if (w == sh::aim_skill_name(k)) got.push_back(k), found = true;
      if (!found) {
        lab.fail("se-team: --skills: '" + w + "' is no skill",
                 "next: novice, average, expert or perfect; one for all, or one a player");
        return;
      }
      at = end + 1;
    }
    if (got.size() == 1 || got.size() == o.teams[0].skills.size() + o.teams[1].skills.size()) {
      size_t k = 0;
      for (cn::TeamSide& side : o.teams)
        for (sh::AimSkill& sk : side.skills) sk = got.size() == 1 ? got[0] : got[k++];
    } else if (!got.empty()) {
      lab.fail("se-team: --skills names " + std::to_string(got.size()) + " skills",
               "next: one for every player, or one a player (red's then blue's)");
      return;
    }
  }
  // The temperaments, red's then blue's, a letter or a word each (a, b, c), comma separated.
  {
    const std::string list = lab.arg("tempers");
    size_t at = 0, k = 0;
    while (!list.empty() && at <= list.size()) {
      const size_t end = std::min(list.find(',', at), list.size());
      sh::Temper tp;
      if (!sh::temper_of(std::string_view(list).substr(at, end - at), tp)) {
        lab.fail("se-team: --tempers: '" + list.substr(at, end - at) + "' is no temperament",
                 "next: aggressive, balanced or coward (a, b or c), one a player, red's first");
        return;
      }
      cn::TeamSide& side = o.teams[k < o.teams[0].skills.size() ? 0 : 1];
      side.tempers.push_back(tp);
      k++;
      at = end + 1;
    }
  }
  // The traits over each player's preset, red's then blue's, comma separated, each its changes
  // joined by '/' ("aim_rl=9/timing=3"; empty: the preset).
  {
    const std::string& list = lab.arg("traits");
    size_t at = 0, k = 0;
    while (!list.empty() && at <= list.size()) {
      const size_t end = std::min(list.find(',', at), list.size());
      cn::TeamSide& side = o.teams[k < o.teams[0].skills.size() ? 0 : 1];
      side.traits.push_back(list.substr(at, end - at));
      k++;
      at = end + 1;
    }
  }
  // The bots' features (shooter/features.hpp): every player's, then red's and blue's over them.
  for (int k = 0; k < 2; k++) {
    o.teams[k].features = lab.arg("features");
    const std::string& own = lab.arg(k == 0 ? "features-red" : "features-blue");
    if (!own.empty()) o.teams[k].features += "," + own;
  }
  const std::string save = lab.arg("save");
  o.packed = save.ends_with("z");  // a `.mvdz`: recorded packed as it goes
  // EVERY COMMAND OF EVERY BOT, watched: the hop measured by skill (below, `team.hop.*`), and THE
  // TRACE (`--trace=<file>`): a TSV row per bot every `--trace-every` commands: where it is, how
  // fast, what it means to do and what its pilot did and why, the view and what turned it (the
  // calm view's `glance` and `sound` swipes named apart), the keys sent and the velocity, the
  // heading the pilot's wish runs along (`walk`) and the calm view's target (`look`), and what the
  // feet did with the aim (`press`: footwork.hpp's note), and the heading the air steers for
  // (`way`: a hop's aim, a walk's target in the air). Both this run's own: a set (--seeds) runs
  // several matches at once.
  std::ofstream trace;
  Watch watch;
  if (!lab.arg("trace").empty()) {
    make_folder_for(lab.arg("trace"));
    trace.open(lab.arg("trace"));
    if (!trace) {
      lab.fail("se-team: cannot write --trace=" + lab.arg("trace"),
               "next: pass a path this user may write");
      return;
    }
    trace << "t\tbot\tx\ty\tz\tspeed\thealth\tmode\tgoal\tgoal_node\tunlocking\tpilot\tlink"
             "\tscript_"
             "step\tbecause\tyaw\tpitch\tview\tforward\tside\tvx\tvy\twalk\tlook\tpress\tk\tnext_"
             "x\tnext_y\twish_f\twish_s\tkey_set\tlink2\tnext_wet\trecipe\tground\tjump\tway"
             "\ttarget\ttx\tty\ttvx\ttvy\tface\tvis\tgoal_seen\tgoal_real\tarmour\tfetch\tmover_up\n";
    watch.out = &trace;
    watch.every = std::max(1, static_cast<int>(lab.arg_num("trace-every")));
  }
  // THE DECISION LOG (`--why=<file>`, labs/why_log.hpp): every fighter's decisions recorded, kept
  // when they change, with the speed lost to walls and players and the deaths, on the
  // recording's clock; written at the match's end, read with the se-why lab.
  shooters_edge::labs::WhyLog why_log;
  if (!lab.arg("why").empty()) {
    o.why = &why_log.why;
    watch.log = &why_log;
  }
  o.on_command_ctx = &watch;
  o.on_command = [](void* ctx, int k, const shooters_edge::bots::Fighter& f,
                    const shooters_edge::server::RulesCommon& w, const sh::MoveCmd& cmd) {
    Watch& tr = *static_cast<Watch*>(ctx);
    if (k < 0 || k >= 64) return;
    // The fighter's own entity is its slot's (not the client's player number).
    const auto& e = w.world.entities[static_cast<size_t>(f.slot) + 1];
    const sh::Pilot& p = f.bot.pilot.pilot;
    const float sp = std::sqrt(e.velocity.x * e.velocity.x + e.velocity.y * e.velocity.y);
    watch_hop(tr, k, f.skill, e.health > 0, p.mode == sh::PilotMode::hop, p.because, sp);
    watch_still(tr, f, p, sp, e.origin.x, e.origin.y, e.origin.z - 24);
    const auto i = static_cast<size_t>(k);
    const bool bumped = e.health > 0 && tr.moved[i] && tr.last_speed[i] - sp >= 120;
    const char* by = !bumped ? ""
                     : e.health < tr.last_health[i]
                         ? "hit"
                         : bump_by(w.sv, f.slot + 1, e.origin, tr.last_vel[i]);
    if (tr.log != nullptr)
      tr.log->take(k, w.sv.time, sp, sh::pilot_mode_name(p.mode), link_of(p), e.origin.x,
                   e.origin.y, e.origin.z - 24, by);
    watch_view(tr, k, w.sv.time, cmd.angles.y, f.bot.view_by);
    if (e.health > 0 && f.team != nullptr && (tr.count[static_cast<size_t>(k)] & 3) == 0) {
      // se-people's FACING rule, on the server's truth.
      const sh::Vec3f eye{e.origin.x, e.origin.y, e.origin.z + 22};
      const sh::ViewAxes ax = sh::angle_vectors({cmd.angles.x, cmd.angles.y, 0});
      const sh::Vec3f wide = sh::angle_vectors({0, 60.f, 0}).forward;
      const sh::Vec3f high = sh::angle_vectors({0, 44.25f, 0}).forward;
      bool seen = false, faced = false;
      float near_d2 = 1e30f;
      std::string near_key;
      for (size_t j = 1; j < w.world.entities.size() && j <= 32 && !faced; j++) {
        const int sl = static_cast<int>(j) - 1;
        if (sl == f.slot || !w.world.clients[static_cast<size_t>(sl)].active ||
            (f.team->friends >> sl & 1u) != 0)
          continue;
        const auto& o = w.world.entities[j];
        if (o.health <= 0) continue;
        const sh::Vec3f head{o.origin.x, o.origin.y, o.origin.z + 22};
        const sh::Vec3f h = head - eye;
        if (sh::dot(h, h) > 4096.f * 4096.f) continue;
        const auto clear = [&](sh::Vec3f to) {
          return sh::trace_move(w.sv, eye, {}, {}, to, sh::kTraceNoMonsters, -1).fraction >= 1;
        };
        if (!clear(head) && !clear(o.origin)) continue;
        seen = true;
        const float ahead = sh::dot(ax.forward, h);
        faced = ahead > 0 && std::fabs(sh::dot(ax.right, h)) * wide.x <= ahead * wide.y &&
                std::fabs(sh::dot(ax.up, h)) * high.x <= ahead * high.y;
        if (!faced && sh::dot(h, h) < near_d2) {
          near_d2 = sh::dot(h, h);
          const float flat = std::sqrt(h.x * h.x + h.y * h.y);
          const float cosf_ = flat > 1 ? (ax.forward.x * h.x + ax.forward.y * h.y) /
                                             (flat * std::sqrt(ax.forward.x * ax.forward.x +
                                                               ax.forward.y * ax.forward.y))
                                       : 1.f;
          const char* where = cosf_ > 0.5f ? "over-under" : cosf_ > -0.5f ? "beside" : "behind";
          const float d = std::sqrt(sh::dot(h, h));
          const char* far = d < 500 ? "near" : d < 1500 ? "mid" : "far";
          const sh::ViewAxes his = sh::angle_vectors({o.view_angles.x, o.view_angles.y, 0});
          const sh::Vec3f back = eye - head;
          const float his_ahead = sh::dot(his.forward, back);
          const bool at_me = his_ahead > 0 && std::fabs(sh::dot(his.right, back)) * wide.x <=
                                                  his_ahead * wide.y;
          // What the bot knows of him: seen within 2 s, heard within 1 s, or nothing.
          const char* known = ".unknown";
          for (const sh::Contact& ct : f.bot.senses.contacts) {
            if (ct.ent != static_cast<int32_t>(j)) continue;
            if (ge::ai::known(ct.seen) && ge::ai::age(ct.seen, w.sv.time) <= 2)
              known = ".seen";
            else if (ge::ai::known(ct.heard) && ge::ai::age(ct.heard, w.sv.time) <= 1)
              known = ".heard";
          }
          near_key = std::string(where) + "." + far + (at_me ? ".coming" : ".unaware") + known;
        }
      }
      if (seen && !faced && !near_key.empty()) tr.facing_off[near_key]++;
      if (seen) {
        auto& row = tr.facing[std::string(shooters_edge::bots::fight_mode_name(f.mode)) + "." +
                              f.bot.view_by];
        row[0]++;
        row[1] += faced ? 1 : 0;
      }
    }
    if (e.health > 0) {
      tr.alive_commands++;
      const size_t side = f.team != nullptr && f.team->name == "blue" ? 1 : 0;
      tr.side_alive[side]++;
      if (bumped && std::string_view(by) == "wall") tr.side_wall[side]++;
      if (bumped && std::string_view(by) == "player") tr.side_player[side]++;
      if (bumped) {
        tr.bumps++;
        if (sp < 100) tr.hard_bumps++;
        tr.bumps_by_mode[sh::pilot_mode_name(p.mode)]++;
        tr.bumps_by[by]++;
        const auto cell = [](float v, int g) {
          return static_cast<int>(std::lround(v / static_cast<float>(g))) * g;
        };
        tr.bump_spots[{cell(e.origin.x, 64), cell(e.origin.y, 64), cell(e.origin.z - 24, 32), by,
                       sh::pilot_mode_name(p.mode)}]++;
      }
    }
    tr.last_speed[i] = sp;
    tr.last_vel[i] = {e.velocity.x, e.velocity.y, 0};
    tr.last_health[i] = e.health;
    tr.moved[i] = e.health > 0;
    auto& ml = tr.modes[static_cast<size_t>(k)];
    const char* mode = sh::pilot_mode_name(p.mode);
    if (ml.empty() || ml.back().second != mode) ml.push_back({w.sv.time, mode});
    tr.t_last = std::max(tr.t_last, w.sv.time);
    if (tr.out == nullptr || (tr.count[static_cast<size_t>(k)]++ % tr.every) != 0) return;
    std::ofstream& out = *tr.out;
    const char* link = link_of(p);
    const char* goal =
        f.goal_item >= 0 && static_cast<size_t>(f.goal_item) < f.items.size()
            ? shooters_edge::bots::item_call(f.items[static_cast<size_t>(f.goal_item)])
            : "-";
    out << to_fixed(w.sv.time, 2) << '\t' << k << '\t' << to_fixed(e.origin.x, 0) << '\t'
        << to_fixed(e.origin.y, 0) << '\t' << to_fixed(e.origin.z - 24, 0) << '\t'
        << to_fixed(sp, 0) << '\t' << to_fixed(e.health, 0) << '\t'
        << shooters_edge::bots::fight_mode_name(f.mode) << '\t' << goal << '\t' << f.goal_node
        << '\t' << (f.unlocking ? 1 : 0) << '\t' << sh::pilot_mode_name(p.mode) << '\t' << link
        << '\t' << p.script_step << '\t' << p.because << '\t' << to_fixed(cmd.angles.y, 2) << '\t'
        << to_fixed(cmd.angles.x, 2) << '\t' << f.bot.view_by << '\t' << cmd.forward << '\t'
        << cmd.side << '\t' << to_fixed(e.velocity.x, 0) << '\t' << to_fixed(e.velocity.y, 0)
        << '\t' << to_fixed(f.bot.walk_yaw, 1) << '\t' << to_fixed(f.bot.look_yaw, 1) << '\t'
        << f.press_note << '\t' << p.k << '\t';
    // The route's next node (where the pilot heads), '-' with no route.
    if (p.g != nullptr && p.k + 1 < p.route.size()) {
      const sh::Vec3f nx = p.g->nodes[static_cast<size_t>(p.route[p.k + 1])].origin;
      out << to_fixed(nx.x, 0) << '\t' << to_fixed(nx.y, 0) << '\t';
    } else {
      out << "-\t-\t";
    }
    out << to_fixed(f.keys.wish_forward, 0) << '\t' << to_fixed(f.keys.wish_side, 0) << '\t'
        << f.keys.set << '\t' << link2_of(p) << '\t'
        << (p.g != nullptr && p.k + 1 < p.route.size() &&
                    p.g->nodes[static_cast<size_t>(p.route[p.k + 1])].wet
                ? 1
                : 0)
        << '\t' << p.recipe_note << '\t' << ((e.flags & sh::kFlagOnGround) != 0 ? 1 : 0) << '\t'
        << ((cmd.buttons & sh::kButtonJump) != 0 ? 1 : 0) << '\t';
    // The heading the air steers for: a hop's aim, a walk's target in the air (`airwalk`).
    if (p.mode == sh::PilotMode::hop)
      out << to_fixed(p.hop_aim_yaw, 1) << '\t';
    else if (p.walk_air)
      out << to_fixed(p.walk_air_yaw, 1) << '\t';
    else
      out << "-\t";
    // Who the aim is on, where he is and how he moves ('-' with no aim): a fight's way read
    // against him (is he chasing?).
    const int32_t tg = f.bot.aiming ? f.bot.target : -1;
    if (tg >= 1 && static_cast<size_t>(tg) < w.world.entities.size()) {
      const auto& te = w.world.entities[static_cast<size_t>(tg)];
      out << tg << '\t' << to_fixed(te.origin.x, 0) << '\t' << to_fixed(te.origin.y, 0) << '\t'
          << to_fixed(te.velocity.x, 0) << '\t' << to_fixed(te.velocity.y, 0) << '\t';
    } else {
      out << "-\t-\t-\t-\t-\t";
    }
    // (`fightway`) the route's back key let go: stood and dodged
    out << (std::string_view(f.press_note) == "stand" ? 1 : 0) << '\t';
    int vis = -1;  // the aimed contact in sight (1), lost (0); -1 with no aim
    for (const sh::Contact& ct : f.bot.senses.contacts)
      if (tg >= 1 && ct.ent == tg) vis = ct.visible ? 1 : 0;
    out << vis << '\t';
    // The goal item there as the fighter believes it, and as it is (-1: no item goal).
    if (f.goal_item >= 0 && static_cast<size_t>(f.goal_item) < f.items.size()) {
      const auto& it = f.items[static_cast<size_t>(f.goal_item)];
      const sh::Edict* ie = it.ent >= 0 ? &w.sv.edicts[static_cast<size_t>(it.ent)] : nullptr;
      const bool real =
          ie != nullptr && !ie->free && ie->modelindex > 0 && ie->solid != sh::kSolidNot;
      out << (it.present ? 1 : 0) << '\t' << (real ? 1 : 0) << '\t';
    } else {
      out << "-1\t-1\t";
    }
    // Its armour's protection (type times value), the rules' numbers.
    const auto& num = w.world.clients[static_cast<size_t>(f.slot)].numbers;
    out << to_fixed(num.armour_type * num.armour, 0) << '\t' << f.mind.fetch << '\t';
    // The ridden mover of the link walked or the next (its height off its rest, as last seen;
    // '-' with none): a wait read against the lift's place.
    {
      float up = 1e9f;
      for (size_t q = p.k; p.g != nullptr && q + 1 < p.route.size() && q < p.k + 2; q++) {
        const sh::NavNode& na = p.g->nodes[static_cast<size_t>(p.route[q])];
        for (int32_t j = 0; j < na.links && up > 1e8f; j++) {
          const sh::NavLink& l =
              p.g->links[static_cast<size_t>(na.first_link) + static_cast<size_t>(j)];
          if (l.to != p.route[q + 1] || l.mover < 0) continue;
          for (const sh::NavMover& m : p.g->movers)
            if (m.ent == l.mover)
              for (const auto& v : p.movers_seen)
                if (v.ent == l.mover) up = v.z - m.absmin.z;
        }
      }
      if (up > 1e8f)
        out << "-\n";
      else
        out << to_fixed(up, 0) << '\n';
    }
  };
  o.frames = static_cast<int>(std::lround(minutes * 60 * 1000 / shooters_edge::bots::kPilotMsec));
  const cn::TeamMatchReport r = cn::run_team_match(o);
  if (!r.ok) {
    lab.fail("se-team: " + r.error, "next: run tests/shooters_edge_team_test.cpp");
    return;
  }
  const double secs = r.seconds;
  hop_metrics(lab, watch);
  still_view_metrics(lab, watch);
  facing_metrics(lab, watch);
  flights_by_mode(lab, watch, r, basedir, lab.arg("game"), name, o.packed);
  if (watch.log != nullptr) write_why(lab, why_log, r);

  // The table.
  static const char* kWeapon[9] = {
      "", "blade", "scattergun", "double", "spiker", "heavy spiker", "grenades", "rockets", "arc"};
  gl::LabTable t{"each player over the match",
                 {"player", "team", "aim", "frags", "deaths", "shots", "held for a mate", "packs",
                  "hold", "rush", "carry", "push", "free", "held most"},
                 {}};
  for (const cn::TeamPlayer& p : r.players) {
    int64_t alive = 0;
    for (int k = 0; k < bt::kRoles; k++) alive += p.role_commands[k];
    const auto share = [&](int k) {
      return to_fixed(
          alive > 0 ? static_cast<double>(p.role_commands[k]) / static_cast<double>(alive) : 0, 2);
    };
    int most = 1;
    for (int w = 1; w <= 8; w++)
      if (p.weapon_commands[w] > p.weapon_commands[most]) most = w;
    t.rows.push_back({p.name, o.teams[p.team].name, sh::aim_skill_name(p.skill),
                      to_fixed(p.frags, 0), std::to_string(p.deaths), std::to_string(p.shots),
                      std::to_string(p.held_fire), std::to_string(p.packs), share(1), share(2),
                      share(3), share(4), share(0), kWeapon[most]});
    lab.metric("team." + p.name + ".frags", p.frags, "count");
    lab.metric("team." + p.name + ".deaths", p.deaths, "count");
  }
  lab.table(t);
  // The reactions drawn per sighting (shooter/aim.hpp `draw_reaction`).
  gl::LabTable rt{
      "each player's reactions", {"player", "reaction", "sightings", "mean ms", "pre-aimed"}, {}};
  int64_t sightings = 0;
  double react_s = 0;
  for (const cn::TeamPlayer& p : r.players) {
    const double n = static_cast<double>(p.reactions);
    rt.rows.push_back({p.name, std::to_string(p.traits[sh::Trait::reaction]),
                       std::to_string(p.reactions), to_fixed(n > 0 ? p.react_s / n * 1000 : 0, 0),
                       to_fixed(n > 0 ? static_cast<double>(p.pre_aimed) / n : 0, 2)});
    sightings += p.reactions;
    react_s += p.react_s;
  }
  lab.table(rt);
  // Hit rates by weapon (net/classic_hits.hpp): each player's, and each skill's against the bands
  // players of the three divisions reach.
  {
    gl::LabTable ht{"each player's hit rates", {"player", "aim"}, {}};
    for (int w = 0; w < cn::kHitWeapons; w++)
      ht.columns.push_back(cn::hit_weapon_name(static_cast<cn::HitWeapon>(w)));
    std::array<std::array<cn::HitCount, cn::kHitWeapons>, 3> by_skill{};
    const auto rate = [](const cn::HitCount& c) {
      return c.fired > 0
                 ? to_fixed(100.0 * static_cast<double>(c.hit) / static_cast<double>(c.fired), 0) +
                       "% of " + std::to_string(c.fired)
                 : std::string("-");
    };
    for (const cn::TeamPlayer& p : r.players) {
      std::vector<std::string> row{p.name, sh::aim_skill_name(p.skill)};
      for (int w = 0; w < cn::kHitWeapons; w++) row.push_back(rate(p.hits[static_cast<size_t>(w)]));
      ht.rows.push_back(row);
      const int k = static_cast<int>(p.skill) - 1;  // novice, average, expert
      if (k >= 0 && k < 3)
        for (int w = 0; w < cn::kHitWeapons; w++) {
          by_skill[static_cast<size_t>(k)][static_cast<size_t>(w)].fired +=
              p.hits[static_cast<size_t>(w)].fired;
          by_skill[static_cast<size_t>(k)][static_cast<size_t>(w)].hit +=
              p.hits[static_cast<size_t>(w)].hit;
        }
    }
    lab.table(ht);
    static const char* kSkill[3] = {"novice", "average", "expert"};
    for (int k = 0; k < 3; k++)
      for (int w = 0; w < cn::kHitWeapons; w++) {
        const cn::HitCount& c = by_skill[static_cast<size_t>(k)][static_cast<size_t>(w)];
        if (c.fired == 0) continue;
        std::string key = std::string("team.hits.") + kSkill[k] + "." +
                          cn::hit_weapon_name(static_cast<cn::HitWeapon>(w));
        for (char& ch : key)
          if (ch == ' ') ch = '-';
        lab.metric(key, 100.0 * static_cast<double>(c.hit) / static_cast<double>(c.fired), "%");
      }
  }
  {
    // Where the hitscan shots passed a moving target along his motion, by skill: + led too much.
    static const char* kSkillName[3] = {"novice", "average", "expert"};
    double sum[3] = {}, abs_sum[3] = {};
    int64_t n[3] = {};
    for (const cn::TeamPlayer& p : r.players) {
      const int k = p.skill == sh::AimSkill::novice ? 0 : p.skill == sh::AimSkill::expert ? 2 : 1;
      sum[k] += p.lead_sum;
      abs_sum[k] += p.lead_abs;
      n[k] += p.lead_n;
    }
    for (int k = 0; k < 3; k++) {
      if (n[k] == 0) continue;
      lab.metric(std::string("team.lead-miss.") + kSkillName[k], sum[k] / static_cast<double>(n[k]),
                 "units");
      lab.metric(std::string("team.lead-miss-abs.") + kSkillName[k],
                 abs_sum[k] / static_cast<double>(n[k]), "units");
    }
  }
  {
    // Of the time an enemy is in plain view: seen (in the cone), aimed at, still reacting.
    int64_t v = 0, seen = 0, aim = 0, react = 0;
    for (const cn::TeamPlayer& p : r.players) {
      v += p.in_view;
      seen += p.in_view_seen;
      aim += p.in_view_aiming;
      react += p.in_view_reacting;
    }
    const auto of = [&](int64_t k) {
      return v > 0 ? static_cast<double>(k) / static_cast<double>(v) : 0;
    };
    lab.metric("team.in-view.seen", of(seen), "share");
    lab.metric("team.in-view.aiming", of(aim), "share");
    lab.metric("team.in-view.reacting", of(react), "share");
    int64_t checks = 0, far = 0, off = 0, shots = 0;
    for (const cn::TeamPlayer& p : r.players) {
      checks += p.trigger_checks;
      far += p.trigger_far;
      off += p.trigger_off;
      shots += p.shots;
    }
    const auto per = [&](int64_t k) {
      return checks > 0 ? static_cast<double>(k) / static_cast<double>(checks) : 0;
    };
    lab.metric("team.trigger.fired", per(shots), "share");
    lab.metric("team.trigger.out-of-range", per(far), "share");
    lab.metric("team.trigger.view-off", per(off), "share");
    lab.metric("team.trigger.per-in-view",
               v > 0 ? static_cast<double>(checks) / (4.0 * static_cast<double>(v)) : 0, "share");
  }
  lab.metric("team.sightings", static_cast<double>(sightings), "count");
  lab.metric("team.reaction-mean-ms",
             sightings > 0 ? react_s / static_cast<double>(sightings) * 1000 : 0, "ms");
  gl::LabTable pt{
      "each player's legs",
      {"player", "travelled", "plans", "stuck", "lost", "waits", "barred", "dives", "given up",
       "dive s", "off graph", "walk", "wait", "arrived", "idle", "air", "hop"},
      {}};
  for (const cn::TeamPlayer& p : r.players) {
    int64_t live = 0;
    for (int k = 0; k < sh::kPilotModes; k++) live += p.pilot_modes[k];
    const auto share = [&](int k) {
      return to_fixed(
          live > 0 ? static_cast<double>(p.pilot_modes[k]) / static_cast<double>(live) : 0, 2);
    };
    pt.rows.push_back(
        {p.name, to_fixed(p.travelled, 0), std::to_string(p.plans), std::to_string(p.stuck),
         std::to_string(p.lost), std::to_string(p.waits), std::to_string(p.barred),
         std::to_string(p.dives), std::to_string(p.dives_given_up), to_fixed(p.dive_wet_s, 0),
         std::to_string(p.off_graph), share(1), share(5), share(8), share(0), share(4), share(9)});
  }
  lab.table(pt);
  {
    // The pilots' stuck and waits over every player: how often the legs failed the plan.
    int64_t stuck = 0, waits = 0;
    for (const cn::TeamPlayer& p : r.players) {
      stuck += p.stuck;
      waits += p.waits;
    }
    lab.metric("team.stuck", static_cast<double>(stuck), "count");
    lab.metric("team.waits", static_cast<double>(waits), "count");
  }
  gl::LabTable tt{
      "each team",
      {"team", "frags", "deaths", "quads taken", "rushes", "shots held", "callouts", "messages"},
      {}};
  for (int k = 0; k < 2; k++) {
    const cn::TeamTotals& tm = r.teams[k];
    tt.rows.push_back({o.teams[k].name, to_fixed(tm.frags, 0), std::to_string(tm.deaths),
                       std::to_string(tm.quad_pickups), std::to_string(tm.rushes),
                       std::to_string(tm.held_shots), std::to_string(tm.callouts),
                       std::to_string(tm.messages)});
    const std::string key = "team." + o.teams[k].name;
    lab.metric(key + ".frags", tm.frags, "count");
    lab.metric(key + ".quads", static_cast<double>(tm.quad_pickups), "count");
    lab.metric(key + ".messages", static_cast<double>(tm.messages), "count");
  }
  lab.table(tt);
  // What each player knew of the map's items, at the start, at the half and at the end.
  gl::LabTable kt{"each player's map: items known (recalled from anywhere)",
                  {"player", "knows the map as", "start", "half", "end", "of", "messages"},
                  {}};
  for (const cn::TeamPlayer& p : r.players) {
    const auto at = [&](size_t k) {
      return p.known.empty() ? 0 : p.known[std::min(k, p.known.size() - 1)];
    };
    kt.rows.push_back({p.name, bt::map_sense_name(bt::map_sense_of(p.skill)), std::to_string(at(0)),
                       std::to_string(at(p.known.size() / 2)), std::to_string(at(p.known.size())),
                       std::to_string(r.item_at.size()), std::to_string(p.said)});
  }
  lab.table(kt);
  gl::LabTable bt_{"the map's buttons: where, how, and what each opens",
                   {"button at (x y z)", "how", "doors", "opens", "used from (x y z)"},
                   {}};
  const auto xyz = [](sh::Vec3f a) {
    return to_fixed(a.x, 0) + " " + to_fixed(a.y, 0) + " " + to_fixed(a.z, 0);
  };
  for (const cn::TeamButton& b : r.buttons) {
    std::string opens;
    for (const std::string& n : b.opens) opens += (opens.empty() ? "" : ", ") + n;
    bt_.rows.push_back({xyz(b.at), b.shoot ? "shoot" : "touch", std::to_string(b.doors.size()),
                        opens.empty() ? "(a way through)" : opens, xyz(b.from)});
  }
  lab.table(bt_);
  lab.metric("team.buttons", static_cast<double>(r.buttons.size()), "count");
  gl::LabTable et{
      "each player's ears and area play",
      {"player", "ears", "sounds", "taken in", "items heard taken", "spawns heard", "spam rockets"},
      {}};
  int64_t heard_taken = 0, spawns = 0, sounds = 0, heard = 0;
  for (const cn::TeamPlayer& p : r.players) {
    et.rows.push_back({p.name, bt::map_sense_name(bt::map_sense_of(p.skill)),
                       std::to_string(p.sounds), std::to_string(p.heard),
                       std::to_string(p.heard_taken), std::to_string(p.heard_spawns),
                       std::to_string(p.spam_shots)});
    heard_taken += p.heard_taken;
    spawns += p.heard_spawns;
    sounds += p.sounds;
    heard += p.heard;
  }
  lab.table(et);
  gl::LabTable tt_{
      "each player's temperament",
      {"player", "temperament", "aim", "frags", "deaths", "outmatched s", "after sounds s"},
      {}};
  for (const cn::TeamPlayer& p : r.players) {
    tt_.rows.push_back({p.name, sh::temper_name(p.temper), sh::aim_skill_name(p.skill),
                        to_fixed(p.frags, 0), std::to_string(p.deaths),
                        to_fixed(static_cast<double>(p.outmatched) * bt::kPilotMsec / 1000, 0),
                        to_fixed(static_cast<double>(p.sound_chases) * bt::kPilotMsec / 1000, 0)});
  }
  lab.table(tt_);
  gl::LabTable traits_table{"each player's traits (1 to 10; shooter/traits.hpp)", {"player"}, {}};
  for (size_t i = 0; i < sh::kTraits; i++)
    traits_table.columns.push_back(sh::trait_name(static_cast<sh::Trait>(i)));
  for (const cn::TeamPlayer& p : r.players) {
    std::vector<std::string> row{p.name};
    for (uint8_t v : p.traits.v) row.push_back(std::to_string(v));
    traits_table.rows.push_back(row);
  }
  lab.table(traits_table);
  // The features, when a player's are not his preset's (shooter/features.hpp).
  gl::LabTable feat_table{
      "each player's bot features (shooter/features.hpp)", {"player", "features"}, {}};
  for (const cn::TeamPlayer& p : r.players)
    if (p.features != sh::features_of(p.skill))
      feat_table.rows.push_back({p.name, sh::features_text(p.features)});
  if (!feat_table.rows.empty()) lab.table(feat_table);
  lab.metric("team.sounds-taken-in", static_cast<double>(heard), "count");
  lab.metric("team.sounds-missed", static_cast<double>(sounds - heard), "count");
  lab.metric("team.items-heard-taken", static_cast<double>(heard_taken), "count");
  lab.metric("team.spawns-heard", static_cast<double>(spawns), "count");
  lab.metric("team.areas-lost-seen",
             static_cast<double>(r.teams[0].lost_seen + r.teams[1].lost_seen), "count");
  lab.metric("team.retakes", static_cast<double>(r.teams[0].retakes + r.teams[1].retakes), "count");
  lab.metric("team.fetches", static_cast<double>(r.teams[0].fetches + r.teams[1].fetches), "count");
  lab.metric("team.fetched", static_cast<double>(r.teams[0].fetched + r.teams[1].fetched), "count");
  {
    static constexpr const char* kEnd[5] = {"role", "gone", "time", "death", "retreat"};
    for (int e = 0; e < 5; e++)
      lab.metric(std::string("team.fetch-end.") + kEnd[e],
                 static_cast<double>(r.teams[0].fetch_end[e] + r.teams[1].fetch_end[e]), "count");
  }
  lab.metric("team.pushes", static_cast<double>(r.teams[0].pushes + r.teams[1].pushes), "count");
  {
    int64_t changes = 0;
    int64_t up = 0, failed = 0, swum = 0;
    for (const cn::TeamPlayer& p : r.players) {
      changes += p.goal_changes;
      up += p.surfaced;
      failed += p.surface_failed;
      swum += p.swim_fights;
    }
    lab.metric("team.surfaced", static_cast<double>(up), "count");
    // Every death's pack known (bots/packs.hpp), by how its place was known; the weapon guessed
    // right by the guesser's skill (novice, average, expert); trips to them and what ended them.
    {
      static constexpr const char* kFrom[5] = {"seen", "cry", "blast", "aim", "lying"};
      int64_t known[5] = {}, guesses[4] = {}, right[4] = {}, guess_rl[4] = {}, truth_rl[4] = {};
      int64_t trips = 0, reached = 0, gone = 0;
      for (const cn::TeamPlayer& p : r.players) {
        for (int k = 0; k < 5; k++) known[k] += p.packs_known[k];
        const auto sk = static_cast<size_t>(p.skill);
        guesses[sk] += p.pack_guesses;
        right[sk] += p.pack_right;
        guess_rl[sk] += p.pack_guess_rl;
        truth_rl[sk] += p.pack_truth_rl;
        trips += p.pack_trips;
        reached += p.pack_reached;
        gone += p.pack_seen_gone;
      }
      for (int k = 0; k < 5; k++)
        lab.metric(std::string("team.packs.known.") + kFrom[k], static_cast<double>(known[k]),
                   "count");
      for (int k = 1; k < 4; k++) {  // by the guesser's skill (none plays `perfect` here)
        const std::string pre =
            std::string("team.packs.guess.") + sh::aim_skill_name(static_cast<sh::AimSkill>(k));
        lab.metric(pre + ".count", static_cast<double>(guesses[k]), "count");
        lab.metric(pre + ".right",
                   guesses[k] ? static_cast<double>(right[k]) / static_cast<double>(guesses[k]) : 0,
                   "share");
        lab.metric(pre + ".rl-guessed", static_cast<double>(guess_rl[k]), "count");
        lab.metric(pre + ".rl-true", static_cast<double>(truth_rl[k]), "count");
      }
      lab.metric("team.packs.trips", static_cast<double>(trips), "count");
      lab.metric("team.packs.reached", static_cast<double>(reached), "count");
      lab.metric("team.packs.seen-gone", static_cast<double>(gone), "count");
    }
    lab.metric("team.swim-fights", static_cast<double>(swum), "count");
    // The share of the time alive waist deep in water (people's recordings: 9 %, se-people `wet`).
    int64_t wet = 0, alive = 0;
    for (const cn::TeamPlayer& p : r.players) {
      wet += p.wet;
      for (int k = 0; k < 4; k++) alive += p.mode_commands[k];
    }
    lab.metric("team.wet", alive > 0 ? static_cast<double>(wet) / static_cast<double>(alive) : 0,
               "share");
    // Shots and damage by the target's state (net/classic_hits.hpp): the pellets' and the rockets'
    // hits over those fired at a target in that state, the damage he took a minute lived in it,
    // and the share of the time lived in it.
    {
      int64_t all = 0;
      for (const int64_t f : r.frames_at) all += f;
      for (int k = 0; k < cn::kTargetStates; k++) {
        const auto i = static_cast<size_t>(k);
        const std::string key = std::string("team.target.") + cn::kTargetStateNames[k];
        const auto rate = [](const cn::HitCount& c) {
          return c.fired > 0 ? static_cast<double>(c.hit) / static_cast<double>(c.fired) : 0;
        };
        // (player-minutes: a frame lasts the match's seconds over its frames)
        const double lived = static_cast<double>(r.frames_at[i]) * r.seconds /
                             std::max<double>(1, static_cast<double>(r.frames)) / 60;
        lab.metric(key + ".pellets", rate(r.pellets_at[i]), "share");
        lab.metric(key + ".rockets", rate(r.rockets_at[i]), "share");
        lab.metric(key + ".rockets-fired", static_cast<double>(r.rockets_at[i].fired), "count");
        lab.metric(key + ".damage-per-min", lived > 0 ? r.given_at[i] / lived : 0, "hp/min");
        lab.metric(key + ".time",
                   all > 0 ? static_cast<double>(r.frames_at[i]) / static_cast<double>(all) : 0,
                   "share");
      }
    }
    // The key items lying untaken (classic_team.hpp), and what the fighters made of it: nobody
    // believed it there, nobody wanted it, the one who wanted it was fighting, one was on his way.
    for (int k = 0; k < cn::kLyingItems; k++) {
      const auto ki = static_cast<size_t>(k);
      if (r.lying_n[ki] == 0) continue;
      const std::string key = std::string("team.lying.") + cn::kLyingItemNames[k];
      lab.metric(key, static_cast<double>(r.lying[ki]) / static_cast<double>(r.lying_n[ki]),
                 "share");
      lab.metric(key + ".takers",
                 r.lying[ki] > 0 ? static_cast<double>(r.lying_takers[ki]) /
                                       static_cast<double>(r.lying[ki])
                                 : 0,
                 "share");
      for (int w = 0; w < cn::kLyingWhy; w++)
        lab.metric(key + "." + cn::kLyingWhyNames[w],
                   r.lying[ki] > 0 ? static_cast<double>(r.lying_why[ki][static_cast<size_t>(w)]) /
                                         static_cast<double>(r.lying[ki])
                                   : 0,
                   "share");
    }
    // The wet time by cause (bots/fighter.hpp kWetWhy), as shares of it: what keeps the bots in
    // the water; and wet aiming, swimming and walking, each's share, speed and time still.
    for (size_t w = 0; w < 12; w++) {
      int64_t why = 0;
      for (const cn::TeamPlayer& p : r.players) why += p.wet_why[w];
      lab.metric(std::string("team.wet-why.") + bt::kWetWhy[w],
                 wet > 0 ? static_cast<double>(why) / static_cast<double>(wet) : 0, "share");
    }
    static const char* kWetHow[3] = {"aiming", "swim", "walk"};
    for (size_t w = 0; w < 3; w++) {
      double sp = 0;
      int64_t n = 0, still = 0;
      for (const cn::TeamPlayer& p : r.players) {
        sp += p.wet_speed[w];
        n += p.wet_n[w];
        still += p.wet_still[w];
      }
      const std::string k = std::string("team.wet-") + kWetHow[w];
      lab.metric(k + ".share", wet > 0 ? static_cast<double>(n) / static_cast<double>(wet) : 0,
                 "share");
      lab.metric(k + ".speed", n > 0 ? sp / static_cast<double>(n) : 0, "units/s");
      lab.metric(k + ".still", n > 0 ? static_cast<double>(still) / static_cast<double>(n) : 0,
                 "share");
    }
    // Pickups by kind a player-minute, set beside people's (a match's statistics count them so).
    static const char* kKinds[12] = {"health", "mega",    "armor1", "armor2", "armor3", "shells",
                                     "nails",  "rockets", "cells",  "weapon", "quad",   "pent"};
    int64_t ways = 0;
    for (const cn::TeamPlayer& p : r.players) ways += p.on_the_way;
    lab.metric("team.on-the-way", static_cast<double>(ways), "count");
    const double player_min = static_cast<double>(r.players.size()) * r.seconds / 60.0;
    for (size_t k = 0; k < 12; k++) {
      int64_t n = 0;
      for (const cn::TeamPlayer& p : r.players) n += p.took[k];
      lab.metric(std::string("team.took-per-min.") + kKinds[k],
                 player_min > 0 ? static_cast<double>(n) / player_min : 0, "per-min");
    }
    lab.metric("team.surface-failed", static_cast<double>(failed), "count");
    lab.metric("team.goal-changes-per-min",
               static_cast<double>(changes) /
                   std::max(1.0, static_cast<double>(r.players.size()) * minutes),
               "per player-min");
  }
  lab.metric("team.push-joins", static_cast<double>(r.teams[0].joins + r.teams[1].joins), "count");
  lab.metric("team.spam-rockets",
             static_cast<double>(r.teams[0].spam_shots + r.teams[1].spam_shots), "count");
  // The team messages: the first lines in the report, all of them as a file.
  {
    std::string all;
    for (const std::string& l : r.chat) all += l + "\n";
    lab.attach("chat.txt", std::vector<uint8_t>(all.begin(), all.end()), gl::Det::deterministic);
    for (size_t k = 0; k < r.chat.size() && k < 24; k++) lab.note(r.chat[k]);
  }
  // The deaths by how (the server's obituaries, the names taken out): what kills the players.
  {
    std::vector<std::string> names;
    names.reserve(r.players.size());
    for (const cn::TeamPlayer& p : r.players) names.push_back(p.name);
    std::map<std::string, int> how;
    std::string all;
    for (std::string l : r.prints) {
      all += l;
      while (!l.empty() && (l.back() == '\n' || l.back() == ' ')) l.pop_back();
      bool death = false;
      for (const std::string& n : names)
        for (size_t at = l.find(n); at != std::string::npos; at = l.find(n, at + 1)) {
          const bool word =
              (at == 0 || l[at - 1] == ' ') &&
              (at + n.size() == l.size() || l[at + n.size()] == ' ' || l[at + n.size()] == '\'');
          if (!word) continue;
          l.replace(at, n.size(), "*");
          death = true;
        }
      if (death && l.size() > 2 && l.find(": ") == std::string::npos) how[l]++;
    }
    lab.attach("prints.txt", std::vector<uint8_t>(all.begin(), all.end()), gl::Det::deterministic);
    std::string dl =
        "t\tplayer\tat\tliquid\tmode\tlink\tat 3 s before\tits mode\tthe way in (2, 1, 0.5 s "
        "before)\n";
    for (const std::string& l : r.deaths) dl += l + "\n";
    lab.attach("deaths.tsv", std::vector<uint8_t>(dl.begin(), dl.end()), gl::Det::deterministic);
    // The deaths in a liquid (a row's fourth field): in lava or slime, the legs' fault.
    for (const std::string_view liquid : {"lava", "slime", "water"}) {
      int n = 0;
      for (const std::string& l : r.deaths) {
        size_t from = 0;
        for (int field = 0; field < 3 && from != std::string::npos; field++) {
          from = l.find('\t', from);
          if (from != std::string::npos) from++;
        }
        if (from == std::string::npos) continue;
        const size_t to = l.find('\t', from);
        if (std::string_view(l).substr(from, to - from) == liquid) n++;
      }
      lab.metric("team.deaths-in." + std::string(liquid), n, "count");
    }
    std::vector<std::pair<int, std::string>> by;
    by.reserve(how.size());
    for (const auto& [k, v] : how) by.push_back({-v, k});
    std::sort(by.begin(), by.end());
    gl::LabTable dt{
        "the deaths by how (the server's obituaries, * a player)", {"how", "count"}, {}};
    for (size_t k = 0; k < by.size() && k < 16; k++)
      dt.rows.push_back({by[k].second, std::to_string(-by[k].first)});
    lab.table(dt);
  }
  lab.metric("team.frags-per-minute", (r.teams[0].frags + r.teams[1].frags) / (secs / 60),
             "per-min");
  // The dodge (shooter/dodge.hpp): the share of the fight spent side-stepping, its turns a minute,
  // and every weapon's hits over its shots (what the dodge is for).
  {
    int64_t dodging = 0, turns = 0, fight = 0, fired = 0, hit = 0, watching = 0, alive = 0;
    int64_t reacted = 0, pre = 0, kept = 0, swipes = 0;
    for (const cn::TeamPlayer& p : r.players) {
      watching += p.watching;
      kept += p.target_kept;
      swipes += p.sound_swipes;
      reacted += p.reactions;
      pre += p.pre_aimed;
      for (int k = 0; k < 4; k++) alive += p.mode_commands[k];
      dodging += p.dodging;
      turns += p.dodge_turns;
      fight += p.mode_commands[static_cast<int>(bt::FightMode::fight)];
      for (const cn::HitCount& c : p.hits) {
        fired += c.fired;
        hit += c.hit;
      }
    }
    // The mind's modes: the share of the time alive roaming, fighting, chasing and retreating.
    {
      int64_t by[4] = {};
      for (const cn::TeamPlayer& p : r.players)
        for (int k = 0; k < 4; k++) by[k] += p.mode_commands[k];
      for (int k = 0; k < 4; k++)
        lab.metric(std::string("team.mode.") + bt::fight_mode_name(static_cast<bt::FightMode>(k)),
                   alive > 0 ? static_cast<double>(by[k]) / static_cast<double>(alive) : 0,
                   "share");
    }
    {
      // People's on dm2 (the se-rj lab): an expert rocket jumps 1.9 times in ten minutes.
      int64_t rj = 0, fl = 0;
      for (const cn::TeamPlayer& p : r.players) rj += p.rocket_jumps, fl += p.flips;
      lab.metric("team.rocket-jumps", static_cast<double>(rj), "count");
      lab.metric("team.flips", static_cast<double>(fl), "count");
      int64_t es = 0, eo = 0;
      for (const cn::TeamPlayer& p : r.players) es += p.escapes, eo += p.escapes_out;
      lab.metric("team.escapes", static_cast<double>(es), "count");
      lab.metric("team.escapes-out", static_cast<double>(eo), "count");
      // The gap tricks (shooter/nav_gaps.hpp): jumped out of a hop, landed across, given up.
      // People's on dm2 (the se-rj lab): the YA pit's about 6 a match, the RA pit's some 40.
      int64_t gj = 0, gl = 0, gr = 0;
      for (const cn::TeamPlayer& p : r.players)
        gj += p.gap_jumps, gl += p.gap_landed, gr += p.gap_refused;
      lab.metric("team.gap-jumps", static_cast<double>(gj), "count");
      lab.metric("team.gap-landed", static_cast<double>(gl), "count");
      lab.metric("team.gap-refused", static_cast<double>(gr), "count");
      // The hops (shooter/pilot.hpp): stretches entered, and jumps pressed on their landings.
      int64_t hs = 0, hj = 0;
      for (const cn::TeamPlayer& p : r.players) hs += p.hops, hj += p.hop_jumps;
      lab.metric("team.hops", static_cast<double>(hs), "count");
      lab.metric("team.hop-jumps", static_cast<double>(hj), "count");
      // The circle jumps (`circlejump`): hops started out of a curve, and their takeoff speed
      // (people's standing jumps: 433 at the median on dm2).
      int64_t cj = 0;
      double cs = 0;
      for (const cn::TeamPlayer& p : r.players) cj += p.circles, cs += p.circle_speed;
      lab.metric("team.circle-jumps", static_cast<double>(cj), "count");
      lab.metric("team.circle-speed", cj > 0 ? cs / static_cast<double>(cj) : 0, "u/s");
      // People's hop starts played (`recipes`): looked for, proven and begun, aborted off the
      // proof's way, and those that left the ground (their takeoff speed: people's ~437).
      int64_t ra = 0, rc = 0, rb = 0, rto = 0;
      double rs = 0;
      for (const cn::TeamPlayer& p : r.players)
        ra += p.recipe_asks, rc += p.recipe_commits, rb += p.recipe_aborts,
            rto += p.recipe_takeoffs, rs += p.recipe_speed;
      lab.metric("team.recipe.asks", static_cast<double>(ra), "count");
      lab.metric("team.recipe.commits", static_cast<double>(rc), "count");
      lab.metric("team.recipe.aborts", static_cast<double>(rb), "count");
      lab.metric("team.recipe.takeoffs", static_cast<double>(rto), "count");
      lab.metric("team.recipe.speed", rto > 0 ? rs / static_cast<double>(rto) : 0, "u/s");
      // Goals chained on past a pickup (`chains`): asked, planned, the pickups run through (and
      // the speed through them) and run past.
      int64_t ch = 0, cv = 0, ct = 0, cm = 0;
      double cvs = 0;
      for (const cn::TeamPlayer& p : r.players)
        ch += p.chained, cv += p.vias, ct += p.via_touched, cm += p.via_missed, cvs += p.via_speed;
      lab.metric("team.chain.asked", static_cast<double>(ch), "count");
      lab.metric("team.chain.planned", static_cast<double>(cv), "count");
      lab.metric("team.chain.through", static_cast<double>(ct), "count");
      lab.metric("team.chain.missed", static_cast<double>(cm), "count");
      lab.metric("team.chain.speed", ct > 0 ? cvs / static_cast<double>(ct) : 0, "u/s");
      // New goals run on to from the route under way (`retarget`), a match.
      int64_t aims = 0;
      for (const cn::TeamPlayer& p : r.players) aims += p.retargets;
      lab.metric("team.retargets", static_cast<double>(aims), "count");
      int64_t rounds = 0;
      for (const cn::TeamPlayer& p : r.players) rounds += p.turned_round;
      lab.metric("team.turned-round", static_cast<double>(rounds), "count");
      // The jumps of the route (not the hop's): run-ups begun again (a run at a jump given up and
      // lined up behind it once more), the jumps pressed, and of them those flown from off the
      // line or the speed (`jumpfly`), a match.
      int64_t ups = 0, jumps = 0, flown = 0;
      for (const cn::TeamPlayer& p : r.players)
        ups += p.run_ups, jumps += p.jumps, flown += p.jumps_flown;
      lab.metric("team.jump.run-ups", static_cast<double>(ups), "count");
      lab.metric("team.jump.jumps", static_cast<double>(jumps), "count");
      lab.metric("team.jump.flown", static_cast<double>(flown), "count");
    }
    lab.metric("team.dodge-share",
               fight > 0 ? static_cast<double>(dodging) / static_cast<double>(fight) : 0, "share");
    // The press (footwork.hpp `press`): of the commands it was weighed, the shares pressed on at
    // him, back, and held off by his nearness or a step not clear.
    {
      int64_t w = 0, on = 0, back = 0, near = 0, blocked = 0;
      for (const cn::TeamPlayer& p : r.players) {
        w += p.press_weighed;
        on += p.press_on;
        back += p.press_back;
        near += p.press_near;
        blocked += p.press_blocked;
      }
      if (w > 0) {
        const auto of = [&](int64_t k) { return static_cast<double>(k) / static_cast<double>(w); };
        lab.metric("team.press.fight-share",
                   fight > 0 ? static_cast<double>(w) / static_cast<double>(fight) : 0, "share");
        lab.metric("team.press.on-share", of(on), "share");
        lab.metric("team.press.back-share", of(back), "share");
        lab.metric("team.press.near-share", of(near), "share");
        lab.metric("team.press.blocked-share", of(blocked), "share");
      }
    }
    lab.metric("team.dodge-turns-per-min",
               static_cast<double>(turns) / (secs / 60) / static_cast<double>(r.players.size()),
               "per-min");
    double speed = 0;
    for (const cn::TeamPlayer& p : r.players) speed += p.speed_sum;
    // People's 4on4 average on this kind of map: 284 units/s (KTX's match stats).
    lab.metric("team.speed-avg", alive > 0 ? speed / static_cast<double>(alive) : 0, "units/s");
    // Damage given a player-minute: people's 4on4 on e1m2, 398 (KTX's `given`).
    double given = 0;
    for (const cn::TeamPlayer& p : r.players) given += p.given;
    int64_t vn = 0, iv = 0, sn = 0, un = 0;
    for (const cn::TeamPlayer& p : r.players) {
      vn += p.view_n;
      iv += p.in_view;
      sn += p.seen_n;
      un += p.unnoticed;
    }
    const auto of_alive = [&](int64_t k) {
      return vn > 0 ? static_cast<double>(k) / static_cast<double>(vn) : 0;
    };
    int64_t rlp = 0, rls = 0, rll = 0, rockets = 0;
    for (const cn::TeamPlayer& p : r.players) {
      rlp += p.rl_pickups;
      rls += p.rl_samples;
      rll += p.rl_loaded;
      rockets += p.hits[static_cast<size_t>(cn::HitWeapon::rocket_direct)].fired;
    }
    // People's 4on4 on e1m2: about 30 launchers taken in 10 minutes, 3.4 rockets a player-minute.
    lab.metric("team.rl-pickups", static_cast<double>(rlp), "count");
    lab.metric("team.rl-had", of_alive(rls), "share");
    lab.metric("team.rl-loaded", of_alive(rll), "share");
    lab.metric("team.rockets-per-min",
               static_cast<double>(rockets) / (secs / 60) /
                   static_cast<double>(std::max<size_t>(r.players.size(), 1)),
               "per-min");
    lab.metric("team.enemy-in-view", of_alive(iv), "share");
    lab.metric("team.enemy-seen", of_alive(sn), "share");
    lab.metric("team.roam-enemy-in-view", of_alive(un), "share");
    lab.metric("team.damage-per-min",
               given / (secs / 60) / static_cast<double>(std::max<size_t>(r.players.size(), 1)),
               "per-min");
    // Each side's fight apart, when the sides' features differ (an A/B by team): what one side
    // does better than the other in the same matches.
    bool differ = lab.arg("sides") == "1";
    for (const cn::TeamPlayer& p : r.players) differ |= p.features != r.players.front().features;
    for (int side = 0; side < 2 && differ; side++) {
      double dmg = 0, react = 0, s_fired = 0, s_hit = 0, s_speed = 0;
      int64_t n = 0, reactions = 0, s_pre = 0, view_n = 0, in_view = 0, seen = 0, checks = 0,
              off = 0, shots = 0, deaths = 0, s_wet = 0, s_alive = 0;
      for (const cn::TeamPlayer& p : r.players) {
        if (p.team != side) continue;
        n++;
        dmg += p.given;
        react += p.react_s;
        reactions += p.reactions;
        s_pre += p.pre_aimed;
        view_n += p.view_n;
        in_view += p.in_view;
        seen += p.in_view_seen;
        checks += p.trigger_checks;
        off += p.trigger_off;
        shots += p.shots;
        deaths += p.deaths;
        s_wet += p.wet;
        for (int m = 0; m < 4; m++) s_alive += p.mode_commands[m];
        s_speed += p.speed_sum;
        for (const cn::HitCount& h : p.hits) {
          s_fired += static_cast<double>(h.fired);
          s_hit += static_cast<double>(h.hit);
        }
      }
      const auto ratio = [](double a, double b) { return b > 0 ? a / b : 0; };
      const std::string k = std::string("team.side.") + (side == 0 ? "red." : "blue.");
      lab.metric(k + "deaths", static_cast<double>(deaths), "count");
      lab.metric(k + "speed-avg", ratio(s_speed, static_cast<double>(s_alive)), "units/s");
      lab.metric(k + "wet", ratio(static_cast<double>(s_wet), static_cast<double>(s_alive)),
                 "share");
      lab.metric(k + "damage-per-min", ratio(dmg, secs / 60 * static_cast<double>(n)), "per-min");
      lab.metric(k + "accuracy", 100 * ratio(s_hit, s_fired), "%");
      lab.metric(k + "sightings", static_cast<double>(reactions), "count");
      lab.metric(k + "pre-aimed", ratio(static_cast<double>(s_pre), static_cast<double>(reactions)),
                 "share");
      lab.metric(k + "reaction-mean-ms", 1000 * ratio(react, static_cast<double>(reactions)), "ms");
      lab.metric(k + "enemy-in-view",
                 ratio(static_cast<double>(in_view), static_cast<double>(view_n)), "share");
      lab.metric(k + "in-view.seen", ratio(static_cast<double>(seen), static_cast<double>(in_view)),
                 "share");
      lab.metric(k + "trigger.fired",
                 ratio(static_cast<double>(shots), static_cast<double>(checks)), "share");
      int64_t unseen[4] = {}, unseen_calm = 0, unseen_n = 0;
      for (const cn::TeamPlayer& p : r.players) {
        if (p.team != side) continue;
        for (int b = 0; b < 4; b++) unseen[b] += p.unseen_off[b];
        unseen_calm += p.unseen_calm;
      }
      for (int64_t u : unseen) unseen_n += u;
      const char* kBands[4] = {"under-60", "60-90", "90-135", "behind"};
      for (int b = 0; b < 4; b++)
        lab.metric(k + "unseen." + kBands[b],
                   ratio(static_cast<double>(unseen[b]), static_cast<double>(unseen_n)), "share");
      int64_t mv[6] = {}, mo[6] = {}, mn = 0, moff = 0;
      for (const cn::TeamPlayer& p : r.players) {
        if (p.team != side) continue;
        for (int h = 0; h < 6; h++) {
          mv[h] += p.moving_n[h];
          mo[h] += p.moving_off[h];
        }
      }
      for (int h = 0; h < 6; h++) {
        mn += mv[h];
        moff += mo[h];
      }
      const char* kHolders[6] = {"aim", "look", "calm", "walk", "fly", "other"};
      int64_t cro = 0;
      for (const cn::TeamPlayer& p : r.players)
        if (p.team == side) cro += p.calm_route_off;
      lab.metric(k + "moving.calm.route-off45",
                 ratio(static_cast<double>(cro), static_cast<double>(mv[2])), "share");
      lab.metric(k + "moving.off45", ratio(static_cast<double>(moff), static_cast<double>(mn)),
                 "share");
      for (int h = 0; h < 6; h++) {
        lab.metric(k + "moving." + kHolders[h],
                   ratio(static_cast<double>(mv[h]), static_cast<double>(mn)), "share");
        lab.metric(k + "moving." + kHolders[h] + ".off45",
                   ratio(static_cast<double>(mo[h]), static_cast<double>(mv[h])), "share");
      }
      lab.metric(k + "unseen.calm",
                 ratio(static_cast<double>(unseen_calm), static_cast<double>(unseen_n)), "share");
      lab.metric(k + "trigger.view-off",
                 ratio(static_cast<double>(off), static_cast<double>(checks)), "share");
    }
    // Where the speed goes: each pilot mode's share of the commands alive and its mean speed.
    gl::LabTable modes{
        "the pilot's modes (aiming apart): share of the time alive and mean speed (units/s)",
        {"mode", "share", "speed"},
        {}};
    for (int m = 0; m <= sh::kPilotModes; m++) {
      double sum = 0;
      int64_t n = 0;
      for (const cn::TeamPlayer& p : r.players) {
        sum += p.mode_speed[m];
        n += p.mode_n[m];
      }
      if (n == 0) continue;
      modes.rows.push_back(
          {m == sh::kPilotModes ? "aiming" : sh::pilot_mode_name(static_cast<sh::PilotMode>(m)),
           to_fixed(alive > 0 ? static_cast<double>(n) / static_cast<double>(alive) : 0, 2),
           to_fixed(sum / static_cast<double>(n), 0)});
    }
    lab.table(modes);
    lab.metric("team.watch-share",
               alive > 0 ? static_cast<double>(watching) / static_cast<double>(alive) : 0, "share");
    lab.metric("team.pre-aimed-share",
               reacted > 0 ? static_cast<double>(pre) / static_cast<double>(reacted) : 0, "share");
    lab.metric("team.target-kept", static_cast<double>(kept), "count");
    lab.metric("team.sound-swipes", static_cast<double>(swipes), "count");
    // What the match was fought with: each weapon's share of the commands holding one.
    int64_t held[9] = {}, all = 0;
    for (const cn::TeamPlayer& p : r.players)
      for (int w = 1; w <= 8; w++) {
        held[w] += p.weapon_commands[w];
        all += p.weapon_commands[w];
      }
    static const char* kHeld[9] = {"",      "blade",    "scattergun", "double", "spiker",
                                   "heavy", "grenades", "rockets",    "arc"};
    for (int w = 1; w <= 8; w++)
      lab.metric(std::string("team.held.") + kHeld[w],
                 all > 0 ? static_cast<double>(held[w]) / static_cast<double>(all) : 0, "share");
    lab.metric("team.accuracy",
               fired > 0 ? 100.0 * static_cast<double>(hit) / static_cast<double>(fired) : 0, "%");
  }
  lab.metric("team.faults", static_cast<double>(r.faults), "count");
  lab.metric("team.place-names", static_cast<double>(r.locs), "count");
  lab.note("place names: " + std::to_string(r.locs) + " from " + r.locs_from);
  lab.note("the map's team plan (bots/map_plan.hpp):\n" + r.plan);
  lab.note("the map's goals (bots/goals.hpp):\n" + r.goals);
  lab.metric("team.plan-areas", r.plan_areas, "count");
  lab.metric("team.lines-read", static_cast<double>(r.lines_read), "count");
  lab.metric("team.lines-missed", static_cast<double>(r.lines_missed), "count");
  lab.metric("team.nav-nodes", static_cast<double>(r.nav_nodes), "count");
  lab.metric("team.teleport-links", static_cast<double>(r.teleport_links), "count");
  lab.metric("team.movers", static_cast<double>(r.movers), "count");
  lab.metric("team.closed-links", static_cast<double>(r.closed_links), "count");
  lab.metric("team.swim-wet-cells", static_cast<double>(r.swim_wet), "count");
  lab.metric("team.swim-leading-cells", static_cast<double>(r.swim_leading), "count");
  lab.metric("team.dive-pockets", static_cast<double>(r.dives), "count");
  {
    int64_t begun = 0, given_up = 0;
    double wet_s = 0;
    for (const cn::TeamPlayer& p : r.players) {
      begun += p.dives;
      given_up += p.dives_given_up;
      wet_s += p.dive_wet_s;
    }
    lab.metric("team.dives", static_cast<double>(begun), "count");
    lab.metric("team.dives-given-up", static_cast<double>(given_up), "count");
    lab.metric("team.dive-wet-s", wet_s, "s");
  }
  {
    double lag = 0;
    int64_t n = 0;
    for (const cn::TeamPlayer& p : r.players) {
      lag += p.lag_s;
      n += p.lag_n;
    }
    if (n > 0) lab.metric("team.round-trip-ms", 1000 * lag / static_cast<double>(n), "ms");
  }
  lab.metric("team.nav-build-ms", r.nav_build_ms, "ms", gl::Det::measured);
  lab.metric("team.mvd-bytes", static_cast<double>(r.mvd.size()), "bytes");
  for (int key = 0; key < static_cast<int>(r.item_at.size()); key++) {
    if (key != r.hold_item && key != r.quad_item && lab.arg("items") != "all") continue;
    const sh::Vec3f a = r.item_at[static_cast<size_t>(key)],
                    b = r.item_node_at[static_cast<size_t>(key)];
    float nearest = 1e9f;
    for (const cn::TeamPlayer& pl : r.players)
      for (const sh::Vec3f& q : pl.path) {
        const sh::Vec3f d = q - a;
        nearest = std::min(nearest, std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z));
      }
    lab.note(r.item_name[static_cast<size_t>(key)] + " at " + to_fixed(a.x, 0) + " " +
             to_fixed(a.y, 0) + " " + to_fixed(a.z, 0) + ", its node at " + to_fixed(b.x, 0) + " " +
             to_fixed(b.y, 0) + " " + to_fixed(b.z, 0) + ", the nearest a player came " +
             to_fixed(nearest, 0));
  }
  std::string unrouted;
  for (size_t i = 0; i < r.item_routed.size(); i++)
    if (!r.item_routed[i]) unrouted += (unrouted.empty() ? "" : ", ") + r.item_name[i];
  lab.note("items no route leads to from the map's main part: " +
           (unrouted.empty() ? "none" : unrouted));
  for (const cn::TeamPlayer& pl : r.players) lab.note(pl.name + ": " + pl.last);
  char hex[24];
  std::snprintf(hex, sizeof hex, "%016llx", static_cast<unsigned long long>(r.digest));
  lab.note(std::string("digest ") + hex);

  // The map from above.
  View v{r.lo, r.hi};
  const double span = std::max(r.hi.x - r.lo.x, r.hi.y - r.lo.y);
  v.scale = span > 0 ? 1400.0 / span : 1;
  const int w = static_cast<int>((r.hi.x - r.lo.x) * v.scale) + 2 * v.margin;
  const int h = static_cast<int>((r.hi.y - r.lo.y) * v.scale) + 2 * v.margin;
  gl::Picture p = gl::create_picture(w, h, {236, 236, 230, 255});
  // The floor: every nav node, shaded by height; red where the first spawn cannot reach it.
  {
    const float zspan = std::max(1.f, r.hi.z - r.lo.z);
    for (size_t i = 0; i < r.nav_points.size(); i++) {
      const sh::Vec3f a = r.nav_points[i];
      const auto g = static_cast<uint8_t>(205 - 80 * std::clamp((a.z - r.lo.z) / zspan, 0.f, 1.f));
      const bool reached = i < r.nav_reached.size() && r.nav_reached[i];
      const gl::Rgba c = reached ? gl::Rgba{g, g, static_cast<uint8_t>(g - 10), 255}
                                 : gl::Rgba{240, 170, 160, 255};
      const int sz = std::max(2, static_cast<int>(32 * v.scale) - 1);
      gl::fill_rect(p, static_cast<int>(v.px(a.x - 16)), static_cast<int>(v.py(a.y + 16)), sz, sz,
                    c);
    }
  }
  for (const cn::TeamPlayer& pl : r.players) {
    gl::Rgba c = kTeam[pl.team];
    c.a = 110;
    for (size_t i = 0; i + 1 < pl.path.size(); i++) {
      const sh::Vec3f a = pl.path[i], b = pl.path[i + 1];
      if (pl.roles[i] == 255 || pl.roles[i + 1] == 255) continue;
      const sh::Vec3f d = a - b;
      if (d.x * d.x + d.y * d.y > 160 * 160) continue;  // a respawn or a teleport
      gl::draw_line(p, v.px(a.x), v.py(a.y), v.px(b.x), v.py(b.y), c, 2);
    }
  }
  // The map's places by name (the team's callouts), each at its points' middle.
  for (const sh::Loc& l : r.places) {
    const int lw = gl::text_width(l.name, 2), lx = static_cast<int>(v.px(l.at.x)) - lw / 2,
              ly = static_cast<int>(v.py(l.at.y)) - 30;
    gl::fill_rect(p, lx - 3, ly - 3, lw + 6, 20, gl::Rgba{255, 255, 255, 220});
    gl::draw_text(p, lx, ly, l.name, gl::Rgba{40, 70, 150, 255}, 2);
  }
  for (size_t i = 0; i < r.item_at.size(); i++) {
    const sh::Vec3f a = r.item_at[i];
    const bool key = static_cast<int>(i) == r.hold_item || static_cast<int>(i) == r.quad_item;
    const int s = key ? 7 : 3;
    gl::fill_rect(p, static_cast<int>(v.px(a.x)) - s, static_cast<int>(v.py(a.y)) - s, 2 * s, 2 * s,
                  key ? gl::Rgba{250, 210, 40, 255} : gl::Rgba{250, 250, 240, 255});
    gl::draw_ring(p, v.px(a.x), v.py(a.y), s + 2, kInk);
    std::string n = r.item_name[i];
    for (const char* pre : {"weapon_", "item_"})
      if (n.rfind(pre, 0) == 0) n = n.substr(std::string(pre).size());
    if (key || n.find("armor") != std::string::npos || n.find("launcher") != std::string::npos ||
        n.find("super") != std::string::npos || n == "mega health")
      gl::draw_text(p, static_cast<int>(v.px(a.x)) + s + 4, static_cast<int>(v.py(a.y)) - 4, n,
                    kInk);
  }
  // The buttons: a square (red for a shot, blue for a touch), a line to each door it opens (a
  // ring) and on to every item behind it, and a dot where a player uses it from.
  for (const cn::TeamButton& b : r.buttons) {
    const gl::Rgba c = b.shoot ? gl::Rgba{220, 40, 40, 255} : gl::Rgba{40, 90, 220, 255};
    for (const sh::Vec3f& d : b.doors) {
      gl::draw_line(p, v.px(b.at.x), v.py(b.at.y), v.px(d.x), v.py(d.y), c, 2);
      gl::draw_ring(p, v.px(d.x), v.py(d.y), 9, c);
      for (size_t i = 0; i < r.item_at.size(); i++)
        for (const std::string& n : b.opens)
          if (n == r.item_name[i]) {
            const sh::Vec3f a = r.item_at[i];
            const sh::Vec3f e = a - d;
            if (e.x * e.x + e.y * e.y < 400 * 400)
              gl::draw_line(p, v.px(d.x), v.py(d.y), v.px(a.x), v.py(a.y), c, 1);
          }
    }
    gl::fill_rect(p, static_cast<int>(v.px(b.at.x)) - 6, static_cast<int>(v.py(b.at.y)) - 6, 12, 12,
                  c);
    gl::fill_rect(p, static_cast<int>(v.px(b.from.x)) - 2, static_cast<int>(v.py(b.from.y)) - 2, 4,
                  4, c);
  }
  int x = v.margin;
  for (int k = 0; k < 2; k++) {
    const std::string label = o.teams[k].name + " team";
    gl::fill_rect(p, x, 8, 14, 6, kTeam[k]);
    gl::draw_text(p, x + 18, 7, label, kInk);
    x += 18 + gl::text_width(label) + 16;
  }
  gl::fill_rect(p, x, 6, 10, 10, {250, 210, 40, 255});
  gl::draw_text(p, x + 14, 7, "THE KEY AREAS (THE HELD ARMOUR, THE QUAD)", kInk);
  lab.shot({"map", "THE TEAM GAME FROM ABOVE",
            "From above, " + name + " on the baseline mod with teamplay for " + to_fixed(secs, 0) +
                " s: every player's trail in his team's colour, the items as the fighters know "
                "them, the two key areas in yellow.",
            "both teams' trails thick round the held armour and the quad; the rest of the map "
            "crossed on the way to weapons, not ignored",
            in},
           p);

  // Each player's role through the match.
  const int strip_w = 900, row_h = 20, pad = 120;
  gl::Picture m = gl::create_picture(
      strip_w + pad + 16, row_h * static_cast<int>(r.players.size()) + 52, {246, 246, 242, 255});
  for (size_t k = 0; k < r.players.size(); k++) {
    const std::vector<uint8_t>& rl = r.players[k].roles;
    const int y = 32 + static_cast<int>(k) * row_h;
    gl::fill_rect(m, 6, y + 2, 8, 12, kTeam[r.players[k].team]);
    gl::draw_text(m, 18, y + 5, r.players[k].name + " " + sh::aim_skill_name(r.players[k].skill),
                  kInk);
    for (int px = 0; px < strip_w; px++) {
      const size_t i = rl.size() * static_cast<size_t>(px) / static_cast<size_t>(strip_w);
      if (i >= rl.size()) break;
      gl::fill_rect(m, pad + px, y, 1, row_h - 4,
                    kRole[rl[i] == 255 ? 5 : std::min<int>(rl[i], 4)]);
    }
  }
  int lx = pad;
  static const char* kRoleName[6] = {"FREE", "HOLD", "RUSH", "CARRY THE QUAD", "PUSH", "DEAD"};
  for (int i = 0; i < 6; i++) {
    gl::fill_rect(m, lx, 10, 14, 8, kRole[i]);
    gl::draw_text(m, lx + 18, 9, kRoleName[i], kInk);
    lx += 18 + gl::text_width(kRoleName[i]) + 16;
  }
  lab.shot({"roles", "EACH PLAYER'S ROLE THROUGH THE MATCH",
            "A strip a player, left to right over the " + to_fixed(secs, 0) +
                " s: holding the armour's area, rushing the quad as it comes due, carrying it to "
                "the enemy, gathering what he lacks, or dead.",
            "one holder a team at a time; a rush of two before each quad; the carry after a rush "
            "won; no team without a holder for long",
            in},
           m);

  // The teams' frags through the match.
  gl::LineChart ch;
  ch.spikes = false;
  ch.frame = {"TEAM FRAGS THROUGH THE MATCH", name + ", seed " + lab.arg("seed"), "t (s)", "frags"};
  for (int k = 1; k >= 0; k--) {  // blue first: the chart's palette opens blue, then red
    gl::Trace tr{o.teams[k].name, {}, {}};
    for (size_t i = 0; i < r.team_frags[k].size(); i++) {
      tr.x.push_back(static_cast<double>(i * static_cast<size_t>(o.path_every)) *
                     shooters_edge::bots::kPilotMsec * 0.001);
      tr.y.push_back(r.team_frags[k][i]);
    }
    ch.traces.push_back(tr);
  }
  lab.shot(
      {"frags", ch.frame.title, "Each team's frags (kills less suicides) against the match's time.",
       "both lines climbing; steps where a quad was carried", in},
      gl::to_picture(gl::line_chart(ch).plot));

  if (!r.mvd.empty()) {
    lab.attach(o.packed ? "match.mvdz" : "match.mvd", r.mvd, gl::Det::deterministic);
    const std::string& out = save;
    if (!out.empty()) {
      make_folder_for(out);
      std::ofstream f(out, std::ios::binary);
      f.write(reinterpret_cast<const char*>(r.mvd.data()),
              static_cast<std::streamsize>(r.mvd.size()));
      lab.note(f ? "wrote " + out : "could not write " + out);
    }
  }
}

// THE SET (`--seeds=1-6`, `--variants="name:flags;name:flags"`): the A/B a bot change is judged by
// (the bots skill, § Shooter's Edge's bot plan). Every variant's match on every seed, each a child
// run of this lab with its own flags (the variant's over this run's, `--seed` the seed's), all in
// parallel on `--jobs` threads (a match is one thread; matches share nothing), read only once
// every one is done. Reported: each variant's means over its seeds as `<variant>.<metric>` (the
// metric's own key when there are no variants), the A/B table (frags by side, red-blue, frags a
// minute, damage, accuracy, the key items lying, speed, wet, the pilots' stuck and waits), and a
// row a seed. `{seed}` and `{variant}` in any flag's value (`--trace=<work>/t{variant}{seed}.tsv`,
// `--save`) are replaced per run. A head to head reads red-blue against the both-with variant's
// red-blue: the sides are not even.
struct SetRun {
  std::string variant;
  int seed = 0;
  std::vector<std::pair<std::string, std::string>> args;
  std::vector<gl::Metric> metrics;
  std::vector<std::string> failures;
};

// "1-6", "1,3,5" or "7-9,12" as the seeds, in order; empty when it does not read.
std::vector<int> parse_seeds(const std::string& text) {
  std::vector<int> out;
  size_t at = 0;
  while (at < text.size()) {
    const size_t end = std::min(text.find(',', at), text.size());
    const std::string part = text.substr(at, end - at);
    const size_t dash = part.find('-');
    const int lo = std::atoi(part.substr(0, dash).c_str());
    const int hi = dash == std::string::npos ? lo : std::atoi(part.substr(dash + 1).c_str());
    if (lo <= 0 || hi < lo || hi - lo > 1000) return {};
    for (int s = lo; s <= hi; s++) out.push_back(s);
    at = end + 1;
  }
  return out;
}

// A variant's flags ("--features=-hop --sides=1") as name/value pairs; false on a word that is no
// `--name=value` or `--name`.
bool parse_flags(const std::string& text, std::vector<std::pair<std::string, std::string>>& out) {
  size_t at = 0;
  while (at < text.size()) {
    if (text[at] == ' ') {
      at++;
      continue;
    }
    const size_t end = std::min(text.find(' ', at), text.size());
    const std::string word = text.substr(at, end - at);
    if (!word.starts_with("--") || word.size() < 3) return false;
    const size_t eq = word.find('=');
    out.emplace_back(word.substr(2, eq == std::string::npos ? std::string::npos : eq - 2),
                     eq == std::string::npos ? "1" : word.substr(eq + 1));
    at = end;
  }
  return true;
}

std::string replaced(std::string v, const std::string& variant, int seed) {
  for (const auto& [from, to] :
       {std::pair<std::string, std::string>{"{seed}", std::to_string(seed)},
        {"{variant}", variant}}) {
    for (size_t at = v.find(from); at != std::string::npos; at = v.find(from, at + to.size()))
      v.replace(at, from.size(), to);
  }
  return v;
}

void run_set(gl::Lab& lab) {
  const std::vector<int> seeds =
      parse_seeds(lab.arg("seeds").empty() ? lab.arg("seed") : lab.arg("seeds"));
  if (seeds.empty()) {
    lab.fail("se-team: --seeds=" + lab.arg("seeds") + " reads as no seeds",
             "next: --seeds=1-6, --seeds=1,3,5 or --seeds=7-12 (seeds from 1)");
    return;
  }
  // The variants, in the order given; none: one variant of this run's own flags.
  std::vector<std::pair<std::string, std::vector<std::pair<std::string, std::string>>>> variants;
  const std::string& list = lab.arg("variants");
  for (size_t at = 0; at < list.size();) {
    const size_t end = std::min(list.find(';', at), list.size());
    const std::string item = list.substr(at, end - at);
    const size_t colon = item.find(':');
    std::vector<std::pair<std::string, std::string>> flags;
    if (colon == 0 || colon == std::string::npos || !parse_flags(item.substr(colon + 1), flags)) {
      lab.fail("se-team: --variants: '" + item + "' is no name:flags",
               "next: --variants=\"on:--sides=1;off:--features=-hop --sides=1\" (a name, a colon, "
               "then the flags the variant changes; variants apart by ';')");
      return;
    }
    variants.emplace_back(item.substr(0, colon), std::move(flags));
    at = end + 1;
  }
  const bool named = !variants.empty();
  if (!named) variants.emplace_back("set", std::vector<std::pair<std::string, std::string>>{});

  std::vector<SetRun> runs;
  for (const auto& [name, flags] : variants) {
    for (const int seed : seeds) {
      SetRun s{name, seed, {}, {}, {}};
      for (const auto& [k, v] : lab.args()) {
        if (k == "seeds" || k == "variants" || k == "jobs") continue;
        s.args.emplace_back(k, k == "seed" ? std::to_string(seed) : v);
      }
      for (const auto& [k, v] : flags) {
        bool found = false;
        for (auto& [ak, av] : s.args)
          if (ak == k) {
            av = v;
            found = true;
          }
        if (!found) s.args.emplace_back(k, v);
      }
      for (auto& [k, v] : s.args) v = replaced(v, name, seed);
      runs.push_back(std::move(s));
    }
  }
  // Each run on the next free thread; a child draws no pictures (only_shots of none).
  const int jobs =
      std::max(1, lab.arg_num("jobs") > 0 ? static_cast<int>(lab.arg_num("jobs"))
                                          : static_cast<int>(std::thread::hardware_concurrency()));
  std::atomic<size_t> next{0};
  const auto work = [&] {
    for (size_t i = next++; i < runs.size(); i = next++) {
      SetRun& s = runs[i];
      gl::Lab child(lab.spec(), s.args);
      child.only_shots({"-"});
      run(child);
      s.metrics = child.metrics();
      s.failures = child.failures();
    }
  };
  {
    std::vector<std::thread> pool;
    for (int k = 1; k < std::min<int>(jobs, static_cast<int>(runs.size())); k++)
      pool.emplace_back(work);
    work();
    for (std::thread& t : pool) t.join();
  }
  for (const SetRun& s : runs)
    for (const std::string& f : s.failures)
      lab.fail("se-team: variant " + s.variant + ", seed " + std::to_string(s.seed) + ": " + f,
               "next: run that one alone: se-team --seed=" + std::to_string(s.seed) +
                   " with the variant's flags");

  // Each variant's mean of every metric all its seeds report, in the first run's order.
  const auto value = [](const SetRun& s, const std::string& key, double& out) {
    for (const gl::Metric& m : s.metrics)
      if (m.key == key) {
        out = m.value;
        return true;
      }
    return false;
  };
  std::map<std::string, std::map<std::string, double>> means;
  for (const auto& [name, flags] : variants) {
    const SetRun* first = nullptr;
    for (const SetRun& s : runs)
      if (s.variant == name && first == nullptr) first = &s;
    for (const gl::Metric& m : first->metrics) {
      double sum = 0;
      int n = 0;
      for (const SetRun& s : runs) {
        double v = 0;
        if (s.variant == name && value(s, m.key, v)) {
          sum += v;
          n++;
        }
      }
      if (n != static_cast<int>(seeds.size())) continue;
      const double mean = sum / n;
      means[name][m.key] = mean;
      lab.metric(named ? name + "." + m.key : m.key, mean, m.unit, m.det);
    }
  }
  // THE A/B TABLE: a row a variant.
  static constexpr std::pair<const char*, const char*> kCols[] = {
      {"red", "team.red.frags"},
      {"blue", "team.blue.frags"},
      {"fpm", "team.frags-per-minute"},
      {"dmg", "team.damage-per-min"},
      {"acc", "team.accuracy"},
      {"ya", "team.lying.ya"},
      {"ya.going", "team.lying.ya.going"},
      {"ya.near", "team.lying.ya.near"},
      {"mh", "team.lying.mh"},
      {"rl", "team.lying.rl"},
      {"speed", "team.speed-avg"},
      {"wet", "team.wet"},
      {"stuck", "team.stuck"},
      {"waits", "team.waits"}};
  gl::LabTable ab{
      "the set: each variant's means over seeds " + lab.arg("seeds"), {"variant", "red-blue"}, {}};
  for (const auto& [label, key] : kCols) ab.columns.emplace_back(label);
  for (const auto& [name, flags] : variants) {
    const std::map<std::string, double>& m = means[name];
    const auto get = [&](const char* key) {
      const auto it = m.find(key);
      return it == m.end() ? std::string("-") : to_fixed(it->second, 2);
    };
    std::vector<std::string> row{name};
    const auto r = m.find("team.red.frags"), b = m.find("team.blue.frags");
    row.push_back(r == m.end() || b == m.end() ? "-" : to_fixed(r->second - b->second, 1));
    for (const auto& [label, key] : kCols) row.push_back(get(key));
    ab.rows.push_back(std::move(row));
  }
  lab.table(ab);
  // A row a run: the frags by side and a minute, so a seed that swings the mean is seen.
  gl::LabTable each{"the set: each run", {"variant", "seed", "red", "blue", "fpm", "speed"}, {}};
  for (const SetRun& s : runs) {
    std::vector<std::string> row{s.variant, std::to_string(s.seed)};
    for (const char* key :
         {"team.red.frags", "team.blue.frags", "team.frags-per-minute", "team.speed-avg"}) {
      double v = 0;
      row.push_back(value(s, key, v) ? to_fixed(v, 2) : "-");
    }
    each.rows.push_back(std::move(row));
  }
  lab.table(each);
}

// One match, or a set when `--seeds` or `--variants` asks for one.
void run_entry(gl::Lab& lab) {
  if (!lab.arg("seeds").empty() || !lab.arg("variants").empty())
    run_set(lab);
  else
    run(lab);
}

const gl::LabSpec kSpec{
    "se-team",
    gl::LabKind::scenario,
    "game:shooters_edge",
    "games/shooters_edge/bots/team.hpp (the roles, the key areas, the quad's timer) through "
    "net/classic_team.hpp: fighters on clients of our server, teamplay on the baseline mod",
    "bots",
    "Does a team game of bots play like a team game: does each team hold its key area, take the "
    "quad on its timer, leave items to the teammate who needs them and hold its fire for its "
    "own, and does the recording play back?",
    {
        {"minutes", "10", "the match's length"},
        {"seed", "1", "the mod's and the fighters' seed"},
        {"seeds", "",
         "a set: a match a seed (1-6, 1,3,5), in parallel, their means and the A/B table; empty: "
         "one match on --seed"},
        {"variants", "",
         "a set's variants, name:flags apart by ';' (\"on:--sides=1;off:--features=-hop "
         "--sides=1\"): every seed played by each; {seed} and {variant} in a flag's value are "
         "replaced per run"},
        {"jobs", "0", "a set's matches at once (0: one a hardware thread)"},
        {"basedir", "", "a folder holding a game folder: play one of its maps"},
        {"game", ge::classicenhance::kProfiles[0].base_dir.data(),
         "the game folder under --basedir"},
        {"map", "", "with --basedir: the map (e1m2 when empty)"},
        {"save", "", "also write the recording to this path (ending in z, .mvdz: packed)"},
        {"locs", "",
         "the map's place names (a .loc file); else the game folder's locs/<map>.loc, else the "
         "items'"},
        {"tempers", "",
         "each player's temperament, red's then blue's, comma separated: aggressive, balanced or "
         "coward (a, b, c); missing: balanced"},
        {"traits", "",
         "each player's traits over his preset, red's then blue's, comma separated, each "
         "name=value joined by / (aim_rl=9/timing=3); empty: the preset"},
        {"features", "",
         "every bot's features over its skill's preset (shooter/features.hpp): +name, -name, all, "
         "none (plain), comma separated (-swimaim,+shortgoals); empty: the preset"},
        {"skills", "",
         "every player's skill (one word: expert, as most people are today) or one a player, red's "
         "then blue's (empty: each side expert, average, average, novice)"},
        {"features-red", "", "red's features over --features (the A/B by team: one side without)"},
        {"features-blue", "", "blue's features over --features"},
        {"trace", "",
         "a TSV of every bot's state each --trace-every commands (the bot plan's scripts)"},
        {"why", "",
         "write the bots' decision log here (labs/why_log.hpp; read it with the se-why lab)"},
        {"trace-every", "4", "--trace's row every this many commands of a bot (77 a second)"},
        {"sides", "0",
         "1: each side's fight apart (damage, accuracy, sightings, what it did not see), as when "
         "the sides' features differ"},
        {"items", "key",
         "the items noted with the nearest a player came: key (the held armour, the quad) or all"},
        {"client-fed", "0",
         "1: every fighter reads only what its own client was sent (net/classic_mirror.hpp), as "
         "it must on a server it cannot read"},
        {"latency", "0",
         "ms one way between every client and the server, jitter a tenth of it (50: a round trip "
         "of about 100 ms, a server across an ocean); 0: a LAN"},
        {"lead", "1", "0: the fighters do not lead their shots on the round trip (as on a LAN)"},
        {"antilag", "0",
         "1: the server rewinds hitscan to what the shooter saw, as KTX servers do"},
        {"drill", "",
         "a weapon drill: every player holds only this weapon, its ammunition topped up "
         "(scattergun, double, spiker, heavy, grenades, rockets, arc); empty: a normal match"},
    },
    "--minutes=0.5",
    run_entry,
};
GE_LAB(kSpec);

}  // namespace
