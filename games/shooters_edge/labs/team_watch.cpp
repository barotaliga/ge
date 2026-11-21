// THE TEAM GAME WATCHED (team_watch.hpp): the notes taken on every command and the metrics, the
// tables and the flights by mode the `se-team` lab reports from them.
//
// ge:summary The se-team lab's watch noted and reported: hops, still, the view, facing, bumps
// ge:keywords lab se-team watch watch_hop watch_still watch_view bump_by median flights_by_mode
// ge:skill bots labs
// ge:test tests/shooters_edge_team_test.cpp
#include "shooters_edge/labs/team_watch.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string_view>

#include "core/num_text.hpp"
#include "shooter/trace.hpp"
#include "shooterbots/hops.hpp"
#include "shooters_edge/bots/knowledge.hpp"
#include "shooters_edge/labs/move_study.hpp"

namespace shooters_edge::labs::se_team {

namespace {

namespace gl = ge::lab;
namespace sh = ge::shooter;
namespace sb = ge::shooterbots;
namespace cn = shooters_edge::net;
using ge::core::num::to_fixed;

}  // namespace

// What took a bumped player's speed: the box swept 16 units on along its velocity before the bump
// meets the world (`wall`), another player (`player`) or a brush entity (`mover`: a door, a plat);
// a wall the same sweep clears raised one or two stair steps (18, 36 units) is a stair's riser
// (`step`: a run jumped into stairs going up, where it sticks); nothing there is `open` (a
// landing, water, a push). A bump in a command that took health is put
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
  if (tr.ent <= 0) {
    if (tr.plane.normal.z >= 0.7f) return "open";
    for (const float up : {18.f, 36.f}) {
      const sh::Vec3f o{origin.x, origin.y, origin.z + up};
      const sh::HullTrace high = sh::trace_move(sv, o, {-16, -16, -24}, {16, 16, 32},
                                                {to.x, to.y, to.z + up}, sh::kTraceNormal, ent);
      if (!high.startsolid && high.fraction > tr.fraction + 0.25f) return "step";
    }
    return "wall";
  }
  if (tr.ent <= static_cast<int>(sv.clients.size())) return "player";
  return "mover";
}

