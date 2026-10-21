// The room a ramp off a top needs, and what keeps off it; the ramps laid, pressed and groomed.
//
// ge:summary The ramps off a top: their room, then each planned off the rim, pressed and groomed
// ge:keywords summit-ramps ramp-room room-blocked near-room in-approach off-line plan-ramps
// ge:keywords lay-ramps press-ramps groom-ramps head-index ramp-to each-cell
// ge:skill mountain
// ge:test tests/mountain_resort_build_test.cpp
#include "mountain/mapgen/summit_ramps.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <unordered_map>

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "core/num.hpp"

namespace ge::mountain::mapgen {

namespace num = ge::core::num;
namespace libm = ge::core::libm;
namespace RR = resort_rules;
namespace K = RR::lift::top::ramp;
using ge::core::hypot;
using ge::core::sample_field;
using ge::core::smoothstep;

namespace {

// How much higher than its top's middle a ramp's head on the rim may stand, m.
constexpr double kRimRise = 0.2;
// How many points round the pad's rim a ramp is tried from.
constexpr int kRim = 36;
// The step a ramp's line is read at, m.
constexpr double kLineStep = 1.5;
// How much further than the pressing a ramp's line keeps off another run's ground, m: a couple
// of cells, so no cell under it is left.
constexpr double kLineSpare = 4;
// Every how many of a run's points a ramp is aimed at.
constexpr size_t kAimEvery = 4;
// How far a ramp keeps off any lift's bottom station (its footprint) and off the first point of
// every run (its start and its windrows), m.
constexpr double kStationKeep = 14;
// A run's own ground past its edge (the flat shoulder and the windrow's bench) that a ramp leaves
// as the run pressed it, m.
constexpr double kShoulder = kBench + 1;
// How far a ramp's line keeps off a lift's line, m (under one a ramp only ever cuts, out to
// `kLineCut`); and how far past the approach cut's own half-width it keeps off the way in to a
// top, m (its head, on the rim, a little further).
constexpr double kLineKeep = 9;
constexpr double kLineCut = kLineKeep;
constexpr double kApproachKeep = 5;
constexpr double kApproachHead = RR::lift::top::approach::blend + 2;
// How much more than the glide a start is asked to lie under a ramp's head, the share of the
// steepest it may fall at, and the step its line is read at (each a real face's where the ramps
// are chosen together).
constexpr double kRoomSpare = 1;
constexpr double kRoomSteep = 0.7;
constexpr double kJointSpare = 1.5;
constexpr double kJointSteep = 0.6;
constexpr double kRoomStep = 3;

// The steepest a ramp down to a run of `row`'s colour falls overall.
double steepest(const GradeRow& row, const RampFit& fit) {
  return row.steepest.max <= grade_row(PisteGrade::blue).steepest.max ? fit.gentle : K::steep;
}

// Whether (x, z) is too near the ground R26's approach cut away under the way in to a pad's top.
bool in_approach(std::span<const StationPad> pads, double x, double z, double keep) {
  namespace A = RR::lift::top::approach;
  for (const StationPad& p : pads) {
    if (!padded(p.kind)) continue;
    const bool gondola = p.kind == LiftKind::gondola;
    const double from = gondola ? A::from.gondola : A::from.chair;
    const double half = gondola ? A::half.gondola : A::half.chair;
    const double back = (p.x - x) * p.dx + (p.z - z) * p.dz;
    if (back < from - A::ease || back > num::min(A::length, p.length / 2)) continue;
    const double v = std::abs((x - p.x) * p.dz - (z - p.z) * p.dx);
    if (v < half + keep) return true;
  }
  return false;
}

}  // namespace

double off_line(const LiftPlan& l, double x, double z) {
  const double ex = l.top.x - l.bottom.x;
  const double ez = l.top.z - l.bottom.z;
  const double len2 = ex * ex + ez * ez;
  const double k = num::max(
      0.0, num::min(1.0, ((x - l.bottom.x) * ex + (z - l.bottom.z) * ez) / (len2 == 0 ? 1 : len2)));
  return hypot(x - (l.bottom.x + ex * k), z - (l.bottom.z + ez * k));
}

double near_room(const RampRoom& r, double x, double z) {
  const double ex = r.to.x - r.from.x;
  const double ez = r.to.z - r.from.z;
  const double len2 = ex * ex + ez * ez;
  const double k = num::max(
      0.0, num::min(1.0, ((x - r.from.x) * ex + (z - r.from.z) * ez) / (len2 == 0 ? 1 : len2)));
  return hypot(x - (r.from.x + ex * k), z - (r.from.z + ez * k)) -
         r.half.value_or(K::width / 2 + K::blend);
}

std::optional<RampRoom> ramp_room(const StationPad& p, std::span<const StationPad> pads,
                                  std::span<const LiftPlan> lifts, double x, double z, double half,
                                  const GradeRow& row,
                                  const std::function<double(double, double)>& height,
                                  const BlockedAt& blocked, const RampFit& fit) {
  const double step = fit.joint ? kLineStep : kRoomStep;
  for (int a = 0; a < kRim; a++) {
    const double ang = ((double(a) / kRim) * ge::core::kPi) * 2;
    const double fx = p.x + libm::sin(ang) * p.r;
    const double fz = p.z + libm::cos(ang) * p.r;
    if ((x - fx) * (fx - p.x) + (z - fz) * (fz - p.z) <= 0) continue;
    if (in_approach(pads, fx, fz, kApproachHead)) continue;
    if (fit.joint && height(fx, fz) > p.y + kRimRise) continue;
    const double d = hypot(x - fx, z - fz);
    const double length = d - half;
    if (length < K::least) continue;
    // Down to the run's edge on its side: falling enough to the ground there, and not too
    // steeply to its line's.
    const double rim = height(fx, fz);
    const double edge = height(fx + ((x - fx) * length) / d, fz + ((z - fz) * length) / d);
    const double spare = fit.joint ? kJointSpare : kRoomSpare;
    if ((rim - edge) / length < fit.glide * spare) continue;
    const double steep = fit.joint ? kJointSteep : kRoomSteep;
    if ((rim - height(x, z)) / length > steepest(row, fit) * steep) continue;
    bool clear = true;
    const double reach = fit.joint ? d : length;
    double u = 0;
    while (clear && u <= reach) {
      const double k = u / (length + half);
      const double px = fx + (x - fx) * k;
      const double pz = fz + (z - fz) * k;
      clear = !blocked(px, pz) && !in_approach(pads, px, pz, kApproachKeep) &&
              std::none_of(lifts.begin(), lifts.end(),
                           [&](const LiftPlan& l) { return off_line(l, px, pz) < kLineKeep; });
      u += step;
    }
    if (clear) return RampRoom{{fx, fz}, {x, z}};
  }
  return std::nullopt;
}

BlockedAt room_blocked(const StationPad* pad, std::span<const StationPad> pads,
                       std::span<const PlanPoint> stations, std::span<const RampRoom> rooms,
                       double keep_pad, double keep_station, BlockedAt on_run) {
  return [=, on_run = std::move(on_run)](double x, double z) {
    for (const RampRoom& r : rooms) {
      if (near_room(r, x, z) < K::width / 2) return true;
    }
    if (on_run(x, z)) return true;
    for (const StationPad& p : pads) {
      if (&p != pad && hypot(x - p.x, z - p.z) < p.r + keep_pad) return true;
    }
    for (const PlanPoint& s : stations) {
      if (!(pad && hypot(s.x - pad->x, s.z - pad->z) < 1) && hypot(x - s.x, z - s.z) < keep_station)
        return true;
    }
    return false;
  };
}

namespace {

// How far down a ramp's length its height has come, 0..1: eased off the pad over the first
// `ease` of it, even from there on.
double even_fall(double t) {
  const double a = K::ease;
  const double u = num::max(0.0, num::min(1.0, t));
  const double ramp = u < a ? (u * u) / (2 * a) : u - a / 2;
  return ramp / (1 - a / 2);
}

// Whether `hypot(dx, dz) < r`: a point a whole `r` off along either axis never is, so the box is
// asked first and the hypot only inside it.
bool within(double dx, double dz, double r) {
  return std::abs(dx) < r && std::abs(dz) < r && hypot(dx, dz) < r;
}

// Every run's head point (its first `ramp.head` m), hashed by where it stands, and the one
// question a ramp's line asks of them: whether any head but `own`'s reaches (x, z), its
// half-width and `past` m beyond.
class HeadIndex {
 public:
  explicit HeadIndex(std::span<const RampRun> runs) {
    for (const RampRun& r : runs) {
      for (const TrackPoint& q : *r.points) {
        if (q.s > K::head) continue;
        cells_[key(cell_of(q.x), cell_of(q.z))].push_back({q.x, q.z, q.width, &r.spec->id});
        widest_ = num::max(widest_, q.width / 2);
      }
    }
  }

