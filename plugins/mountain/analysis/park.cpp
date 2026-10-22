// R20's checks behind shared.hpp: the terrain park held on the finished map, read off the
// kickers it publishes (`Kicker::trick`), never off the plan that laid them. Only a map built for a
// tricks run carries a park, so every check here is of a park that is there.
//
// ge:summary R20 checks: the park's count, each kicker built and dug, its pitch, gaps and gates
// ge:keywords analysis check-park terrain-park trick-kicker built-landing run-in r20
// ge:skill mountain
// ge:test tests/mountain_park_test.cpp
#include <algorithm>
#include <cmath>
#include <string>
#include <utility>
#include <vector>

#include "core/num.hpp"
#include "mountain/analysis/shared.hpp"
#include "mountain/mapgen/query.hpp"
#include "mountain/mapgen/rules.hpp"
#include "mountain/mapgen/spawn.hpp"

namespace ge::mountain::analysis {

namespace T = mapgen::rules::trick;
using mapgen::Kicker;

namespace {

// How much of a built landing's drop the grid may shave off its foot, a share.
constexpr double kDropKept = 0.85;
// How far a kicker may pass the pitch it was laid on once its own lift and dig are taken out.
constexpr double kPitchSlack = 0.03;

const T::Size& size_of(mapgen::TrickSize s) {
  return s == mapgen::TrickSize::low      ? T::low
         : s == mapgen::TrickSize::medium ? T::medium
                                          : T::high;
}

std::pair<double, double> span_of(const Kicker& k) {
  const double s = k.s.value_or(0);
  return {s - k.ramp, s + k.landing};
}

}  // namespace

void check_park(const mapgen::Level& level, const Add& add) {
  std::vector<const Kicker*> field;
  std::vector<const Kicker*> others;
  for (const Kicker& k : level.kickers) {
    if (!k.on_track) continue;
    (k.trick ? field : others).push_back(&k);
  }
  if (field.empty()) return;
  std::stable_sort(field.begin(), field.end(), [](const Kicker* a, const Kicker* b) {
    return a->s.value_or(0) < b->s.value_or(0);
  });
  if (!mapgen::within_band(double(field.size()), T::count)) {
    add("R20", Severity::error,
        str(double(field.size())) + " park kicker(s) (band " + band_text(T::count) + ")");
  }
  const double L = level.track.length;
  const auto ground_on = [&](double s) {
    const mapgen::TrackPoint p = mapgen::track_point_at(level.track, s);
    return level.ground_at(p.x, p.z);
  };
  for (size_t i = 0; i < field.size(); i++) {
    const Kicker& k = *field[i];
    const T::Size& z = size_of(*k.trick);
    const mapgen::KickerShape shape = k.shape.value_or(mapgen::KickerShape{-1, -1, -1});
    const bool built = std::abs(k.height - z.height) < 1e-6 &&
                       std::abs(k.ramp - z.height * z.ramp) < 1e-6 &&
                       std::abs(k.landing - (z.deck + z.fall + z.runout)) < 1e-6 &&
                       shape.deck == z.deck && shape.fall == z.fall && shape.dig == z.dig;
    const double s0 = k.s.value_or(0);
    if (!built) {
      add("R20", Severity::error,
          k.id + " is not built as a " + mapgen::trick_size_name(*k.trick) + " kicker");
    } else {
      // The landing slope is there: its foot lies at least the lip's height and the dig under
      // the lip.
      const double run = z.deck + z.fall;
      const double drop = ground_on(s0) - ground_on(s0 + run);
      const double least = kDropKept * (z.height + z.dig);
      if (drop < least) {
        add("R20", Severity::error,
            k.id + "'s landing falls " + fmt(drop) + " m (least " + fmt(least) + ")");
      }
      // On a moderate pitch, the park's own lift and dig taken out.
      const double from = s0 - k.ramp;
      const double to = s0 + run;
      const double under = (ground_on(from) - (ground_on(to) + z.dig)) / (to - from);
      if (under > T::max_grade + kPitchSlack) {
        add("R20", Severity::error, k.id + " stands on a " + fmt(under * 100, 0) + " % pitch");
      }
    }
    const auto [from, to] = span_of(k);
    if (from < mapgen::start_gate_arc() + T::lead - 1 || to > L - T::lead + 1) {
      add("R20", Severity::error,
          k.id + " stands inside the lead after the start or before the finish");
    }
    if (i + 1 < field.size()) {
      const double gap = span_of(*field[i + 1]).first - to;
      if (gap < T::gap - 1) {
        add("R20", Severity::error,
            k.id + " and " + field[i + 1]->id + " leave " + fmt(gap, 0) + " m between them");
      }
    }
    for (const Kicker* c : others) {
      const auto [a, b] = span_of(*c);
      if (from < b + T::gap - 1 && to > a - T::gap + 1) {
        add("R20", Severity::error, k.id + " crowds " + c->id);
      }
    }
    double before = -INFINITY;
    for (const mapgen::Checkpoint& g : level.checkpoints) {
      if (g.s > from - T::gate_before + 1 && g.s < to + T::gate_after - 1) {
        add("R20", Severity::error,
            k.id + " stands on or hard by the gate at s " + fmt(g.s, 0) + " m");
      }
      if (g.s <= from - T::gate_before + 1 && g.s > before) before = g.s;
    }
    if (std::isfinite(before)) {
      const double fall = ground_on(before) - ground_on(from);
      if (fall < T::run_in * k.height - 0.3) {
        add("R20", Severity::error,
            k.id + "'s run-in from the gate at s " + fmt(before, 0) + " m falls only " + fmt(fall) +
                " m");
      }
    }
  }
}

}  // namespace ge::mountain::analysis