void watch_hop(Watch& w, int k, sb::AimSkill skill, bool alive, bool hop, const char* because,
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

void watch_still(Watch& w, const shooters_edge::bots::Fighter& f, const sb::Pilot& p, float speed,
                 float x, float y, float z) {
  w.commands++;
  const std::string_view why = p.because != nullptr ? p.because : "";
  int kind = -1;
  if (why == "a mover in the way")
    kind = 0;
  else if (why == "no route")
    kind = 1;
  else if (p.mode == sb::PilotMode::walk && speed < 20)
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

void watch_view(Watch& w, int k, double t, float yaw, const char* by, float vx, float vy) {
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
  // se-people's rule: a 40 Hz read (kHandEvery), the rate over the read's step.
  Watch::View& u = w.view40[static_cast<size_t>(k)];
  const char* by40 = by != nullptr ? by : "-";
  if (u.t < 0 || t - u.t > 0.1) {
    u = {t, yaw, 0, by40, by40};
    return;
  }
  if (t - u.t < 0.025) return;
  float d40 = yaw - u.yaw;
  while (d40 > 180) d40 -= 360;
  while (d40 < -180) d40 += 360;
  const auto rate40 = static_cast<float>(-d40 / (t - u.t));
  auto& wr40 = w.writers40[by40];
  wr40.first += t - u.t;
  if (std::fabs(rate40) > 60 && std::fabs(u.rate) > 60 && rate40 * u.rate < 0) {
    wr40.second++;
    const char* from = u.by2 != nullptr ? u.by2 : by40;
    if (std::strcmp(from, by40) != 0 || std::strcmp(u.by != nullptr ? u.by : by40, by40) != 0)
      w.seams40[{from, by40}]++;
  }
  // The hop's swap (swap_read.hpp's rule): the ring's middle read the crossing.
  Watch::SwapRing& sr = w.swap40[static_cast<size_t>(k)];
  if (std::sqrt(vx * vx + vy * vy) <= 340) {
    sr.n = 0;
  } else {
    w.hop40 += t - u.t;
    float lean = yaw - static_cast<float>(std::atan2(vy, vx) * 180 / 3.14159265358979);
    while (lean > 180) lean -= 360;
    while (lean < -180) lean += 360;
    for (size_t i = 0; i + 1 < sr.lean.size(); i++) {
      sr.lean[i] = sr.lean[i + 1];
      sr.by[i] = sr.by[i + 1];
    }
    sr.lean[6] = lean;
    sr.by[6] = by;
    sr.n = std::min(sr.n + 1, 7);
    if (sr.n == 7 && sr.lean[2] * sr.lean[3] < 0 && std::fabs(sr.lean[0]) > 5 &&
        std::fabs(sr.lean[6]) > 5 && sr.lean[0] * sr.lean[6] < 0)
      w.swaps40[sr.by[3] != nullptr ? sr.by[3] : "-"]++;
  }
  u = {t, yaw, rate40, by40, u.by};
}

void facing_metrics(gl::Lab& lab, const Watch& w) {
  if (w.run_frames > 0)
    lab.metric("team.turnview.on-run",
               static_cast<double>(w.run_on) / static_cast<double>(w.run_frames), "share");
  if (!w.turn_lead.empty()) {
    std::vector<float> v = w.turn_lead;
    std::sort(v.begin(), v.end());
    const auto at = [&](double q) {
      return static_cast<double>(v[static_cast<size_t>(q * static_cast<double>(v.size() - 1))]);
    };
    lab.metric("team.turnview.lead-p25", at(0.25), "deg");
    lab.metric("team.turnview.lead-p50", at(0.5), "deg");
    int64_t trail = 0;
    for (const auto& [key, row] : w.turn_by) trail += row[1];
    lab.metric("team.turnview.trail40", static_cast<double>(trail) / static_cast<double>(v.size()),
               "share");
    for (const auto& [key, row] : w.turn_by)
      lab.metric("team.turnview." + key + ".trail40",
                 static_cast<double>(row[1]) / static_cast<double>(v.size()), "share");
  }
  int64_t seen = 0, faced = 0;
  for (const auto& [key, row] : w.facing) {
    seen += row[0];
    faced += row[1];
  }
  if (seen == 0) return;
  lab.metric("team.facing.faced", static_cast<double>(faced) / static_cast<double>(seen), "share");
  // Each mode and view writer: its share of the time an enemy is in sight, and of the time one
  // is in sight off the screen.
  int64_t app = 0, app_on = 0;
  for (const auto& [key, row] : w.appear) {
    app += row[0];
    app_on += row[1];
  }
  if (app > 0) {
    lab.metric("team.appear.faced", static_cast<double>(app_on) / static_cast<double>(app),
               "share");
    for (const auto& [key, row] : w.appear) {
      lab.metric("team.appear." + key + ".share",
                 static_cast<double>(row[0]) / static_cast<double>(app), "share");
      lab.metric("team.appear." + key + ".faced",
                 row[0] > 0 ? static_cast<double>(row[1]) / static_cast<double>(row[0]) : 0,
                 "share");
    }
    for (const auto& [key, row] : w.appear_way) {
      lab.metric("team.appear." + key + ".share",
                 static_cast<double>(row[0]) / static_cast<double>(app), "share");
      lab.metric("team.appear." + key + ".faced",
                 row[0] > 0 ? static_cast<double>(row[1]) / static_cast<double>(row[0]) : 0,
                 "share");
    }
  }
  int64_t off = 0;
  for (const auto& [key, n] : w.facing_off) off += n;
  for (const auto& [key, n] : w.facing_off)
    lab.metric("team.facing-off." + key,
               static_cast<double>(n) / static_cast<double>(std::max<int64_t>(off, 1)), "share");
  for (const auto& [key, row] : w.facing) {
    lab.metric("team.facing." + key + ".seen",
               static_cast<double>(row[0]) / static_cast<double>(seen), "share");
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
  double t40 = 0;
  int64_t r40 = 0;
  for (const auto& [by, tr] : w.writers40) {
    t40 += tr.first;
    r40 += tr.second;
  }
  for (const auto& [by, tr] : w.writers40) {
    lab.metric("team.view40." + by + ".share", t40 > 0 ? tr.first / t40 : 0, "share");
    lab.metric("team.view40." + by + ".reversals-per-s",
               tr.first > 0 ? static_cast<double>(tr.second) / tr.first : 0, "per-s");
  }
  lab.metric("team.view40.reversals-per-s", t40 > 0 ? static_cast<double>(r40) / t40 : 0, "per-s");
  {
    // The reversals at a seam (the writer changed across the reads): a hand-over's, by pair.
    int64_t seam = 0;
    std::map<std::string, int64_t> into;
    std::vector<std::pair<int64_t, std::string>> pairs;
    for (const auto& [ft, c] : w.seams40) {
      seam += c;
      into[ft.second] += c;
      pairs.push_back({c, ft.first + ">" + ft.second});
    }
    lab.metric("team.view40.seam-share",
               r40 > 0 ? static_cast<double>(seam) / static_cast<double>(r40) : 0, "share");
    for (const auto& [wb, wt] : w.writers40)
      lab.metric("team.view40." + wb + ".seam-share",
                 wt.second > 0 ? static_cast<double>(into[wb]) / static_cast<double>(wt.second) : 0,
                 "share");
    std::sort(pairs.begin(), pairs.end(), [](const auto& pa, const auto& pb) {
      return pa.first != pb.first ? pa.first > pb.first : pa.second < pb.second;
    });
    gl::LabTable seams{
        "the view's reversals at a seam (the writer changed across the 25 ms reads), "
        "by the writer before > after",
        {"seam", "reversals", "% of all", "a second of live time"},
        {}};
    for (size_t i = 0; i < pairs.size() && i < 12; i++)
      seams.rows.push_back(
          {pairs[i].second, std::to_string(pairs[i].first),
           to_fixed(
               r40 > 0 ? 100.0 * static_cast<double>(pairs[i].first) / static_cast<double>(r40) : 0,
               1),
           to_fixed(t40 > 0 ? static_cast<double>(pairs[i].first) / t40 : 0, 3)});
    lab.table(seams);
  }
  int64_t swaps = 0;
  for (const auto& [who, c] : w.swaps40) swaps += c;
  lab.metric("team.swap.per-s", w.hop40 > 0 ? static_cast<double>(swaps) / w.hop40 : 0, "per-s");
  for (const auto& [who, c] : w.swaps40)
    lab.metric("team.swap." + who + ".share",
               swaps > 0 ? static_cast<double>(c) / static_cast<double>(swaps) : 0, "share");
}

void hop_metrics(gl::Lab& lab, const Watch& w) {
  for (const sb::AimSkill k :
       {sb::AimSkill::expert, sb::AimSkill::average, sb::AimSkill::novice, sb::AimSkill::perfect}) {
    const HopSkill& s = w.skill[static_cast<size_t>(k)];
    if (s.alive == 0) continue;
    const std::string key = std::string("team.hop.") + sb::aim_skill_name(k) + ".";
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
// own recording read as se-moves reads people's (move_study.hpp, shooterbots/hops.hpp: each
// player's track off the server's frames, its hops traced on the map's hulls), each flight of 0.3-1
// s taking off at 380-460 u/s put down to its bot's pilot mode at the takeoff: how many, the air's
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
  const sb::HopSettings hs;
  for (size_t slot = 0; slot < dt.tracks.size(); slot++) {
    size_t k = r.players.size();
    for (size_t j = 0; j < r.players.size(); j++)
      if (r.players[j].name == dt.names[slot]) k = j;
    if (k >= r.players.size() || k >= w.modes.size() || dt.tracks[slot].size() < 3) continue;
    sb::HopReading hr;
    sb::read_hops(dt.tracks[slot], *m.world, hs, hr);
    const auto& ml = w.modes[k];
    for (const sb::Hop& h : hr.hops) {
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
const char* link_of(const sb::Pilot& p) {
  if (p.g == nullptr || p.k + 1 >= p.route.size()) return "-";
  const sb::NavNode& a = p.g->nodes[static_cast<size_t>(p.route[p.k])];
  for (int32_t q = 0; q < a.links; q++) {
    const sb::NavLink& l = p.g->links[static_cast<size_t>(a.first_link) + static_cast<size_t>(q)];
    if (l.to == p.route[p.k + 1]) return sb::nav_kind_name(l.kind);
  }
  return "-";
}

// The link walked: '!' along a lip (`brink`), '?' a side step off into the harm (`brink_side`).
char link_brink(const sb::Pilot& p) {
  if (p.g == nullptr || p.k + 1 >= p.route.size()) return 0;
  const sb::NavNode& a = p.g->nodes[static_cast<size_t>(p.route[p.k])];
  for (int32_t q = 0; q < a.links; q++) {
    const sb::NavLink& l = p.g->links[static_cast<size_t>(a.first_link) + static_cast<size_t>(q)];
    if (l.to == p.route[p.k + 1]) return l.brink ? '!' : l.brink_side ? '?' : 0;
  }
  return 0;
}

// The kind of the link after the current one (`end`: the next node is the route's last).
const char* link2_of(const sb::Pilot& p) {
  if (p.g == nullptr || p.k + 1 >= p.route.size()) return "-";
  if (p.k + 2 >= p.route.size()) return "end";
  const sb::NavNode& b = p.g->nodes[static_cast<size_t>(p.route[p.k + 1])];
  for (int32_t q = 0; q < b.links; q++) {
    const sb::NavLink& l = p.g->links[static_cast<size_t>(b.first_link) + static_cast<size_t>(q)];
    if (l.to == p.route[p.k + 2]) return sb::nav_kind_name(l.kind);
  }
  return "-";
}

}  // namespace shooters_edge::labs::se_team
