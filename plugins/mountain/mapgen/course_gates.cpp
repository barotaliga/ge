// The definitions behind course_gates.hpp.
//
// ge:summary The slalom gates' arcs, sides and offsets; the course a map is raced on
// ge:keywords course-gates choose-course
// ge:skill mountain
// ge:test tests/mountain_resort_level_test.cpp
#include "mountain/mapgen/course_gates.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "core/num.hpp"
#include "mountain/mapgen/query.hpp"
#include "mountain/mapgen/resort_rules.hpp"
#include "mountain/mapgen/rules.hpp"
#include "mountain/mapgen/spawn.hpp"

namespace ge::mountain::mapgen {

namespace num = ge::core::num;
namespace G = resort_rules::course::gates;
using ge::core::kPi;

namespace {

// How far before and after a kicker or a drop a gate stands on the line, m.
constexpr double kStraight = 20;
// The weave read either side of a gate for its pitch, m.
constexpr double kPitchReach = 30;

struct Zone {
  double from;
  double to;
};

// The arcs of every gate, start gate first and the finish last.
std::vector<double> gate_arcs_down(double length, std::span<const Zone> zones) {
  const auto in_zone = [&](double at) {
    return std::any_of(zones.begin(), zones.end(),
                       [&](const Zone& z) { return at > z.from && at < z.to; });
  };
  std::vector<double> arcs{start_gate_arc()};
  double prev = arcs[0];
  while (length - prev > G::spacing.max) {
    const double left = length - prev;
    const double near = prev + G::spacing.min;
    const double far = prev + G::spacing.max;
    // At least two gaps left, so no gate lands on the finish; the last gap may run short.
    const double gaps = num::max(2.0, num::round(left / G::spacing_target));
    double at = prev + num::min(G::spacing.max, num::max(G::spacing.min, left / gaps));
    const auto zone = std::find_if(zones.begin(), zones.end(),
                                   [&](const Zone& z) { return at > z.from && at < z.to; });
    if (zone != zones.end()) {
      // Step out of it to whichever side keeps the spacing, the nearer first.
      std::vector<double> fits;
      for (const double a : {zone->from - 1, zone->to + 1}) {
        if (a >= near && a <= far && length - a >= G::spacing.min) fits.push_back(a);
      }
      std::sort(fits.begin(), fits.end(),
                [&](double a, double b) { return std::abs(a - at) < std::abs(b - at); });
      at = fits.empty() ? zone->to + 1 : fits[0];
      // No room either side: the gate before steps back up so this one can stand past it.
      const size_t back = arcs.size() - 1;
      if (fits.empty() && back > 0 && at - prev > G::spacing.max) {
        const double moved = at - G::spacing.max;
        if (moved - arcs[back - 1] >= G::spacing.min && !in_zone(moved)) arcs[back] = moved;
      }
    }
    arcs.push_back(at);
    prev = at;
  }
  arcs.push_back(length);
  return arcs;
}

}  // namespace

double hash_pick(uint32_t seed, int i) {
  uint32_t v = static_cast<uint32_t>(num::imul(seed ^ 0x9e3779b9u, 0x85ebca6bu)) ^
               static_cast<uint32_t>(num::imul(static_cast<uint32_t>(i + 1), 0xc2b2ae35u));
  v ^= v >> 15;
  return v / 4294967296.0;
}

std::vector<Checkpoint> course_gates(const Track& track, std::span<const Cliff> drops,
                                     std::span<const Kicker> kickers, PisteGrade grade,
                                     uint32_t salt, std::span<const Kicker> park) {
  const double length = track.length;
  std::vector<Zone> zones;
  zones.reserve(drops.size() + park.size());
  for (const Cliff& d : drops) {
    const double s = d.s.value_or(0);
    zones.push_back(
        {s - d.shelf - rules::drop::gate_clear, s + d.face + d.landing + rules::drop::gate_clear});
  }
  for (const Kicker& k : park) {
    const double s = k.s.value_or(0);
    zones.push_back(
        {s - k.ramp - rules::trick::gate_before, s + k.landing + rules::trick::gate_after});
  }
  const std::vector<double> arcs = gate_arcs_down(length, zones);
  const auto pick = [&](int k, int j) {
    return hash_pick(salt + static_cast<uint32_t>(j * 7919), k);
  };
  const double first = pick(0, 1) < 0.5 ? -1 : 1;
  const auto straight = [&](double at) {
    for (const Kicker& k : kickers) {
      const double s = k.s.value_or(0);
      if (at > s - k.ramp - kStraight && at < s + k.landing + kStraight) return true;
    }
    for (const Cliff& d : drops) {
      const double s = d.s.value_or(0);
      if (at > s - d.shelf - kStraight && at < s + d.face + d.landing + kStraight) return true;
    }
    return false;
  };
  std::vector<Checkpoint> gates;
  gates.reserve(arcs.size());
  for (size_t k = 0; k < arcs.size(); k++) {
    const double at = arcs[k];
    const TrackPoint p = track_point_at(track, at);
    Checkpoint gate{p.x,
                    p.z,
                    p.y,
                    p.heading,
                    p.width + 2 * rules::checkpoint::margin,
                    p.s,
                    k % 2 == 0 ? GateColour::red : GateColour::blue};
    if (k > 0 && k + 1 < arcs.size()) {
      // A gate in a neck closes to fit it, its margins kept.
      const double width = num::min(G::width[static_cast<size_t>(grade)], p.width - 2 * G::margin);
      const double room = num::max(0.0, p.width / 2 - width / 2 - G::margin);
      // The weave between this gate and its neighbours, at the pitch here.
      const TrackPoint a = track_point_at(track, at - kPitchReach);
      const TrackPoint b = track_point_at(track, at + kPitchReach);
      const double pitch = num::max(0.0, (a.y - b.y) / (2 * kPitchReach));
      const double radius = num::max(G::radius, bend_floor(pitch) * 1.5);
      const double gap = num::min(at - arcs[k - 1], arcs[k + 1] - at);
      const double carry = (gap * gap) / (kPi * kPi * radius);
      const double share = G::share.min + (G::share.max - G::share.min) * pick(int(k), 2);
      const double side = k % 2 == 1 ? first : -first;
      const double offset = straight(at) ? 0 : side * num::min(room, carry) * share;
      gate.x = p.x + ge::core::libm::cos(p.heading) * offset;
      gate.z = p.z - ge::core::libm::sin(p.heading) * offset;
      gate.width = width;
      gate.offset = offset;
      gate.span = p.width;
    }
    gates.push_back(gate);
  }
  return gates;
}

size_t choose_course(std::span<const Course> courses, double seed,
                     const std::optional<std::string>& course, std::optional<PisteGrade> grade,
                     PisteGrade dealt) {
  if (course) {
    for (size_t i = 0; i < courses.size(); i++) {
      if (courses[i].id == *course) return i;
    }
  }
  const int want = static_cast<int>(grade.value_or(dealt));
  size_t best = 0;
  double best_score = std::numeric_limits<double>::infinity();
  for (size_t i = 0; i < courses.size(); i++) {
    const int has = static_cast<int>(courses[i].grade);
    const double score = std::abs(has - want) * 4 - (has > want ? 1 : 0) +
                         hash_pick(num::to_uint32(seed), int(i)) * 0.5;
    if (score < best_score) {
      best_score = score;
      best = i;
    }
  }
  return best;
}

}  // namespace ge::mountain::mapgen
