// Autotrack: the hottest player's cuts over a multiview recording (demo_autotrack.hpp).
//
// ge:summary Autotrack: the hottest player's cuts over a multiview recording, by kit and streaks
// ge:keywords autotrack autotrack_plan autotrack_kit hottest-player streak cuts
// ge:skill shooter replay
#include "shooters_edge/client/demo_autotrack.hpp"

#include <algorithm>
#include <array>

#include "shooters_edge/client/timedemo.hpp"

namespace shooters_edge::client {

namespace {

constexpr int kSlots = 32;
// The classic item bits.
constexpr int32_t kScattergun = 1, kDouble = 2, kSpiker = 4, kHeavy = 8, kGrenades = 16,
                  kLauncher = 32, kArc = 64, kBlade = 4096;
constexpr int32_t kArmourGreen = 8192, kArmourYellow = 16384, kArmourRed = 32768;
constexpr int32_t kRing = 524288, kShield = 1048576, kQuad = 4194304;

struct Sample {
  bool playing = false, alive = false;
  int32_t items = 0;
  double hot = 0;
};

}  // namespace

double autotrack_kit(int32_t items, int32_t armour, int32_t frags) {
  const double aw = (items & kArmourRed)      ? 3
                    : (items & kArmourYellow) ? 2
                    : (items & kArmourGreen)  ? 1
                                              : 0;
  const double ww = (items & kLauncher)     ? 10
                    : (items & kArc)        ? 10
                    : (items & kGrenades)   ? 6
                    : (items & kHeavy)      ? 4
                    : (items & kSpiker)     ? 2
                    : (items & kDouble)     ? 4
                    : (items & kScattergun) ? 2
                    : (items & kBlade)      ? 1
                                            : 0;
  double pw = 0;
  if (items & kRing) pw += 500;
  if (items & kQuad) pw += 900;
  if (items & kShield) pw += 1000;
  return aw * armour + 50 * ww + pw + frags;
}

bool autotrack_plan(const Demo& opened, const DemoScan& scan, const AutotrackSettings& set,
                    AutotrackPlan& out, std::string& error) {
  out = AutotrackPlan{};
  if (opened.state.format != DemoFormat::multiview) {
    error =
        "autotrack needs a multiview recording (an .mvd) with every player's stats in it; "
        "pick one player with --track instead";
    return false;
  }
  // Each slot's enemy kills and deaths, in time order, off the scan.
  std::array<std::vector<double>, kSlots> kills, deaths;
  for (const DemoKill& k : scan.kills) {
    if (k.victim >= 0 && k.victim < kSlots) deaths[static_cast<size_t>(k.victim)].push_back(k.t);
    if (k.killer < 0 || k.killer >= kSlots || k.killer == k.victim) continue;
    kills[static_cast<size_t>(k.killer)].push_back(k.t);
  }
  Timedemo t;
  t.demo = opened;
  const Demo& d = t.demo;
  const DemoState& s = d.state;
  const auto teammates = [&](int a, int b) {
    return !s.teams[static_cast<size_t>(a)].empty() &&
           s.teams[static_cast<size_t>(a)] == s.teams[static_cast<size_t>(b)];
  };
  const auto count_in = [](const std::vector<double>& v, double from, double to) {
    int n = 0;
    for (double x : v) n += x > from && x <= to ? 1 : 0;
    return n;
  };
  std::array<Sample, kSlots> now{}, was{};
  int tracked = -1;
  double on_since = 0, last_cut = -1e9, dead_since = -1;
  std::string team;  // the locked team
  double next = 0;
  for (;;) {
    const TimedemoStep st = timedemo_step(t, error);
    if (st == TimedemoStep::error) return false;
    if (st == TimedemoStep::end) break;
    const double at = t.timeline.t;
    if (at < next) continue;
    next = at + set.every_s;
    out.samples++;
    was = now;
    for (int i = 0; i < kSlots; i++) {
      Sample& p = now[static_cast<size_t>(i)];
      p = Sample{};
      if (static_cast<size_t>(i) >= d.player_stats.size()) continue;
      const auto& st_i = d.player_stats[static_cast<size_t>(i)];
      const bool seen =
          static_cast<size_t>(i) < s.players.size() && s.players[static_cast<size_t>(i)].present;
      p.playing = seen;
      p.alive = seen && st_i[kStatHealth] > 0 && !s.players[static_cast<size_t>(i)].dead;
      p.items = st_i[kStatItems];
      // Kills that count: enemies only (the scan's killer; a teammate's death is no streak).
      double died = -1e9;
      for (double x : deaths[static_cast<size_t>(i)])
        if (x <= at) died = x;
      int streak = 0, recent = 0;
      for (const DemoKill& k : scan.kills) {
        if (k.killer != i || k.victim < 0 || teammates(i, k.victim)) continue;
        if (k.t > died && k.t <= at) streak++;
        if (k.t > at - set.window_s && k.t <= at + set.lookahead_s) recent++;
      }
      p.hot = autotrack_kit(p.items, st_i[kStatArmour], s.frags[static_cast<size_t>(i)]) +
              set.streak_weight * streak + set.recent_weight * recent;
    }
    // The hottest alive, `skip` aside, within the locked team.
    const auto hottest = [&](int skip) {
      int best = -1;
      for (int i = 0; i < kSlots; i++) {
        const Sample& p = now[static_cast<size_t>(i)];
        if (i == skip || !p.alive) continue;
        if (set.lockteam && !team.empty() && s.teams[static_cast<size_t>(i)] != team) continue;
        if (best < 0 || p.hot > now[static_cast<size_t>(best)].hot) best = i;
      }
      return best;
    };
    const auto cut = [&](int slot, const char* why) {
      out.cuts.push_back({at, slot, why, now[static_cast<size_t>(slot)].hot});
      tracked = slot;
      on_since = at;
      last_cut = at;
      dead_since = -1;
      if (team.empty()) team = s.teams[static_cast<size_t>(slot)];
    };
    if (tracked < 0) {
      const int b = hottest(-1);
      if (b >= 0) cut(b, "start");
      continue;
    }
    if (at - last_cut < set.gap_s) continue;
    const Sample& me = now[static_cast<size_t>(tracked)];
    if (!me.alive) {
      if (dead_since < 0) dead_since = at;
      if (at - dead_since >= set.deathwait_s || !me.playing) {
        const int b = hottest(tracked);
        if (b >= 0) cut(b, "death");
      }
      continue;
    }
    dead_since = -1;
    const int c = hottest(tracked);
    if (c < 0) continue;
    const Sample& ch = now[static_cast<size_t>(c)];
    const double need = me.hot + std::max(set.margin, set.margin_share * me.hot);
    if (ch.hot <= need) continue;
    const int32_t took = ch.items & ~was[static_cast<size_t>(c)].items;
    const bool quad = (took & kQuad) != 0, shield = (took & kShield) != 0;
    const bool burst = count_in(kills[static_cast<size_t>(c)], at - set.every_s, at) > 0 &&
                       count_in(kills[static_cast<size_t>(c)], at - set.window_s, at) >= 2;
    if (quad || shield || burst)
      cut(c, quad ? "quad" : shield ? "shield" : "kills");
    else if (at - on_since >= set.dwell_s)
      cut(c, "hotter");
  }
  out.length_s = t.timeline.t;
  return true;
}

int autotrack_at(const AutotrackPlan& plan, double t) {
  int slot = -1;
  for (const AutotrackCut& c : plan.cuts) {
    if (c.t > t) break;
    slot = c.slot;
  }
  return slot;
}

}  // namespace shooters_edge::client
