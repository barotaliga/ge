// The lanes between the runs: the sectors, the branch points, the link lanes, access rounds.
//
// ge:summary Branch and link lanes: sectors, branch points, the access rounds, the way home
// ge:keywords links lay-links lay-access branch link box-of lane-spec room sectors
// ge:skill mountain
// ge:test tests/mountain_resort_build_test.cpp
#include "mountain/mapgen/links.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <set>

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "core/num.hpp"
#include "mountain/mapgen/resort_rules.hpp"

namespace ge::mountain::mapgen {

namespace num = ge::core::num;
namespace libm = ge::core::libm;
namespace RR = resort_rules;
using ge::core::hypot;

namespace {

constexpr double kInf = std::numeric_limits<double>::infinity();

// How far down a piste a branch lane may leave it, and how far up from its end, m; the arcs it
// is tried at between, m apart.
constexpr double kBranchFrom = 200;
constexpr double kBranchTo = 300;
constexpr double kBranchStep = 60;
// The least plan distance from a branch point to the join, m.
constexpr double kBranchLeast = 100;
// How far inside its piste's edge a branch lane starts, m.
constexpr double kBranchIn = 12;
// The branch points tried for each pair of sectors, best first.
constexpr size_t kBranchTries = 5;
// What a steep face costs a branch point, as metres further per unit of fall past the free.
constexpr double kSteepCost = 1500;
constexpr double kSteepFree = 0.25;
// How much lower than a branch point the other sector's piste must lie, as a lane's pitch.
constexpr double kBranchFall = 0.06;
// The most of an area's kilometres its lanes run to before no more are laid between its runs.
constexpr double kLaneShare = 0.13;
// Where on a piste a link lane to a bottom station leaves it: how far up from its end, m; and
// for a link lane home, up from where it begins closing on the run it merges into.
constexpr Band kLinkFrom{250, 900};
constexpr Band kLinkHome{RR::network::junction, 700};
// How many times R29's verdict is read again as lanes and lifts are laid to it, and how many
// bottom stations a piste's link lane is tried toward.
constexpr int kAccessRounds = 16;
constexpr size_t kAccessGoals = 2;
// How far round the start and what it may join a lane's search reaches, m.
constexpr double kBoxPad = 240;

// How hard a walked run is to ski, for R29, by the colour it was built to: a lane is a green.
int rank(const WalkedRun& w) {
  if (w.spec.kind == RunKind::road) return 0;
  return static_cast<int>(w.spec.row->id.value_or(PisteGrade::green));
}

// Whether the area has room for another lane: its lanes under `kLaneShare` of its kilometres.
bool room(const LinkBuilder& b) {
  double lanes = 0;
  double all = 0;
  for (const WalkedRun& w : *b.walked) {
    all += w.track.length;
    if (w.spec.kind == RunKind::road) lanes += w.track.length;
  }
  return lanes < kLaneShare * all;
}

// A branch point on a piste: the run, the arc, where the lane starts, and its cost.
struct Point {
  int run;
  double s, x, z, far;
};

const TrackPoint& at_arc(const WalkedRun& w, double s) {
  const auto& pts = w.track.points;
  return pts[size_t(num::round((s / w.track.length) * double(pts.size() - 1)))];
}

// The stretch of the map a lane from `p` to any of `to` is looked for in.
LaneAsk::Box box_of(const Point& p, const std::vector<PlanPoint>& to) {
  double x0 = p.x;
  double x1 = p.x;
  double z1 = p.z;
  for (const PlanPoint& q : to) {
    if (q.z <= p.z) continue;
    x0 = num::min(x0, q.x);
    x1 = num::max(x1, q.x);
    z1 = num::max(z1, q.z);
  }
  return {x0 - kBoxPad, x1 + kBoxPad, z1 + kBoxPad};
}

// A lane's spec off a branch point along its route.
RunSpec lane_spec(const WalkedRun& parent, const Point& p, const LaneRoute& lane, double side,
                  std::string id) {
  const PlanPoint ahead = lane.route[std::min<size_t>(5, lane.route.size() - 1)];
  RunSpec spec{std::move(id),
               parent.spec.from,
               RunKind::road,
               &kRoadRow,
               p.x,
               p.z,
               libm::atan2(ahead.x - p.x, num::max(1.0, ahead.z - p.z)),
               {lane.x, lane.z},
               0.05,
               side};
  spec.route = lane.route;
  return spec;
}

void sort_points(std::vector<Point>& points) {
  std::stable_sort(points.begin(), points.end(),
                   [](const Point& a, const Point& c) { return a.far - c.far < 0; });
}

// One branch lane off sector `from`'s pistes toward the `side` sector `to`, at the first of its
// best branch points that routes; whether one stood.
bool branch(const LinkBuilder& b, const std::vector<int>& from, const std::string& to, double side,
            const std::function<std::string()>& next_id) {
  const std::deque<WalkedRun>& walked = *b.walked;
  std::vector<int> targets;
  for (size_t i = 0; i < walked.size(); i++) {
    if (walked[i].spec.kind == RunKind::piste && walked[i].spec.from == to) {
      targets.push_back(static_cast<int>(i));
    }
  }
  if (from.empty() || targets.empty()) return false;
  std::vector<Point> points;
  for (const int r : from) {
    const WalkedRun& w = walked[size_t(r)];
    // Never across a junction: clear of where it merges, and of where any other run merges into
    // it or leaves it.
    std::vector<double> busy;
    for (size_t i = 0; i < walked.size(); i++) {
      const WalkedRun& o = walked[i];
      if (o.into && o.into->run == r) busy.push_back(o.into->s);
      if (o.spec.branch && o.spec.branch->run == r) busy.push_back(o.spec.branch->s);
      if (static_cast<int>(i) == r && o.into) busy.push_back(o.merge_start);
    }
    const double last = w.into ? w.merge_start : w.track.length - kBranchTo;
    // Below every lane that joins it: one leaving above would cross it.
    double first = kBranchFrom;
    for (const WalkedRun& o : walked) {
      if (o.spec.kind == RunKind::road && o.into && o.into->run == r) {
        first = num::max(first, o.into->s + RR::network::junction);
      }
    }
    double s = first;
    while (s < last) {
      const double at = s;
      s += kBranchStep;
      if (std::any_of(busy.begin(), busy.end(),
                      [&](double j) { return std::abs(j - at) < RR::network::junction; })) {
        continue;
      }
      const TrackPoint& p = at_arc(w, at);
      const double off = num::max(0.0, p.width / 2 - kBranchIn) * side;
      const double x = p.x + libm::cos(p.heading) * off;
      const double z = p.z - libm::sin(p.heading) * off;
      // The nearest point of the other sector it could fall to at a lane's pitch.
      double far = kInf;
      for (const int t : targets) {
        const auto& pts = walked[size_t(t)].track.points;
        for (size_t k = 0; k < pts.size(); k += 5) {
          const TrackPoint& q = pts[k];
          const double d = hypot(q.x - x, q.z - z);
          if (q.z - z > 40 && num::sign(q.x - x) == side && p.y - q.y > d * kBranchFall && d < far)
            far = d;
        }
      }
      const double pitch = b.fall(x, z);
      if (far < RR::road::reach.max) {
        points.push_back({r, p.s, x, z, far + kSteepCost * num::max(0.0, pitch - kSteepFree)});
      }
    }
  }
  sort_points(points);
  for (size_t k = 0; k < std::min(kBranchTries, points.size()); k++) {
    const Point& p = points[k];
    std::vector<PlanPoint> ends;
    for (const int t : targets) {
      for (const TrackPoint& q : walked[size_t(t)].track.points) ends.push_back({q.x, q.z});
    }
    LaneAsk ask;
    ask.least = kBranchLeast;
    ask.box = box_of(p, ends);
    const std::optional<LaneRoute> lane = b.route(
        p.x, p.z, side,
        [&](int r) { return std::find(targets.begin(), targets.end(), r) != targets.end(); }, ask);
    if (!lane || lane->run < 0) continue;
    RunSpec spec = lane_spec(walked[size_t(p.run)], p, *lane, side, next_id());
    spec.join = lane->run;
    spec.branch = RunSpec::Branch{p.run, p.s};
    const int parent = p.run;
    if (b.lay(spec, [parent](int r) { return r == parent; })) return true;
  }
  return false;
}

// One link lane to a lift's bottom station, off the lower part of the piste nearest it that a
// lane can fall from (of `from`, when given); whether one stood.
bool link(const LinkBuilder& b, const LiftPlan& lift, const std::function<std::string()>& next_id,
          const std::vector<int>* from = nullptr, Band reach = kLinkFrom) {
  const std::deque<WalkedRun>& walked = *b.walked;
  const PlanPoint goal = lift.bottom;
  std::vector<Point> points;
  for (size_t r = 0; r < walked.size(); r++) {
    const WalkedRun& w = walked[r];
    if (w.spec.kind != RunKind::piste ||
        (from && std::find(from->begin(), from->end(), static_cast<int>(r)) == from->end())) {
      continue;
    }
    const double end = w.into ? w.merge_start : w.track.length;
    double s = num::max(kBranchFrom, end - reach.max);
    while (s < end - reach.min) {
      const TrackPoint& p = at_arc(w, s);
      s += kBranchStep;
      const double sg = num::sign(goal.x - p.x);
      const double side = sg == 0 ? 1 : sg;
      const double off = num::max(0.0, p.width / 2 - kBranchIn) * side;
      const double x = p.x + libm::cos(p.heading) * off;
      const double z = p.z - libm::sin(p.heading) * off;
      const double d = hypot(goal.x - x, goal.z - z);
      if (goal.z - z < 40 || p.y - b.ground.height(goal.x, goal.z) < d * kBranchFall ||
          d > RR::road::reach.max) {
        continue;
      }
      points.push_back({static_cast<int>(r), p.s, x, z,
                        d + kSteepCost * num::max(0.0, b.fall(x, z) - kSteepFree)});
    }
  }
  sort_points(points);
  for (size_t k = 0; k < std::min(kBranchTries, points.size()); k++) {
    const Point& p = points[k];
    const double sg = num::sign(goal.x - p.x);
    const double side = sg == 0 ? 1 : sg;
    LaneAsk ask;
    ask.goal = goal;
    ask.box = box_of(p, {goal});
    const std::optional<LaneRoute> lane = b.route(p.x, p.z, side, [](int) { return false; }, ask);
    if (!lane || lane->run >= 0) continue;
    RunSpec spec = lane_spec(walked[size_t(p.run)], p, *lane, side, next_id());
    spec.to = lift.id;
    spec.branch = RunSpec::Branch{p.run, p.s};
    const int parent = p.run;
    if (b.lay(spec, [parent](int r) { return r == parent; })) return true;
  }
  return false;
}

std::vector<PlanLift> lifts_of(const LinkBuilder& b) {
  std::vector<PlanLift> out;
  for (const LiftPlan& l : *b.lifts)
    out.push_back({l.id, l.bottom, l.top, b.ground.floor(l.bottom)});
  return out;
}

}  // namespace

bool AccessLaid::is_lost(size_t i) const {
  return std::find(lost.begin(), lost.end(), i) != lost.end();
}

std::vector<PlanRun> plan_runs(const std::deque<WalkedRun>& walked) {
  std::vector<PlanRun> out;
  out.reserve(walked.size());
  for (const WalkedRun& w : walked) {
    out.push_back({w.spec.kind, rank(w), w.spec.from, &w.track.points, w.track.length, w.into,
                   w.merge_start, w.spec.branch, w.spec.to, !w.into && !w.spec.to});
  }
  return out;
}

AccessLaid lay_access(const LinkBuilder& b, const std::function<std::string()>& next_id) {
  AccessLaid laid;
  std::set<std::string> tried;
  const std::deque<WalkedRun>& walked = *b.walked;
  for (int round = 0; round < kAccessRounds; round++) {
    const std::vector<PlanRun> runs = plan_runs(walked);
    const PlanAccess verdict = reckon_access(runs, lifts_of(b), b.ground.slack);
    bool done = false;
    for (const std::string& id : verdict.orphans) {
      if (tried.count(id)) continue;
      tried.insert(id);
      const LiftPlan* lift = nullptr;
      for (const LiftPlan& l : *b.lifts) {
        if (l.id == id) {
          lift = &l;
          break;
        }
      }
      if (lift) {
        const LiftPlan copy = *lift;
        if (link(b, copy, next_id)) {
          laid.lanes++;
          done = true;
          break;
        }
      }
    }
    if (done) continue;
    std::vector<size_t> failing;
    for (size_t i = 0; i < runs.size(); i++) {
      if (!verdict.ok[i] && !laid.is_lost(i)) failing.push_back(i);
    }
    if (failing.empty()) break;
    for (const size_t i : failing) {
      if (drag_back(b.ground, runs, *b.lifts, i)) {
        laid.drags++;
        done = true;
        break;
      }
      // The runs a skier on it goes on down, no harder than it, and the foot of the last.
      const WalkedRun& p = walked[i];
      std::vector<int> chain{static_cast<int>(i)};
      const WalkedRun* w = &p;
      while (w->into && rank(walked[size_t(w->into->run)]) <= rank(p) &&
             chain.size() <= walked.size()) {
        chain.push_back(w->into->run);
        w = &walked[size_t(w->into->run)];
      }
      const WalkedRun& foot = walked[size_t(chain.back())];
      const TrackPoint& end = foot.track.points.back();
      std::vector<LiftPlan> goals = *b.lifts;
      const auto own = [&](const LiftPlan& x) { return x.id == p.spec.from ? -1e9 : 0.0; };
      std::stable_sort(goals.begin(), goals.end(), [&](const LiftPlan& l, const LiftPlan& m) {
        return own(l) + hypot(l.bottom.x - end.x, l.bottom.z - end.z) -
                   (own(m) + hypot(m.bottom.x - end.x, m.bottom.z - end.z)) <
               0;
      });
      for (size_t g = 0; g < std::min(kAccessGoals, goals.size()); g++) {
        const std::string key = std::to_string(i) + " " + goals[g].id;
        if (tried.count(key)) continue;
        tried.insert(key);
        if (link(b, goals[g], next_id, &chain)) {
          laid.lanes++;
          done = true;
          break;
        }
      }
      if (done) break;
      laid.lost.push_back(i);
    }
    if (!done) break;
  }
  // THE LONG WAY HOME: a piste off a lift whose bottom stands up the mountain that leaves its
  // skier more than `access.runout` of the runs below to ski on the way back is given a link
  // lane off its lower part to that bottom station.
  const std::vector<PlanRun> runs = plan_runs(walked);
  const PlanAccess verdict = reckon_access(runs, lifts_of(b), b.ground.slack);
  for (size_t i = 0; i < runs.size(); i++) {
    if (laid.is_lost(i) || verdict.home[i] <= RR::access::runout) continue;
    const LiftPlan* own = nullptr;
    for (const LiftPlan& l : *b.lifts) {
      if (l.id == runs[i].from) {
        own = &l;
        break;
      }
    }
    if (!own || b.ground.floor(own->bottom)) continue;
    const LiftPlan copy = *own;
    const std::vector<int> only{static_cast<int>(i)};
    if (link(b, copy, next_id, &only, kLinkHome)) laid.home++;
  }
  return laid;
}

void lay_links(const LinkBuilder& b, const std::function<std::string()>& next_id) {
  const std::deque<WalkedRun>& walked = *b.walked;
  const auto pistes = [&](const std::string& from) {
    std::vector<int> out;
    for (size_t i = 0; i < walked.size(); i++) {
      if (walked[i].spec.kind == RunKind::piste && walked[i].spec.from == from) {
        out.push_back(static_cast<int>(i));
      }
    }
    return out;
  };
  // THE SECTORS across the face, by where their pistes run.
  struct Sector {
    std::string id;
    double x;
  };
  std::vector<Sector> sectors;
  for (const WalkedRun& w : walked) {
    if (w.spec.kind != RunKind::piste) continue;
    if (std::any_of(sectors.begin(), sectors.end(),
                    [&](const Sector& s) { return s.id == w.spec.from; })) {
      continue;
    }
    sectors.push_back({w.spec.from, 0});
  }
  for (Sector& sec : sectors) {
    double sx = 0;
    double k = 0;
    for (const int r : pistes(sec.id)) {
      for (const TrackPoint& p : walked[size_t(r)].track.points) {
        sx += p.x;
        k++;
      }
    }
    sec.x = sx / num::max(1.0, k);
  }
  std::stable_sort(sectors.begin(), sectors.end(),
                   [](const Sector& a, const Sector& c) { return a.x - c.x < 0; });
  // Each two sectors side by side, those no run or lane links yet first.
  const auto linked = [&](const std::string& a, const std::string& c) {
    for (const WalkedRun& w : walked) {
      const std::string* into = w.into ? &walked[size_t(w.into->run)].spec.from : nullptr;
      if ((w.spec.from == a && into && *into == c) || (w.spec.from == c && into && *into == a)) {
        return true;
      }
    }
    return false;
  };
  struct Pair {
    size_t a, c;  // indices into `sectors`
  };
  std::vector<Pair> order;
  for (size_t i = 1; i < sectors.size(); i++) {
    if (!linked(sectors[i - 1].id, sectors[i].id)) order.push_back({i - 1, i});
  }
  for (size_t i = 1; i < sectors.size(); i++) {
    if (linked(sectors[i - 1].id, sectors[i].id)) order.push_back({i - 1, i});
  }
  std::vector<Pair> back;
  for (const Pair& pr : order) {
    if (!room(b)) break;
    if (branch(b, pistes(sectors[pr.a].id), sectors[pr.c].id, 1, next_id)) {
      back.push_back({pr.c, pr.a});
    } else if (branch(b, pistes(sectors[pr.c].id), sectors[pr.a].id, -1, next_id)) {
      back.push_back({pr.a, pr.c});
    }
  }
  // …and, while the area still has room for lane, the way back too.
  for (const Pair& pr : back) {
    if (!room(b)) break;
    const double side = num::sign(double(pr.c) - double(pr.a));
    branch(b, pistes(sectors[pr.a].id), sectors[pr.c].id, side, next_id);
  }
}

}  // namespace ge::mountain::mapgen
