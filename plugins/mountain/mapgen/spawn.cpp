// Port of game5 `engine/mapgen/spawn.ts`.
//
// ge:summary Chooses the start off the summit and lays the gates and the grid on the piste
// ge:keywords spawn chooseStart gridOnTrack layCheckpoints
// ge:oracle game5/engine/mapgen/spawn.ts
// ge:skill mountain
// ge:test tests/mountain_generate_test.cpp
#include "mountain/mapgen/spawn.hpp"

#include <array>
#include <limits>

#include "core/js.hpp"
#include "core/libm/libm.hpp"
#include "mountain/mapgen/query.hpp"

namespace ge::mountain::mapgen {

namespace js = ge::core::js;
namespace libm = ge::core::libm;
namespace R = rules;
using ge::core::sample_field;

namespace {

// The headings tried out of the start, rad off the fall line, in order.
constexpr std::array<double, 5> kStartHeadings{0, 0.35, 0.7, 1.0, 1.2};

// How many places under the ridge an attempt tries.
constexpr int kStartTries = 4;

// The most the untouched ground may climb over a window of the start's run, m per m.
constexpr double kStartClimb = 0.08;

// How far over a grade's floor under the start the ground must fall (R23).
constexpr double kStartFloor = 1.15;

}  // namespace

Fall fall_along(const ge::core::Heightfield& ground, double x, double z, double heading,
                double run) {
  const double w = R::track::grade_window;
  const double fx = libm::sin(heading);
  const double fz = libm::cos(heading);
  double steepest = -std::numeric_limits<double>::infinity();
  double gentlest = std::numeric_limits<double>::infinity();
  for (double d = 0; d + w <= run + 1e-9; d += w / 2) {
    const double a = sample_field(ground, x + fx * d, z + fz * d);
    const double b = sample_field(ground, x + fx * (d + w), z + fz * (d + w));
    const double fall = (a - b) / w;
    if (fall > steepest) steepest = fall;
    if (fall < gentlest) gentlest = fall;
  }
  return {steepest, gentlest};
}

std::variant<Spawn, std::string> choose_start(ge::core::Rng& rng,
                                              const ge::core::Heightfield& ground,
                                              const GradeRow& grade) {
  const double size = R::world::size;
  const double z = size * R::track::start_z;
  const double run = start_gate_arc() + R::spawn::run;
  const double max_slope = grade.spawn.max_slope;
  const double min_slope = grade.spawn.min_slope;
  for (int attempt = 0; attempt < kStartTries; attempt++) {
    const double x = size * in_band(rng, R::track::start_x);
    const double side = ge::core::chance(rng, 0.5) ? 1 : -1;
    for (const double a : kStartHeadings) {
      const std::array<double, 2> signs{a == 0 ? 1.0 : side, -side};
      for (size_t k = 0; k < (a == 0 ? 1u : 2u); k++) {
        const double heading = a * signs[k];
        const Fall fall = fall_along(ground, x, z, heading, run);
        if (fall.steepest > max_slope * 0.9 || fall.gentlest < -kStartClimb) continue;
        if (min_slope > 0) {
          const double end =
              sample_field(ground, x + libm::sin(heading) * run, z + libm::cos(heading) * run);
          if ((sample_field(ground, x, z) - end) / run < min_slope * kStartFloor) continue;
        }
        return Spawn{x, z, heading};
      }
    }
  }
  return std::string("no heading off the summit makes a fair start");
}

void grid_on_track(const Track& track, Spawn& spawn, std::vector<Spawn>& grid) {
  const TrackPoint p = track_point_at(track, 0);
  spawn = Spawn{p.x, p.z, p.heading};
  grid.clear();
  const double rx = libm::cos(p.heading);
  const double rz = -libm::sin(p.heading);
  for (int slot = 0; slot < R::grid::slots; slot++) {
    const int col = slot % R::grid::abreast;
    const double o = (col - (R::grid::abreast - 1) / 2.0) * R::grid::spacing;
    grid.push_back(Spawn{p.x + rx * o, p.z + rz * o, p.heading});
  }
}

std::vector<double> gate_arcs(double length) {
  const double s0 = start_gate_arc();
  const double count = js::max(2.0, js::round((length - s0) / R::checkpoint::spacing_target));
  const double spacing = (length - s0) / count;
  std::vector<double> out;
  for (int k = 0; k <= int(count); k++) out.push_back(k == count ? length : s0 + k * spacing);
  return out;
}

std::vector<Checkpoint> lay_checkpoints(const Track& track) {
  std::vector<Checkpoint> out;
  const std::vector<double> arcs = gate_arcs(track.length);
  for (size_t k = 0; k < arcs.size(); k++) {
    const TrackPoint p = track_point_at(track, arcs[k]);
    out.push_back(Checkpoint{p.x, p.z, p.y, p.heading, p.width + 2 * R::checkpoint::margin, p.s,
                             k % 2 == 0 ? GateColour::red : GateColour::blue});
  }
  return out;
}

}  // namespace ge::mountain::mapgen