  bool near(double x, double z, double past, const std::string& own) const {
    const int reach = static_cast<int>(std::ceil((widest_ + past) / kCell));
    const int qc = cell_of(x);
    const int qr = cell_of(z);
    for (int c = qc - reach; c <= qc + reach; c++) {
      for (int r = qr - reach; r <= qr + reach; r++) {
        const auto it = cells_.find(key(c, r));
        if (it == cells_.end()) continue;
        for (const Head& h : it->second) {
          if (*h.run != own && within(x - h.x, z - h.z, h.width / 2 + past)) return true;
        }
      }
    }
    return false;
  }

 private:
  static constexpr double kCell = 32;
  struct Head {
    double x, z, width;
    const std::string* run;
  };
  static int cell_of(double v) { return static_cast<int>(std::floor(v / kCell)); }
  static int64_t key(int c, int r) { return int64_t(c) * 65536 + int64_t(r); }
  std::unordered_map<int64_t, std::vector<Head>> cells_;
  double widest_ = 0;
};

// Whether a point stands where a ramp's line to run `run` may not pass.
using StationAt = std::function<bool(double x, double z, const std::string& run)>;

// The ramp from (fx, fz) on a pad's rim toward the run's point `q`, ended where the line first
// meets the run's snow, at that snow's height (through its own shoulder, which it is pressed
// over); none where the line crosses another run's ground or a station first, or never meets it.
std::optional<SummitRamp> ramp_to(const RampRun& run, int own, double fx, double fz,
                                  const TrackPoint& q, const HeightAt& height,
                                  const RunCover& cover, const StationAt& station) {
  const double len = hypot(q.x - fx, q.z - fz);
  const double ux = (q.x - fx) / len;
  const double uz = (q.z - fz) / len;
  for (int k = 0; k * kLineStep <= len; k++) {
    const double u = k * kLineStep;
    const double x = fx + ux * u;
    const double z = fz + uz * u;
    if (station(x, z, run.spec->id)) return std::nullopt;
    const int near = cover(x, z, kShoulder + kLineSpare);
    if (near >= 0 && near != own) return std::nullopt;
    // No run's ground out past its shoulder here is no run's snow either.
    if (near < 0 || cover(x, z, 0) != own) continue;
    double best = std::numeric_limits<double>::infinity();
    double s = 0;
    for (const TrackPoint& a : *run.points) {
      const double e = hypot(a.x - x, a.z - z);
      if (e < best) {
        best = e;
        s = a.s;
      }
    }
    return SummitRamp{run.spec->id, {fx, height(fx, fz), fz}, {x, height(x, z), z, s}, K::width};
  }
  return std::nullopt;
}

// Whether two ramps' corridors would press the same ground: one's line comes within both their
// half-widths and the blend of the other's.
bool overlaps(const SummitRamp& a, const SummitRamp& b) {
  const double room = (a.width + b.width) / 2 + K::blend;
  const double len = hypot(b.to.x - b.from.x, b.to.z - b.from.z);
  for (int i = 0; i * 3.0 <= len; i++) {
    const double k = (i * 3.0) / len;
    const double x = b.from.x + (b.to.x - b.from.x) * k;
    const double z = b.from.z + (b.to.z - b.from.z) * k;
    const RampAt f = ramp_frame(a, x, z);
    const double past = f.t < 0 ? -f.t * f.length : f.t > 1 ? (f.t - 1) * f.length : 0;
    if (hypot(past, f.d) < room) return true;
  }
  return false;
}

struct Candidate {
  SummitRamp ramp;
  double length;
  double grade;
};

// Whether ramp `a` is to be laid before `b`: one falling within `grade` before one steeper, the
// shortest of those, the gentlest of the rest.
bool better_ramp(const Candidate& a, const Candidate& b) {
  const bool fits = a.grade <= K::grade;
  if (fits != (b.grade <= K::grade)) return fits;
  return fits ? a.length < b.length : a.grade < b.grade;
}

// The ramps off every pad: to each run leaving its top joined below its deck near enough, the
// best line off the rim (`better_ramp`) that falls at least a glide and no steeper than the run's
// colour, off the ramps already laid off that pad.
RampsByLift plan_ramps(std::span<const StationPad> pads, std::span<const RampRun> runs,
                       const HeightAt& height, const RunCover& cover, const StationAt& station,
                       const BlockedAt& head, const RampFit& fit) {
  RampsByLift out;
  for (const StationPad& p : pads) {
    std::vector<SummitRamp> ramps;
    for (size_t own = 0; own < runs.size(); own++) {
      const RampRun& r = runs[own];
      if (r.spec->from != p.lift) continue;
      std::optional<Candidate> best;
      const std::vector<TrackPoint>& pts = *r.points;
      for (size_t k = 0; k < pts.size(); k += kAimEvery) {
        const TrackPoint& q = pts[k];
        if (q.y > p.y - K::drop) continue;
        const double d = hypot(q.x - p.x, q.z - p.z);
        if (d > K::far || d < p.r + K::least) continue;
        for (int a = 0; a < kRim; a++) {
          const double ang = ((double(a) / kRim) * ge::core::kPi) * 2;
          const double fx = p.x + libm::sin(ang) * p.r;
          const double fz = p.z + libm::cos(ang) * p.r;
          // Out off the rim, never back across the pad, from a head clear and no higher than
          // the middle a rider is let go on.
          if ((q.x - fx) * (fx - p.x) + (q.z - fz) * (fz - p.z) <= 0 || head(fx, fz)) continue;
          if (height(fx, fz) > p.y + kRimRise) continue;
          std::optional<SummitRamp> ramp =
              ramp_to(r, static_cast<int>(own), fx, fz, q, height, cover, station);
          if (!ramp) continue;
          if (std::any_of(ramps.begin(), ramps.end(),
                          [&](const SummitRamp& o) { return overlaps(o, *ramp); })) {
            continue;
          }
          const double length = hypot(ramp->to.x - ramp->from.x, ramp->to.z - ramp->from.z);
          if (length < K::least) continue;
          const double grade = (ramp->from.y - ramp->to.y) / length;
          // Steeper than its colour all the way down is no ramp, and nor is one a rider let go
          // at a crawl would not glide down.
          if (grade > steepest(*r.spec->row, fit) || grade < fit.glide) continue;
          Candidate found{std::move(*ramp), length, grade};
          if (!best || better_ramp(found, *best)) best = std::move(found);
        }
      }
      if (best) ramps.push_back(std::move(best->ramp));
    }
    if (!ramps.empty()) out[p.lift] = std::move(ramps);
  }
  return out;
}

// Every cell of `field` a ramp's corridor reaches, with its weight and its share along the ramp:
// whole across the ramp, eased out over `blend` beside it and past its foot; never inside its
// pad (the pad's own), on a run's ground (`on_run`) or where `keep` says.
template <class OnRunFn, class KeepFn, class Visit>
void each_cell(const ge::core::Heightfield& field, const SummitRamp& ramp, const StationPad& pad,
               OnRunFn&& on_run, KeepFn&& keep, Visit&& visit) {
  const double reach = ramp.width / 2 + K::blend;
  const double cell = field.cell;
  const double x0 = num::min(ramp.from.x, ramp.to.x);
  const double x1 = num::max(ramp.from.x, ramp.to.x);
  const double z0 = num::min(ramp.from.z, ramp.to.z);
  const double z1 = num::max(ramp.from.z, ramp.to.z);
  const int c0 = static_cast<int>(num::max(0.0, std::floor((x0 - reach - field.origin_x) / cell)));
  const int c1 = static_cast<int>(
      num::min(double(field.cols - 1), std::ceil((x1 + reach - field.origin_x) / cell)));
  const int r0 = static_cast<int>(num::max(0.0, std::floor((z0 - reach - field.origin_z) / cell)));
  const int r1 = static_cast<int>(
      num::min(double(field.rows - 1), std::ceil((z1 + reach - field.origin_z) / cell)));
  for (int row = r0; row <= r1; row++) {
    for (int col = c0; col <= c1; col++) {
      const double x = field.origin_x + col * cell;
      const double z = field.origin_z + row * cell;
      const RampAt f = ramp_frame(ramp, x, z);
      if (f.t < 0 || hypot(x - pad.x, z - pad.z) < pad.r) continue;
      const double past = num::max(0.0, (f.t - 1) * f.length);
      const double w =
          (1 - smoothstep(ramp.width / 2, reach, f.d)) * (1 - smoothstep(0, K::blend, past));
      if (w <= 0 || on_run(x, z) || keep(x, z)) continue;
      visit(static_cast<size_t>(row) * static_cast<size_t>(field.cols) + size_t(col), x, z, w, f.t);
    }
  }
}

// Whether (x, z) is a run's snow, or another run's than `own` out to `past` m past its edge:
// ground a ramp to `own` leaves as it lies.
bool off_own(const RunCover& cover, double x, double z, int own, double past) {
  if (cover(x, z, 0) >= 0) return true;
  const int near = cover(x, z, past);
  return near >= 0 && near != own;
}

// The place of the run with id `id` among `runs`, or -1.
int place_of(std::span<const RampRun> runs, const std::string& id) {
  for (size_t i = 0; i < runs.size(); i++) {
    if (runs[i].spec->id == id) return static_cast<int>(i);
  }
  return -1;
}

}  // namespace

RampAt ramp_frame(const SummitRamp& r, double x, double z) {
  const double ex = r.to.x - r.from.x;
  const double ez = r.to.z - r.from.z;
  const double length = num::max(1.0, hypot(ex, ez));
  const double along = ((x - r.from.x) * ex + (z - r.from.z) * ez) / length;
  const double d = std::abs((x - r.from.x) * ez - (z - r.from.z) * ex) / length;
  return {along / length, d, length};
}

double ramp_height(const SummitRamp& r, double t) {
  return r.from.y - (r.from.y - r.to.y) * even_fall(t);
}

double off_ramp(const SummitRamp& r, double x, double z) {
  const RampAt f = ramp_frame(r, x, z);
  const double past = f.t < 0 ? -f.t * f.length : f.t > 1 ? (f.t - 1) * f.length : 0;
  return hypot(past, num::max(0.0, f.d - r.width / 2)) - K::blend;
}

RampsByLift lay_ramps(ge::core::Heightfield& ground, std::span<const StationPad> pads,
                      std::span<const RampRun> runs, const RunCover& cover,
                      std::span<const LiftPlan> lifts,
                      const std::function<double(double x, double z)>& way, const RampFit& fit) {
  // A station's footprint, every run's head but the one a ramp makes for, and the way to the
  // next lift's queue (`way`: how far off its ground).
  const HeadIndex heads(runs);
  const auto station = [&](double x, double z, double more, const std::string& own) {
    for (const LiftPlan& l : lifts) {
      if (within(x - l.bottom.x, z - l.bottom.z, kStationKeep + more)) return true;
    }
    return heads.near(x, z, kShoulder + more, own) || (way && way(x, z) < more);
  };
  // The line is kept a couple of cells further off than the pressing is, so no cell it is read
  // over is one left unpressed.
  const double margin = ground.cell * 2;
  const HeightAt height = [&](double x, double z) { return sample_field(ground, x, z); };
  // Never down under a lift's line, among the towers, but beside the track of the drag it leaves
  // the top of, which its rider was just pulled up on the snow.
  const auto from_of = [&](const std::string& run) -> const std::string* {
    const int i = place_of(runs, run);
    return i < 0 ? nullptr : &runs[size_t(i)].spec->from;
  };
  const auto under = [&](double x, double z, const std::string& run, double keep) {
    const std::string* from = from_of(run);
    for (const LiftPlan& l : lifts) {
      if (l.kind == LiftKind::drag && from && l.id == *from) continue;
      if (off_line(l, x, z) < keep) return true;
    }
    return false;
  };
  const StationAt blocked = [&](double x, double z, const std::string& run) {
    return station(x, z, margin, run) || under(x, z, run, kLineKeep) ||
           in_approach(pads, x, z, kApproachKeep) || (way && way(x, z) < K::width / 2 + margin);
  };
  const BlockedAt head = [&](double x, double z) { return in_approach(pads, x, z, kApproachHead); };
  RampsByLift ramps = plan_ramps(pads, runs, height, cover, blocked, head, fit);
  // Pressed off a run's snow and every other run's ground, a station's, and under a lift's line
  // only ever cut, never up into the rope.
  for (const StationPad& pad : pads) {
    const auto it = ramps.find(pad.lift);
    if (it == ramps.end()) continue;
    for (const SummitRamp& ramp : it->second) {
      const int own = place_of(runs, ramp.run);
      each_cell(
          ground, ramp, pad,
          [&](double x, double z) { return off_own(cover, x, z, own, kShoulder); },
          [&](double x, double z) { return station(x, z, 0, ramp.run); },
          [&](size_t i, double x, double z, double w, double t) {
            const double y = double(ground.data[i]);
            const double h = y * (1 - w) + ramp_height(ramp, t) * w;
            if (h > y && under(x, z, ramp.run, kLineCut)) return;
            ground.data[i] = static_cast<float>(h);
          });
    }
  }
  return ramps;
}

void groom_ramps(ge::core::Heightfield& packed, const RampsByLift& ramps,
                 std::span<const StationPad> pads, std::span<const RampRun> runs,
                 const RunCover& cover, double past) {
  for (const StationPad& pad : pads) {
    const auto it = ramps.find(pad.lift);
    if (it == ramps.end()) continue;
    for (const SummitRamp& ramp : it->second) {
      const int own = place_of(runs, ramp.run);
      each_cell(
          packed, ramp, pad, [&](double x, double z) { return off_own(cover, x, z, own, past); },
          [](double, double) { return false; },
          [&](size_t i, double, double, double w, double) {
            if (w > double(packed.data[i])) packed.data[i] = static_cast<float>(w);
          });
    }
  }
}

}  // namespace ge::mountain::mapgen
