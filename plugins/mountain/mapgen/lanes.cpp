// A lane's route: the nearest-run raster, the costs, the row-by-row search.
//
// ge:summary The lane search: each cell's nearest run, the step costs, the row-by-row sweep
// ge:keywords lanes route-lane nearest-raster moves pitch-band turn-cost beside-cost joinable
// ge:skill mountain
// ge:test tests/mountain_resort_build_test.cpp
#include "mountain/mapgen/lanes.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "core/num.hpp"
#include "core/parallel.hpp"
#include "mountain/mapgen/network.hpp"
#include "mountain/mapgen/resort_rules.hpp"
#include "mountain/mapgen/rules.hpp"

namespace ge::mountain::mapgen {

namespace num = ge::core::num;
namespace libm = ge::core::libm;
namespace R = rules;
namespace RR = resort_rules;
using ge::core::angle_diff;
using ge::core::hypot;
using ge::core::sample_field;

namespace {

constexpr double kInf = std::numeric_limits<double>::infinity();

// The route's grid, m a cell.
constexpr double kCell = 8;

// A step of the search, in cells across (+x) and down the map, its heading and its length, m.
struct Move {
  int dx, dz;
  double heading, length;
};
constexpr int kMoveCells[][2] = {{-1, 0}, {-4, 1}, {-3, 1}, {-2, 1}, {-1, 1},
                                 {-1, 2}, {-1, 3}, {0, 1},  {1, 3},  {1, 2},
                                 {1, 1},  {2, 1},  {3, 1},  {4, 1},  {1, 0}};
constexpr int M = 15;
constexpr int kWest = 0;
constexpr int kEast = M - 1;
constexpr int kDeepest = 3;  // the most rows one step spans

// What a step costs, per metre of it, past its length: a metre of pitch outside the band per
// metre run, the cross-slope past what a lane's bench takes in its stride, and running beside a
// piste; and how far past the band a single step may fall.
constexpr double kPitchCost = 60;
constexpr double kHardMin = 0;
constexpr double kHardMax = 1.75;
constexpr double kCrossCost = 3;
constexpr double kCrossFree = 0.35;
constexpr double kBesideCost = 8;
// What joining the other way from the slot's costs: per metre, as if the lane were longer.
constexpr double kOtherWay = 0.6;
// How near a piste's line a cell must lie to be a place to join it, m.
constexpr double kOnLine = 6;
// The least angle a step crosses a run's corridor at, rad.
constexpr double kSquare = 0.65;
// The tightest a route turns, m of radius; what a radian of turn costs, as metres of lane.
constexpr double kBend = 45;
constexpr double kTurnCost = 20;
// How far from its top station a lane runs beside the pistes that leave it, m.
constexpr double kTopFree = 80;
// How much further off than the clearance a route keeps from a run it is not crossing, m.
constexpr double kMargin = 10;
// How far past the route's grid the runs are read, cells.
constexpr int kPad = 16;
// How near a lift's bottom station a route must come to end at it, m.
constexpr double kGoalReach = 12;

struct Moves {
  std::array<Move, M> moves;
  std::array<double, size_t(M) * size_t(M)> turn;
  std::array<double, size_t(M) * size_t(M)> turn_most;
  std::vector<int> down;
};

const Moves& moves() {
  static const Moves table = [] {
    Moves t{};
    for (int m = 0; m < M; m++) {
      const int dx = kMoveCells[m][0];
      const int dz = kMoveCells[m][1];
      t.moves[size_t(m)] = {dx, dz, libm::atan2(dx, dz), hypot(dx * kCell, dz * kCell)};
      if (dz > 0) t.down.push_back(m);
    }
    for (int a = 0; a < M; a++) {
      for (int b = 0; b < M; b++) {
        const Move& ma = t.moves[size_t(a)];
        const Move& mb = t.moves[size_t(b)];
        t.turn[size_t(a) * size_t(M) + size_t(b)] = std::abs(mb.heading - ma.heading);
        t.turn_most[size_t(a) * size_t(M) + size_t(b)] = (mb.length + ma.length) / 2 / kBend;
      }
    }
    return t;
  }();
  return table;
}

// Whether a lane may join run `r` at arc `s` (R27): not in its first stretch nor its last.
bool joinable(const NetIndex& net, int r, double s) {
  const NetIndex::Run& run = net.runs[size_t(r)];
  const double last = run.points->back().s;
  return s > kJoinEnd && s < last - (run.into < 0 ? kJoinFinish : kJoinEnd);
}

struct Raster {
  std::vector<double> dist;
  std::vector<int> run;
  std::vector<double> heading, width, arc;
};

// EVERY CELL'S NEAREST RUN on the route's grid: the stations seed the cells they stand in, and
// two sweeps carry each cell's nearest seed to its neighbours.
Raster nearest_raster(const NetIndex& net, double left, double z0, int cols, int rows) {
  const int W = cols + 2 * kPad;
  const int H = rows + 2 * kPad;
  const double x0 = left - kPad * kCell;
  const double zz0 = z0 - kPad * kCell;
  const size_t wh = size_t(W) * size_t(H);
  std::vector<int> seed_run(wh, -1);
  std::vector<int> seed_idx(wh, 0);
  std::vector<double> d2(wh, kInf);
  for (size_t r = 0; r < net.runs.size(); r++) {
    const std::vector<TrackPoint>& pts = *net.runs[r].points;
    for (size_t i = 0; i < pts.size(); i++) {
      const double c = num::round((pts[i].x - x0) / kCell);
      const double q = num::round((pts[i].z - zz0) / kCell);
      if (c < 0 || q < 0 || c >= W || q >= H) continue;
      const size_t o = size_t(q) * size_t(W) + size_t(c);
      const double ex = x0 + c * kCell - pts[i].x;
      const double ez = zz0 + q * kCell - pts[i].z;
      const double d = ex * ex + ez * ez;
      if (d < d2[o]) {
        d2[o] = d;
        seed_run[o] = static_cast<int>(r);
        seed_idx[o] = static_cast<int>(i);
      }
    }
  }
  const auto take = [&](size_t o, size_t from, double cx, double cz) {
    const int r = seed_run[from];
    if (r < 0) return;
    const TrackPoint& p = (*net.runs[size_t(r)].points)[size_t(seed_idx[from])];
    const double ex = cx - p.x;
    const double ez = cz - p.z;
    const double d = ex * ex + ez * ez;
    if (d < d2[o]) {
      d2[o] = d;
      seed_run[o] = r;
      seed_idx[o] = seed_idx[from];
    }
  };
  const size_t w = size_t(W);
  for (int q = 0; q < H; q++) {
    for (int c = 0; c < W; c++) {
      const size_t o = size_t(q) * w + size_t(c);
      const double cx = x0 + c * kCell;
      const double cz = zz0 + q * kCell;
      if (c > 0) take(o, o - 1, cx, cz);
      if (q > 0) {
        take(o, o - w, cx, cz);
        if (c > 0) take(o, o - w - 1, cx, cz);
        if (c + 1 < W) take(o, o - w + 1, cx, cz);
      }
    }
  }
  for (int q = H - 1; q >= 0; q--) {
    for (int c = W - 1; c >= 0; c--) {
      const size_t o = size_t(q) * w + size_t(c);
      const double cx = x0 + c * kCell;
      const double cz = zz0 + q * kCell;
      if (c + 1 < W) take(o, o + 1, cx, cz);
      if (q + 1 < H) {
        take(o, o + w, cx, cz);
        if (c + 1 < W) take(o, o + w + 1, cx, cz);
        if (c > 0) take(o, o + w - 1, cx, cz);
      }
    }
  }
  const size_t n = size_t(cols) * size_t(rows);
  Raster out{std::vector<double>(n, kInf), std::vector<int>(n, -1), std::vector<double>(n),
             std::vector<double>(n), std::vector<double>(n)};
  for (int q = 0; q < rows; q++) {
    for (int c = 0; c < cols; c++) {
      const size_t o = size_t(q + kPad) * w + size_t(c + kPad);
      const int r = seed_run[o];
      if (r < 0) continue;
      const TrackPoint& p = (*net.runs[size_t(r)].points)[size_t(seed_idx[o])];
      const size_t k = size_t(q) * size_t(cols) + size_t(c);
      out.dist[k] = std::sqrt(d2[o]);
      out.run[k] = r;
      out.heading[k] = p.heading;
      out.width[k] = p.width;
      out.arc[k] = p.s;
    }
  }
  return out;
}

}  // namespace

std::optional<LaneRoute> route_lane(const ge::core::Heightfield& ground, const NetIndex& net,
                                    double x0, double z0, double side,
                                    const std::function<bool(int run)>& may, const LaneAsk& ask) {
  const Moves& T = moves();
  const double least = ask.least.value_or(RR::road::reach.min);
  const double top = ground.origin_z + double(ground.rows - 1) * ground.cell;
  const double z_end =
      num::min(num::min(top - 200, z0 + RR::road::reach.max), ask.box ? ask.box->z1 : kInf);
  const int rows = static_cast<int>(num::max(1.0, std::floor((z_end - z0) / kCell)));
  const double edge = ground.origin_x + double(ground.cols - 1) * ground.cell;
  const double left =
      num::max(num::max(0.0, x0 - RR::road::reach.max * 1.5), ask.box ? ask.box->x0 : 0);
  const double right =
      num::min(num::min(edge, x0 + RR::road::reach.max * 1.5), ask.box ? ask.box->x1 : kInf);
  const int cols = static_cast<int>(std::floor((right - left) / kCell)) + 1;
  const auto x_at = [&](int c) { return left + c * kCell; };
  const auto z_at = [&](int r) { return z0 + r * kCell; };
  // The band every step is held to: never so flat the grading has to cut it down to R8's least
  // fall, never past the lane's ceiling.
  const double lo = R::track::min_grade * 1.15;
  const double hi = RR::road::grade * 0.95;
  const size_t n = size_t(rows + 1) * size_t(cols);
  std::vector<double> h(n), steep(n), beside(n), run_heading(n);
  std::vector<uint8_t> at_top(n), laned(n), at_goal(n);
  std::vector<int> on_run(n, -1);
  const Raster nearest = nearest_raster(net, left, z0, cols, rows + 1);
  // Every cell is a pure reading of its own place, so the rows are read side by side.
  ge::core::parallel_bands(size_t(rows) + 1, [&](size_t band0, size_t band1) {
    for (int r = int(band0); r < int(band1); r++) {
      for (int c = 0; c < cols; c++) {
        const double x = x_at(c);
        const double z = z_at(r);
        const size_t o = size_t(r) * size_t(cols) + size_t(c);
        h[o] = sample_field(ground, x, z);
        const double gx = (sample_field(ground, x + 6, z) - sample_field(ground, x - 6, z)) / 12;
        const double gz = (sample_field(ground, x, z + 6) - sample_field(ground, x, z - 6)) / 12;
        steep[o] = hypot(gx, gz);
        const double d = nearest.dist[o];
        const double near = clearance(RR::road::width.max, nearest.width[o]) + kMargin;
        beside[o] = d < near ? 1 - d / near : 0;
        run_heading[o] = nearest.heading[o];
        at_top[o] = hypot(x - x0, z - z0) < kTopFree ? 1 : 0;
        const int r0 = nearest.run[o];
        if (d < kOnLine && may(r0) && joinable(net, r0, nearest.arc[o])) on_run[o] = r0;
        if (ask.goal && hypot(x - ask.goal->x, z - ask.goal->z) < kGoalReach) at_goal[o] = 1;
        if (ask.keep_off && ask.keep_off(x, z)) laned[o] = 1;
      }
    }
  });
  // THE LANES LAID BEFORE: a lane never crosses or runs beside another (R27).
  const int reach_lane = static_cast<int>(
      std::ceil((clearance(RR::road::width.max, RR::road::width.max) + kMargin) / kCell));
  for (const NetIndex::Run& other : net.runs) {
    if (!other.road) continue;
    for (const TrackPoint& q : *other.points) {
      const int qc = static_cast<int>(num::round((q.x - left) / kCell));
      const int qr = static_cast<int>(num::round((q.z - z0) / kCell));
      for (int r = std::max(0, qr - reach_lane); r <= std::min(rows, qr + reach_lane); r++) {
        for (int c = std::max(0, qc - reach_lane); c < std::min(cols, qc + reach_lane + 1); c++) {
          if ((c - qc) * (c - qc) + (r - qr) * (r - qr) <= reach_lane * reach_lane) {
            laned[size_t(r) * size_t(cols) + size_t(c)] = 1;
          }
        }
      }
    }
  }
  // THE SEARCH, row by row: a row's steps along it swept left to right for the ones heading +x
  // and back for the ones heading −x, then every step down pushed on to the rows below.
  std::vector<double> cost(n * M, kInf);
  std::vector<int64_t> from(n * M, -1);
  const size_t c0 = static_cast<size_t>(num::round((x0 - left) / kCell));
  for (size_t m = 0; m < M; m++) cost[c0 * M + m] = 0;
  int64_t best = -1;
  double best_score = kInf;
  const auto relax = [&](size_t o, int m0, int m, int r, int c) {
    const double base = cost[o * M + size_t(m0)];
    if (base == kInf) return;
    const Move& mv = T.moves[size_t(m)];
    const int cc = c + mv.dx;
    const int rr = r + mv.dz;
    if (cc < 0 || cc >= cols || rr > rows) return;
    const double turn = T.turn[size_t(m0) * size_t(M) + size_t(m)];
    // Off the start any way; then never tighter than the bend.
    if (o != c0 && turn > T.turn_most[size_t(m0) * size_t(M) + size_t(m)]) return;
    const size_t p = size_t(rr) * size_t(cols) + size_t(cc);
    if (laned[p]) return;
    if (beside[p] > 0 && !at_top[p] && std::abs(angle_diff(mv.heading, run_heading[p])) < kSquare)
      return;
    const double pitch = (h[o] - h[p]) / mv.length;
    if (pitch < lo * kHardMin || pitch > hi * kHardMax) return;
    const double off = pitch < lo ? lo - pitch : pitch > hi ? pitch - hi : 0;
    const double step =
        mv.length * (1 + off * kPitchCost + num::max(0.0, steep[p] - kCrossFree) * kCrossCost +
                     beside[p] * kBesideCost) +
        turn * kTurnCost;
    const size_t k = p * M + size_t(m);
    if (base + step < cost[k]) {
      cost[k] = base + step;
      from[k] = int64_t(o * M + size_t(m0));
    }
  };
  for (int r = 0; r <= rows; r++) {
    for (int c = 0; c < cols; c++) {
      const size_t o = size_t(r) * size_t(cols) + size_t(c);
      for (int m0 = 0; m0 < M; m0++) {
        if (m0 != kWest) relax(o, m0, kEast, r, c);
      }
    }
    for (int c = cols - 1; c >= 0; c--) {
      const size_t o = size_t(r) * size_t(cols) + size_t(c);
      for (int m0 = 0; m0 < M; m0++) {
        if (m0 != kEast) relax(o, m0, kWest, r, c);
      }
    }
    // A join on this row: on a piste's line, a lane's length from the start.
    for (int c = 0; c < cols; c++) {
      const size_t o = size_t(r) * size_t(cols) + size_t(c);
      if (on_run[o] < 0 && !at_goal[o]) continue;
      const double dx = x_at(c) - x0;
      const double dist = hypot(dx, z_at(r) - z0);
      if (dist < least) continue;
      for (size_t m = 0; m < M; m++) {
        const size_t k = o * M + m;
        if (cost[k] == kInf) continue;
        const double score = cost[k] + (num::sign(dx) == side ? 0 : kOtherWay * dist);
        if (score < best_score) {
          best_score = score;
          best = int64_t(k);
        }
      }
    }
    if (r == rows) break;
    // Every step costs at least its length: once nothing on this row, nor on the rows a step
    // from above already reaches, is cheaper than the best join, nothing further down can be.
    double cheapest = kInf;
    const int reach_row = std::min(rows, r + kDeepest);
    const size_t k_end = size_t(reach_row + 1) * size_t(cols) * M;
    for (size_t k = size_t(r) * size_t(cols) * M; k < k_end; k++) {
      if (cost[k] < cheapest) cheapest = cost[k];
    }
    if (cheapest >= best_score) break;
    for (int c = 0; c < cols; c++) {
      const size_t o = size_t(r) * size_t(cols) + size_t(c);
      for (int m0 = 0; m0 < M; m0++) {
        if (cost[o * M + size_t(m0)] == kInf) continue;
        for (const int m : T.down) relax(o, m0, m, r, c);
      }
    }
  }
  if (best < 0) return std::nullopt;
  LaneRoute out{};
  for (int64_t k = best; k >= 0; k = from[size_t(k)]) {
    const size_t o = size_t(k) / M;
    out.route.push_back({x_at(int(o % size_t(cols))), z_at(int(o / size_t(cols)))});
  }
  std::reverse(out.route.begin(), out.route.end());
  const PlanPoint end = out.route.back();
  const size_t o = size_t(best) / M;
  out.run = ask.goal && at_goal[o] ? -1 : on_run[o];
  out.x = end.x;
  out.z = end.z;
  return out;
}

}  // namespace ge::mountain::mapgen
