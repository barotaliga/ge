// The access reckoning: each run's exits, each top's runs, the reach and the way home.
//
// ge:summary The access graph walked: exits, tops, reach from a start, the way home's cost
// ge:keywords access-build reckon-access exits tops reach home-of nearest-on
// ge:skill mountain
// ge:test tests/mountain_resort_build_test.cpp
#include "mountain/mapgen/access_build.hpp"

#include <algorithm>
#include <limits>

#include "core/math.hpp"
#include "mountain/mapgen/resort_rules.hpp"

namespace ge::mountain::mapgen {

using ge::core::hypot;

namespace {

constexpr double kInf = std::numeric_limits<double>::infinity();

// A way off a run at or below arc `at`: to a bottom station, onto a run, or into the hub.
struct Exit {
  double at;
  int lift = -1;
  int run = -1;
  double s = 0;
  bool hub = false;
};

struct OnRun {
  int run;
  double s;
};

struct Top {
  std::vector<OnRun> on;
  std::vector<int> bottoms;
};

// The arc on a run nearest a point, and how near, m.
OnRun nearest_on(const PlanRun& run, PlanPoint p, double& d) {
  double s = 0;
  d = kInf;
  for (const TrackPoint& q : *run.points) {
    const double dd = hypot(q.x - p.x, q.z - p.z);
    if (dd < d) {
      d = dd;
      s = q.s;
    }
  }
  return {0, s};
}

}  // namespace

PlanAccess reckon_access(std::span<const PlanRun> runs, std::span<const PlanLift> lifts,
                         int slack) {
  const double skate = resort_rules::access::skate;
  const size_t nr = runs.size();
  const size_t nl = lifts.size();
  const auto lift_index = [&](const std::string& id) -> int {
    // The last lift of that id, as a map built in order keeps it.
    int found = -1;
    for (size_t i = 0; i < nl; i++) {
      if (lifts[i].id == id) found = static_cast<int>(i);
    }
    return found;
  };
  std::vector<std::vector<Exit>> exits(nr);
  for (size_t r = 0; r < nr; r++) {
    const PlanRun& run = runs[r];
    std::vector<Exit>& out = exits[r];
    for (size_t li = 0; li < nl; li++) {
      double last = -1;
      for (const TrackPoint& p : *run.points) {
        if (hypot(p.x - lifts[li].bottom.x, p.z - lifts[li].bottom.z) < skate) last = p.s;
      }
      if (last >= 0) out.push_back({last, static_cast<int>(li)});
    }
    if (run.into) {
      out.push_back({run.length, -1, run.into->run, run.into->s});
    } else if (run.to) {
      const int li = lift_index(*run.to);
      if (li >= 0) out.push_back({run.length, li});
    } else if (run.floor) {
      out.push_back({run.length, -1, -1, 0, true});
    }
  }
  for (size_t i = 0; i < nr; i++) {
    const PlanRun& lane = runs[i];
    if (lane.branch && size_t(lane.branch->run) < nr) {
      exits[size_t(lane.branch->run)].push_back({lane.branch->s, -1, static_cast<int>(i), 0});
    }
  }
  std::vector<Top> tops(nl);
  for (size_t l = 0; l < nl; l++) {
    for (size_t i = 0; i < nr; i++) {
      const PlanRun& r = runs[i];
      if (r.from == lifts[l].id && !r.branch) {
        tops[l].on.push_back({static_cast<int>(i), 0});
      } else {
        double d = 0;
        const OnRun n = nearest_on(r, lifts[l].top, d);
        if (d < skate) tops[l].on.push_back({static_cast<int>(i), n.s});
      }
    }
    for (size_t b = 0; b < nl; b++) {
      if (hypot(lifts[b].bottom.x - lifts[l].top.x, lifts[b].bottom.z - lifts[l].top.z) < skate) {
        tops[l].bottoms.push_back(static_cast<int>(b));
      }
    }
  }
  std::vector<int> hub;
  for (size_t i = 0; i < nl; i++) {
    if (lifts[i].floor) hub.push_back(static_cast<int>(i));
  }

  // The lifts a skier rides from `starts`, skiing only runs `may` lets him; the arc he first
  // comes onto each run at; and whether a lift's top put him back on run `home` near its start.
  struct Reach {
    std::vector<bool> ridden;
    std::vector<double> best;
    bool back;
  };
  const auto reach = [&](std::vector<Exit> queue, const auto& may, int home) {
    Reach out{std::vector<bool>(nl), std::vector<double>(nr, kInf), false};
    bool hubbed = false;
    while (!queue.empty()) {
      const Exit e = queue.back();
      queue.pop_back();
      if (e.hub) {
        if (hubbed) continue;
        hubbed = true;
        for (const int li : hub) queue.push_back({0, li});
      } else if (e.lift >= 0) {
        if (out.ridden[size_t(e.lift)]) continue;
        out.ridden[size_t(e.lift)] = true;
        const Top& top = tops[size_t(e.lift)];
        for (const OnRun& o : top.on) {
          if (o.run == home && o.s <= skate) out.back = true;
          if (may(o.run)) queue.push_back({0, -1, o.run, o.s});
        }
        for (const int b : top.bottoms) queue.push_back({0, b});
      } else if (e.run >= 0) {
        const double s = e.s;
        if (!may(e.run) || s >= out.best[size_t(e.run)]) continue;
        out.best[size_t(e.run)] = s;
        for (const Exit& x : exits[size_t(e.run)]) {
          if (x.at >= s - 1e-6) queue.push_back(x);
        }
      }
    }
    return out;
  };

  // How far a skier off piste `i` skis on the runs below it, no harder than it and its own line
  // free, all the way back to its start, m; infinity where nothing brings him back.
  const auto home_of = [&](size_t i) {
    const auto may = [&](int r) { return runs[size_t(r)].rank <= runs[i].rank; };
    struct Open {
      Exit e;
      double c;
    };
    std::vector<Open> open{{{0, -1, static_cast<int>(i), 0}, 0}};
    struct Seen {
      double s, c;
    };
    std::vector<std::vector<Seen>> seen(nr);
    std::vector<bool> rode(nl);
    bool hubbed = false;
    while (!open.empty()) {
      std::stable_sort(open.begin(), open.end(),
                       [](const Open& x, const Open& y) { return y.c - x.c < 0; });
      const Open cur = open.back();
      open.pop_back();
      const Exit& e = cur.e;
      const double c = cur.c;
      if (e.hub) {
        if (hubbed) continue;
        hubbed = true;
        for (const int li : hub) open.push_back({{0, li}, c});
      } else if (e.lift >= 0) {
        if (rode[size_t(e.lift)]) continue;
        rode[size_t(e.lift)] = true;
        const Top& top = tops[size_t(e.lift)];
        for (const OnRun& o : top.on) {
          if (o.run == static_cast<int>(i) && o.s <= skate) return c;
          if (may(o.run)) open.push_back({{0, -1, o.run, o.s}, c});
        }
        for (const int b : top.bottoms) open.push_back({{0, b}, c});
      } else if (e.run >= 0 && may(e.run)) {
        const double s = e.s;
        std::vector<Seen>& sv = seen[size_t(e.run)];
        if (std::any_of(sv.begin(), sv.end(), [&](const Seen& v) { return v.s <= s && v.c <= c; }))
          continue;
        sv.push_back({s, c});
        for (const Exit& x : exits[size_t(e.run)]) {
          if (x.at >= s - 1e-6) {
            open.push_back({x, c + (e.run == static_cast<int>(i) ? 0 : x.at - s)});
          }
        }
      }
    }
    return kInf;
  };

  std::vector<Exit> every_top;
  for (size_t li = 0; li < nl; li++) every_top.push_back({0, static_cast<int>(li)});
  PlanAccess out;
  for (size_t i = 0; i < nr; i++) {
    const PlanRun& run = runs[i];
    if (run.kind != RunKind::piste) {
      out.ok.push_back(true);
      out.why.emplace_back();
      out.home.push_back(0);
      continue;
    }
    const auto may = [&](int r) { return runs[size_t(r)].rank <= run.rank; };
    // THE WAY IN: from some lift's top, onto it within a skate of its start.
    if (reach(every_top, may, -1).best[i] > skate) {
      out.ok.push_back(false);
      out.why.emplace_back("the way in: no lift's top reaches its start without a harder run");
      out.home.push_back(kInf);
      continue;
    }
    // THE WAY OUT AND BACK: down it, on from its foot, and up to it again.
    const auto back = [&](int r) { return runs[size_t(r)].rank <= run.rank + slack; };
    const std::vector<Exit> self{{0, -1, static_cast<int>(i), 0}};
    if (reach(self, back, static_cast<int>(i)).back) {
      out.ok.push_back(true);
      out.why.emplace_back();
      // The way home is measured off a lift whose bottom stands up the mountain.
      const int own = lift_index(run.from);
      out.home.push_back(own < 0 || lifts[size_t(own)].floor ? 0 : home_of(i));
      continue;
    }
    const bool any = reach(self, [](int) { return true; }, static_cast<int>(i)).back;
    out.ok.push_back(false);
    out.home.push_back(kInf);
    out.why.emplace_back(any ? "the way out and back: only down a harder run"
                             : "the way out and back: no lift or lane brings it back");
  }
  const Reach from = reach({{0, -1, -1, 0, true}}, [](int) { return true; }, -1);
  for (size_t li = 0; li < nl; li++) {
    if (!from.ridden[li]) out.orphans.push_back(lifts[li].id);
  }
  return out;
}

}  // namespace ge::mountain::mapgen
