// The walk of one run down the mountain, its smoothing, and the colour it measures.
//
// ge:summary The walk of a run: heading choice, merging, lanes crossing, the finish and checks
// ge:keywords network walk-run choose-heading smooth-run run-colour follow-route heading-at
// ge:skill mountain
// ge:test tests/mountain_resort_build_test.cpp
#include "mountain/mapgen/network.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "core/noise.hpp"
#include "core/num.hpp"
#include "core/num_text.hpp"
#include "mountain/mapgen/track.hpp"

namespace ge::mountain::mapgen {

namespace num = ge::core::num;
namespace libm = ge::core::libm;
namespace R = rules;
namespace RR = resort_rules;
using ge::core::angle_diff;
using ge::core::clamp;
using ge::core::hypot;
using ge::core::sample_field;
using ge::core::smoothstep;

namespace {

constexpr double kInf = std::numeric_limits<double>::infinity();

// How much further off than the clearance a walk keeps from another run: the line is smoothed
// and its widths read again afterwards, and either moves an edge a few metres.
constexpr double kWalkMargin = 12;
// The look-aheads a heading is judged over, m.
constexpr double kLookAhead[] = {20, 40, 60};
// The headings a step chooses among, every this many rad either side of the fall line.
constexpr double kHeadingStep = 0.06;
// THE ZIG-ZAG: the side a traverse holds costs the other this much; it turns back when it has
// strayed this far off the line to its target, m, or held one side this long, m.
constexpr double kZigzagSide = 0.8;
constexpr double kZigzagBand = 110;
constexpr double kZigzagHold = 420;
// Turning away from a run it may not merge into: by how much, rad, for how long, m, and how far
// inside the clearance it may come before it is refused, m.
constexpr double kRepelTurn = 0.7;
constexpr double kRepelHold = 40;
constexpr double kRepelSlack = 14;
// How far past its inside bench a bend's centre stands, m (R18).
constexpr double kBendRoom = 10;
// The wave's shape, the ease into and out of a sweep (m), and the look-aheads for a run to meet.
constexpr double kWaveShape = 0.6;
constexpr double kSweepEase = 40;
constexpr double kAhead[] = {30, 60, 90, 120, 150};
// How close to the other's line, and how true to its heading, a merging run is one with it.
constexpr double kJoinLateral = 2.5;
constexpr double kJoinHeading = 0.12;
// How far a run may run closing on another before the merge is given up on, and a lane on its
// traverse to the run it joins, m.
constexpr double kMergeMost = 700;
constexpr double kJoinMost = 1600;
// How hard a merging run turns in toward the other's line: rad per metre off it, and the most.
constexpr double kMergeGain = 1.0 / 50;
constexpr double kMergeTurn = 0.45;
// How far off the other's heading a run may be pointed at its edge and still be merging, rad.
constexpr double kCrossing = 0.9;
// A lane is let pass a run it crossed for this many metres.
constexpr double kCrossPast = 140;
// How near the end of its route a link lane ends at its station, m.
constexpr double kGoalEnd = 6;
// The least angle a lane crosses a run at, rad; and inside another's clearance; and how much
// further off than the clearance it keeps from a run it does not cross, m.
constexpr double kCrossLeast = 0.5;
constexpr double kLaneSquare = 0.6;
constexpr double kLaneMargin = 10;
// No neck closes a piste this near its top station, m.
constexpr double kNeckFrom = 150;
// How far apart the ends of a run may come out of the smoothing before the tail is drawn back.
constexpr double kHeld = 0.05;

// The heading a lane takes to follow its route: toward a point about forty metres past the
// route's point nearest it. `at` only moves on.
double follow_route(const std::vector<PlanPoint>& route, double x, double z, size_t& at) {
  const auto d2 = [&](size_t k) {
    const double dx = route[k].x - x;
    const double dz = route[k].z - z;
    return dx * dx + dz * dz;
  };
  while (at + 1 < route.size() && d2(at + 1) <= d2(at)) at++;
  size_t j = at;
  double run = 0;
  while (j + 1 < route.size() && run < 40) {
    run += hypot(route[j + 1].x - route[j].x, route[j + 1].z - route[j].z);
    j++;
  }
  const PlanPoint& t = route[j];
  return libm::atan2(t.x - x, num::max(0.5, t.z - z));
}

// A run's heading at arc `s`, off its nearest station.
double heading_at(const std::vector<TrackPoint>& points, double s) {
  const double last = points.back().s;
  const double i = num::round((clamp(s, 0, last) / num::max(1e-9, last)) *
                              static_cast<double>(points.size() - 1));
  return points[static_cast<size_t>(i)].heading;
}

// THE WALK SMOOTHED: the headings through a triangular window and the line laid again from the
// start along them — and, on a run that ends on another's line or at a station, the tail drawn
// back so it still ends where the walk ended. Writes x, z and heading in place.
void smooth_run(std::vector<TrackPoint>& points, const RunSpec& spec,
                const std::optional<PlanPoint>& end) {
  const size_t n = points.size();
  std::vector<double> raw(n);
  for (size_t i = 0; i < n; i++) raw[i] = points[i].heading;
  const double step = R::track::step;
  constexpr int K = 5;
  double x = spec.x;
  double z = spec.z;
  const int last = static_cast<int>(n) - 1;
  for (int i = 1; i < static_cast<int>(n); i++) {
    double sum = 0;
    double norm = 0;
    for (int j = -K; j <= K; j++) {
      const double w = K + 1 - std::abs(j);
      sum += raw[size_t(std::clamp(i + j, 1, last))] * w;
      norm += w;
    }
    const double h = sum / norm;
    points[size_t(i)].heading = h;
    x += libm::sin(h) * step;
    z += libm::cos(h) * step;
    points[size_t(i)].x = x;
    points[size_t(i)].z = z;
  }
  if (!end) return;
  const double dx = end->x - points[n - 1].x;
  const double dz = end->z - points[n - 1].z;
  if (std::abs(dx) < kHeld && std::abs(dz) < kHeld) return;
  const int span = std::min(last, static_cast<int>(num::round(240 / step)));
  for (int i = last - span; i <= last; i++) {
    const double w = smoothstep(0, 1, double(i - (last - span)) / span);
    points[size_t(i)].x += dx * w;
    points[size_t(i)].z += dz * w;
  }
}

struct Sweep {
  double at, length, angle;
};
struct Neck {
  double from, to;
};

}  // namespace

int rank_of(const RunSpec& spec) {
  if (spec.kind == RunKind::road || !spec.row->id) return 0;
  switch (*spec.row->id) {
    case PisteGrade::blue:
      return 1;
    case PisteGrade::red:
      return 2;
    case PisteGrade::black:
      return 3;
    default:
      return 0;
  }
}

double choose_heading(const ge::core::Heightfield& ground, double x, double z, double want,
                      double current, double cap, std::optional<double> aim, double swing,
                      int side) {
  const double y0 = sample_field(ground, x, z);
  double best = want;
  double best_cost = kInf;
  const double least = R::track::min_fall;
  const double w = R::track::grade_window;
  const double down =
      libm::atan2(-(sample_field(ground, x + w, z) - sample_field(ground, x - w, z)),
                  -(sample_field(ground, x, z + w) - sample_field(ground, x, z - w)));
  double h = -swing;
  while (h <= swing + 1e-9) {
    const double fx = libm::sin(h);
    const double fz = libm::cos(h);
    double over = 0;
    double under = 0;
    double fall_sum = 0;
    for (const double L : kLookAhead) {
      const double fall = (y0 - sample_field(ground, x + fx * L, z + fz * L)) / L;
      over = num::max(over, fall - cap);
      under = num::max(under, least - fall);
      fall_sum += fall;
    }
    double cost = num::max(0.0, over) * 40 + num::max(0.0, under) * 40;
    if (aim) cost += std::abs(fall_sum / 3 - *aim) * 12;
    cost += std::abs(angle_diff(h, want)) * 0.35 + std::abs(angle_diff(h, current)) * 0.25;
    if (side != 0) {
      const double off = angle_diff(down, h);
      if (std::abs(off) > 0.12 && num::sign(off) != side) cost += kZigzagSide;
    }
    if (cost < best_cost) {
      best_cost = cost;
      best = h;
    }
    h += kHeadingStep;
  }
  return best;
}

std::variant<WalkedRun, std::string> walk_run(ge::core::Rng& rng, const TerrainPlan& plan,
                                              const ge::core::Heightfield& ground,
                                              const RunSpec& spec, const NetIndex& net,
                                              const SharedTop& shared) {
  namespace T = R::track;
  namespace N = RR::network;
  namespace NK = RR::piste::neck;
  const GradeRow& G = *spec.row;
  const bool road = spec.kind == RunKind::road;
  const double step = T::step;
  const double size = plan.size;
  // Everything drawn is drawn here, in this order.
  const double width_seed = ge::core::int_in(rng, 1, 1 << 30);
  const double bend_seed = ge::core::int_in(rng, 1, 1 << 30);
  const double period = in_band(rng, T::wander_scale);
  double phase = ge::core::range(rng, 0, ge::core::kTau);
  std::vector<Sweep> sweeps;
  const int n_sweeps =
      road ? 0 : ge::core::int_in(rng, 0, static_cast<int32_t>(G.track.sweeps.max));
  for (int i = 0; i < n_sweeps; i++) {
    Sweep sw{};
    sw.at = ge::core::range(rng, 0.15, 0.7) * N::sweep_reach;
    sw.length = in_band(rng, T::sweeps_length);
    const double angle = in_band(rng, T::sweeps_angle);
    sw.angle = angle * (ge::core::chance(rng, 0.5) ? 1 : -1);
    sweeps.push_back(sw);
  }
  // THE NECKS down a piste (R27): where it tightens and opens out again.
  std::vector<Neck> necks;
  if (!road) {
    const double first = in_band(rng, NK::every);
    double at = first * ge::core::range(rng, 0.3, 0.8);
    while (at < N::longest) {
      if (at >= kNeckFrom) necks.push_back({at, at + in_band(rng, NK::length)});
      at += in_band(rng, NK::every);
    }
  }
  const double neck_w = G.id ? NK::width[size_t(*G.id)] : 0;
  const double swing = road ? RR::road::swing : T::swing;
  // The pitch a run's walk holds its traverses to: well under its colour's ceiling (R27); a
  // black's is its row's own.
  const double steepest = road ? RR::road::grade * RR::road::steer
                          : (!G.id || *G.id == PisteGrade::black) ? G.track.steepest
                                                                  : G.steepest.max * N::traverse;
  const double aim_pitch = (RR::road::aim.min + RR::road::aim.max) / 2;
  const Band W = road ? RR::road::width : G.id ? RR::piste::width[size_t(*G.id)] : G.track.width;
  // The tree line as a height: a piste opens out on the open snow above it.
  const double line_y = plan.tree_line - plan.altitude;
  const double z_end = size * T::finish_z;
  const double edge = plan.flank_band.inner - 140;
  const auto width_at = [&](const TrackPoint& p) {
    const double v = ge::core::value_noise(p.s, 0, T::width_scale, width_seed);
    double wide = W.min + (W.max - W.min) * smoothstep(0.15, 0.85, v);
    if (road) return wide;
    const double y = sample_field(ground, p.x, p.z);
    wide = num::min(RR::piste::most,
                    wide * (1 + RR::piste::open * smoothstep(line_y - 60, line_y + 60, y)));
    const double rx = libm::cos(p.heading);
    const double rz = -libm::sin(p.heading);
    const double reach = wide / 2;
    const double fall = (sample_field(ground, p.x + rx * reach, p.z + rz * reach) -
                         sample_field(ground, p.x - rx * reach, p.z - rz * reach)) /
                        wide;
    wide -= (wide - W.min) * smoothstep(T::narrow.min, T::narrow.max, std::abs(fall));
    const Neck* neck = nullptr;
    for (const Neck& k : necks) {
      if (p.s > k.from && p.s < k.to) {
        neck = &k;
        break;
      }
    }
    if (!neck || neck_w >= wide) return wide;
    const double close = smoothstep(neck->from, neck->from + NK::ease, p.s) *
                         (1 - smoothstep(neck->to - NK::ease, neck->to, p.s));
    return wide + (neck_w - wide) * close;
  };
  const auto turn_for = [&](double width, double grade) {
    return (step / num::max(bend_floor(grade), width / 2 + kBench + kBendRoom + 4)) * 0.85;
  };
  const auto pitch_at = [&](double px, double pz, double heading) {
    constexpr double L = 30;
    const double dx = libm::sin(heading) * L;
    const double dz = libm::cos(heading) * L;
    const double here = sample_field(ground, px, pz);
    return num::max((here - sample_field(ground, px + dx, pz + dz)) / L,
                    (sample_field(ground, px - dx, pz - dz) - here) / L);
  };
  WalkedRun out{};
  std::vector<TrackPoint>& points = out.track.points;
  double x = spec.x;
  double z = spec.z;
  double h = spec.heading;
  double s = 0;
  points.push_back({x, z, 0, s, h, 0});
  points[0].width = width_at(points[0]);
  NetHit hit;
  int merging = -1;
  bool joining = false;
  size_t on_route = 0;
  std::map<int, double> crossed;
  const auto last_s = [&](int r) { return net.runs[size_t(r)].points->back().s; };
  // Whether a run's stretch at arc `at` is its finish near the floor, which a run coming into the
  // village beside it passes by (R27).
  const auto at_finish = [&](int r, double at) {
    return z >= z_end - N::finish && at > last_s(r) - N::finish;
  };
  // The runs a walk does not meet here: its own top's, near it; a run a lane has crossed, past
  // it; a finish near the floor.
  const auto skip = [&](int r, double at) {
    const auto c = crossed.find(r);
    return (shared(r) && s < N::shared) || (c == crossed.end() ? -1.0 : c->second) > s ||
           at_finish(r, at);
  };
  NetHit third;
  NetHit beside;
  // The runs a lane may come near: the pistes off its own top, near it; the run it joins, from
  // where it turns to join it, and any other joining that run there.
  const auto lane_skip = [&](int r, double at) {
    const NetIndex::Run& o = net.runs[size_t(r)];
    return (shared(r) && !o.road && num::min(s, at) < N::sibling) ||
           (joining && (r == merging || (o.into == merging && at >= o.from)));
  };
  bool pressed = false;
  struct {
    double side;
    double until;
  } repel{0, -1};
  int turned = 0;
  const int rank = rank_of(spec);
  double merge_from = kInf;
  double core_from = kInf;
  double merge_start = 0;
  std::optional<RunJoin> into;
  // The zig-zag's side (+1 toward +x of the fall line) and where it began.
  int zz = spec.lean >= 0 ? 1 : -1;
  double zz_from = 0;
  const double line_x = spec.target.x - spec.x;
  const double line_z = spec.target.z - spec.z;
  const double line_len0 = hypot(line_x, line_z);
  const double line_len = line_len0 == 0 ? 1 : line_len0;
  const double most = std::ceil(N::longest / step);
  const bool has_route = !spec.route.empty();
  const auto toward_heading = [&](double tx, double tz) {
    return clamp(libm::atan2(tx - x, num::max(1.0, tz - z)), -swing, swing);
  };
  for (;;) {
    if (static_cast<double>(points.size()) > most) {
      return std::string("the run runs out of length before it ends");
    }
    const double hw = points.back().width;
    double want = h;
    if (merging >= 0 && joining) {
      // A LANE JOINING its run: on along its own traverse toward the point it was laid to, at a
      // lane's pitch, until its line meets the run's.
      net.nearest(x, z, 600, [&](int r, double) { return r != merging; }, hit);
      if (hit.distance == kInf) return std::string("a lane lost the run it was joining");
      if (hit.distance < hit.width / 2 + kBench + hw / 2) merge_from = num::min(merge_from, s);
      if (hit.distance < hit.width / 2 + T::shoulder_flat) core_from = num::min(core_from, s);
      if (hit.distance < num::max(kJoinLateral * 2, hit.width / 4)) {
        into = RunJoin{merging, hit.s};
        break;
      }
      if (has_route && on_route >= spec.route.size() - 1) {
        const PlanPoint& end = spec.route.back();
        if (hypot(end.x - x, end.z - z) > 30) {
          return std::string("a lane runs past the end of its route");
        }
      }
      if (s - merge_start > kJoinMost) return std::string("a lane never reaches the run it joins");
      if (has_route) {
        want = clamp(follow_route(spec.route, x, z, on_route), -swing, swing);
      } else {
        const bool to_target = spec.join && *spec.join == merging;
        const double tx = to_target ? spec.target.x : hit.x;
        const double tz = to_target ? spec.target.z : hit.z;
        want = choose_heading(ground, x, z, toward_heading(tx, tz), h, steepest, aim_pitch, swing);
      }
    } else if (merging >= 0) {
      // MERGING: the other's heading, turned in toward its line.
      net.nearest(x, z, 300, [&](int r, double) { return r != merging; }, hit);
      if (hit.distance == kInf) return std::string("a run lost the one it was merging into");
      if (std::abs(hit.lateral) < hw / 2 + hit.width / 2 + kBench) {
        merge_from = num::min(merge_from, s);
      }
      if (std::abs(hit.lateral) < hit.width / 2 + T::shoulder_flat) {
        core_from = num::min(core_from, s);
      }
      const double off = angle_diff(hit.heading, h);
      if (std::abs(hit.lateral) < hit.width / 2 && std::abs(off) > kCrossing) {
        return std::string("a run crosses another");
      }
      // Never into the other's last stretch, nor its first.
      const NetIndex::Run& other = net.runs[size_t(merging)];
      const double last = other.points->back().s;
      if (hit.s > last - (other.into < 0 ? kJoinFinish : kJoinEnd)) {
        return std::string("a run merges into another's finish");
      }
      if (hit.s < kJoinEnd && std::abs(hit.lateral) < hit.width / 2 + kBench + hw / 2) {
        return std::string("a run merges into another's start");
      }
      if (std::abs(hit.lateral) < kJoinLateral && std::abs(off) < kJoinHeading) {
        into = RunJoin{merging, hit.s};
        break;
      }
      if (s - merge_start > kMergeMost) {
        return std::string("a run never closes on the one it merges into");
      }
      // On its way in it keeps clear of every third run but one merging into the same run.
      net.nearest(
          x, z, hw / 2 + 2 * kBench + N::gap + kWidest,
          [&](int r, double at) {
            const NetIndex::Run& o = net.runs[size_t(r)];
            return r == merging || (o.into == merging && at >= o.from) || skip(r, at);
          },
          third);
      if (third.distance < clearance(hw, third.width)) {
        return std::string("a run is pressed against a run it may not merge into");
      }
      want = clamp(hit.heading - clamp(hit.lateral * kMergeGain, -kMergeTurn, kMergeTurn), -swing,
                   swing);
    } else if (s >= T::hold) {
      const double vary =
          1 + T::wander_vary * (ge::core::value_noise(s, 0, 500, bend_seed) * 2 - 1);
      phase += (ge::core::kTau * step) / (period * vary);
      const double wave = libm::sin(phase);
      double bend = spec.amplitude * num::sign(wave) * libm::pow(std::abs(wave), kWaveShape);
      for (const Sweep& sw : sweeps) {
        const double in = s - sw.at;
        if (in <= -kSweepEase || in >= sw.length + kSweepEase) continue;
        const double w = smoothstep(-kSweepEase, 0, in) *
                         (1 - smoothstep(sw.length, sw.length + kSweepEase, in));
        bend = bend * (1 - w) + sw.angle * w;
      }
      // Away from a run it may not merge into.
      if (repel.until > s) bend += repel.side * kRepelTurn;
      // THE CENTRE: the bearing to the target, then the fall line below it.
      const PlanPoint& aim = spec.target;
      const double tz = num::max(aim.z, z + 60);
      const double level = road ? 0 : smoothstep(spec.target.z - 120, spec.target.z, z);
      const double off = road ? RR::road::swing : N::centre;
      const double centre = clamp(libm::atan2(aim.x - x, tz - z), -off, off) * (1 - level);
      bend += centre;
      // THE FINISH: turned to the fall line for the village.
      bend *= 1 - smoothstep(z_end - T::finish_turn, z_end - 20, z);
      // THE FACE: turned back from the side ridges.
      const double across = x - size / 2;
      const double away = smoothstep(edge, edge + 120, std::abs(across));
      bend = bend * (1 - away) + -num::sign(across) * 0.8 * away;
      if (!road) {
        // How far off the line to its target the run has strayed, + toward +x of it.
        const double strayed = ((x - spec.x) * line_z - (z - spec.z) * line_x) / line_len;
        const bool flip = (zz > 0 && strayed > kZigzagBand) || (zz < 0 && strayed < -kZigzagBand) ||
                          s - zz_from > kZigzagHold;
        if (flip) {
          zz = s - zz_from > kZigzagHold ? (strayed > 0 ? -1 : 1) : -zz;
          zz_from = s;
        }
      }
      want = has_route ? clamp(follow_route(spec.route, x, z, on_route), -swing, swing)
                       : choose_heading(ground, x, z, clamp(bend, -swing, swing), h, steepest,
                                        road ? std::optional<double>(aim_pitch) : std::nullopt,
                                        swing, road ? 0 : zz);
    }
    const double max_turn = turn_for(hw, pitch_at(x, z, h) * G.track.pitch);
    h = clamp(h + clamp(angle_diff(h, want), -max_turn, max_turn), -swing, swing);
    x += libm::sin(h) * step;
    z += libm::cos(h) * step;
    s += step;
    TrackPoint p{x, z, 0, s, h, 0};
    p.width = width_at(p);
    points.push_back(p);
    if (merging < 0 && (road || s >= T::hold)) {
      // ANOTHER RUN NEAR: beside us, or ahead along the heading. A piste merges into it; a lane
      // merges into one it meets going its way down a gentle line, and crosses any other.
      const double reach = p.width / 2 + 2 * kBench + N::gap + kWidest;
      const auto meet = [&]() {
        const NetIndex::Run& o = net.runs[size_t(hit.run)];
        // A piste merges only into a run built no harder than itself (R29), and not into one it
        // left its top with until it has run its own way.
        if (!road && ((shared(hit.run) && s < N::sibling) || o.rank > rank)) {
          if (o.rank > rank && repel.until < s) turned++;
          repel = {num::sign(hit.lateral) == 0 ? 1.0 : num::sign(hit.lateral), s + kRepelHold};
          if (hit.distance < clearance(p.width, hit.width) - kRepelSlack) pressed = true;
          return false;
        }
        if (!road) return true;
        // A lane joins the run it was laid to, and any it meets running its own way.
        if ((spec.join && hit.run == *spec.join) ||
            (!has_route && std::abs(angle_diff(hit.heading, h)) < kCrossLeast)) {
          joining = true;
          return true;
        }
        crossed[hit.run] = s + kCrossPast;
        return false;
      };
      net.nearest(x, z, reach, skip, hit);
      if (hit.distance < kInf && hit.distance < clearance(p.width, hit.width) + kWalkMargin) {
        if (meet()) {
          merging = hit.run;
          merge_start = s;
        }
      } else {
        for (const double d : kAhead) {
          const double ax = x + libm::sin(h) * d;
          const double az = z + libm::cos(h) * d;
          net.nearest(ax, az, reach, skip, hit);
          if (hit.distance < kInf && hit.distance < clearance(p.width, hit.width) + kWalkMargin) {
            if (meet()) {
              merging = hit.run;
              merge_start = s;
            }
            break;
          }
        }
      }
    }
    if (!road && s < N::sibling) {
      // OFF A SHARED TOP: beside the runs that left it with this one, never onto one.
      net.nearest(x, z, kWidest / 2 + kBench, [&](int r, double) { return !shared(r); }, beside);
      if (beside.distance < beside.width / 2 + kBench) {
        return std::string("a run runs over one that left its top with it");
      }
    }
    if (road) {
      // A LANE BESIDE A RUN it does not join (R27): it crosses a piste square or keeps the gap
      // from it, and never meets another lane.
      net.nearest(x, z, p.width / 2 + 2 * kBench + N::gap + kWidest, lane_skip, beside);
      if (beside.distance < clearance(p.width, beside.width) + kLaneMargin &&
          (net.runs[size_t(beside.run)].road ||
           std::abs(angle_diff(beside.heading, h)) < kLaneSquare)) {
        return std::string("a lane runs beside a run");
      }
    }
    if (!road && spec.avoid && spec.avoid(x, z, p.width / 2)) {
      return std::string("a run runs over a drag lift's line");
    }
    if (pressed) return std::string("a run is pressed against a run it may not merge into");
    // A LINK LANE ends at the lift's bottom station it was laid to.
    if (spec.to) {
      if (has_route) {
        const PlanPoint& end = spec.route.back();
        if (hypot(end.x - x, end.z - z) < kGoalEnd) break;
        if (on_route >= spec.route.size() - 1 && z > end.z + kGoalEnd) {
          return std::string("a lane runs past the end of its route");
        }
      }
      continue;
    }
    if (merging < 0 && z >= z_end) break;
  }
  const bool station = spec.to.has_value();
  const int straight = static_cast<int>(num::round(T::finish / step));
  if (!into && !station) {
    // THE FINISH STRAIGHT, opening out into the village.
    const double arena = points.back().width;
    const double open = num::max(arena, T::width.max);
    for (int k = 1; k <= straight; k++) {
      x += libm::sin(h) * step;
      z += libm::cos(h) * step;
      s += step;
      points.push_back(
          {x, z, 0, s, h, arena + (open - arena) * smoothstep(0, 0.5, double(k) / straight)});
    }
  }
  const double length = s;
  if (length < (road ? RR::road::shortest : N::shortest)) {
    return "a run comes out " + num::to_fixed(length, 0) + " m long";
  }
  smooth_run(points, spec,
             into || station ? std::optional<PlanPoint>(PlanPoint{x, z}) : std::nullopt);
  if (!into && !station) {
    const size_t walked = points.size() - size_t(straight) - 1;
    for (size_t i = 0; i <= walked; i++) points[i].width = width_at(points[i]);
    for (int k = 1; k <= straight; k++) {
      const double open = smoothstep(0, 0.5, double(k) / straight);
      const double w0 = points[walked].width;
      points[walked + size_t(k)].width = w0 + (num::max(w0, T::width.max) - w0) * open;
    }
  } else {
    for (TrackPoint& q : points) q.width = width_at(q);
  }
  set_headings(points);
  const size_t n = points.size();
  out.spec = spec;
  out.track.length = length;
  out.raw.assign(n, 0);
  out.cross.assign(n, 0);
  out.into = into;
  out.merge_from = into ? num::min(merge_from, length) : length;
  out.core_from = into ? num::min(core_from, length) : length;
  out.merge_start = into ? merge_start : length;
  out.turned = turned;
  const BendReading bend = tightest_bend(
      out.track, [&](size_t i) { return sample_field(ground, points[i].x, points[i].z); });
  if (bend.radius < bend.floor) {
    return "a bend tightens to " + num::to_fixed(bend.radius, 0) + " m on a " +
           num::to_fixed(bend.grade * 100, 0) + " % pitch";
  }
  if (bend_room(out.track) < kBendRoom) return std::string("a bend folds the inside bench");
  if (min_separation(out.track) < T::separation_plan) {
    return std::string("a run passes close by itself");
  }
  if (road) {
    // A LANE AS SMOOTHED, held to the gap again (R27), but where it leaves its piste and where
    // it joins its run.
    const int onto = into ? into->run : -1;
    for (const TrackPoint& q : points) {
      const auto skip_lane = [&](int r, double at) {
        const NetIndex::Run& o = net.runs[size_t(r)];
        return (shared(r) && !o.road && num::min(q.s, at) < N::sibling) ||
               (r == onto && q.s >= merge_start) ||
               (onto >= 0 && o.into == onto && q.s >= merge_start && at >= o.from) ||
               (!o.road &&
                std::abs(angle_diff(heading_at(*o.points, at), q.heading)) >= kCrossLeast);
      };
      net.nearest(q.x, q.z, q.width / 2 + 2 * kBench + N::gap + kWidest, skip_lane, beside);
      if (beside.distance < clearance(q.width, beside.width)) {
        return std::string("a lane runs beside a run");
      }
    }
  }
  if (!into && !road) {
    // THE VILLAGE: a run that reaches the floor comes in beside the others' finishes (R27).
    const double from = length - N::finish - T::finish;
    for (const TrackPoint& q : points) {
      if (q.s < from) continue;
      net.nearest(
          q.x, q.z, kWidest + 2 * kBench,
          [&](int r, double) { return shared(r) && q.s < N::shared; }, hit);
      if (hit.distance < q.width / 2 + hit.width / 2 + 2 * kBench) {
        return std::string("a run comes into the village over another");
      }
    }
  }
  const double reach = kWidest / 2 + kBench + T::bank_max + 10;
  for (const TrackPoint& q : points) {
    if (q.x < reach || q.x > size - reach || q.z < reach || q.z > size - reach) {
      return std::string("a run runs off the map");
    }
    const double rx = libm::cos(q.heading) * (q.width / 2 + kBench);
    const double rz = -libm::sin(q.heading) * (q.width / 2 + kBench);
    if (flank_at(plan, q.x + rx, q.z + rz) > 0.05 || flank_at(plan, q.x - rx, q.z - rz) > 0.05) {
      return std::string("a run runs up a side ridge");
    }
  }
  return out;
}

PisteGrade run_colour(const std::vector<TrackPoint>& points, const std::vector<TrackPoint>* onto) {
  size_t n = points.size();
  if (onto) {
    for (size_t i = n - 1; i > 1; i--) {
      const TrackPoint& p = points[i];
      double best = kInf;
      double width = 0;
      for (const TrackPoint& q : *onto) {
        const double dx = q.x - p.x;
        const double dz = q.z - p.z;
        const double d = dx * dx + dz * dz;
        if (d < best) {
          best = d;
          width = q.width;
        }
      }
      if (std::sqrt(best) >= width / 2) break;
      n = i;
    }
  }
  Track own;
  own.points.assign(points.begin(), points.begin() + std::ptrdiff_t(std::max<size_t>(2, n)));
  own.length = own.points.back().s;
  return piste_grade_of(steepest_span(own, {}));
}

}  // namespace ge::mountain::mapgen
