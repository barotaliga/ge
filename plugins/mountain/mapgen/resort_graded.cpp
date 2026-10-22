// The ski-area build's fourth stage: every walked run graded and pressed, in the order walked.
//
// ge:summary Stage 4 of the ski-area build: runs graded, featured and pressed in order, renumbered
// ge:keywords resort-graded grade press drops kickers drifts keep-off junction measured-colour
// ge:skill mountain
// ge:test tests/mountain_resort_build_test.cpp
#include <algorithm>
#include <vector>

#include "core/heightfield.hpp"
#include "core/num.hpp"
#include "core/num_text.hpp"
#include "core/output.hpp"
#include "mountain/mapgen/drift.hpp"
#include "mountain/mapgen/drops.hpp"
#include "mountain/mapgen/grades.hpp"
#include "mountain/mapgen/kickers.hpp"
#include "mountain/mapgen/network_build.hpp"
#include "mountain/mapgen/resort_build.hpp"
#include "mountain/mapgen/resort_rules.hpp"

namespace ge::mountain::mapgen {

namespace num = ge::core::num;
namespace RR = resort_rules;
using ge::core::sample_field;

namespace {

// The salt each run's own stream (its drops, its drifts) is dealt off the attempt's by.
constexpr uint32_t kRunSalt = 0x2b1d5e7;
// How near a run's line a pad's ground counts as the run's when the pads are pressed again, m.
constexpr double kPadLine = 8;

// The colour ceiling a run's grading holds its colour windows under: its row's, but for a black
// (none) and a lane (none: its own grade is lower).
std::optional<double> colour_cap(const WalkedRun& run) {
  const GradeRow& row = *run.spec.row;
  if (run.spec.kind == RunKind::road || !row.id || *row.id == PisteGrade::black) {
    return std::nullopt;
  }
  return row.steepest.max;
}

}  // namespace

StageResult resort_graded(ResortAttempt& a) {
  ge::core::Heightfield& ground = a.ground;
  std::deque<WalkedRun>& walked = a.walked;
  const size_t n = walked.size();
  a.packed = ge::core::create_heightfield(0, 0, ground.cell, ground.cols, ground.rows);
  a.stamp = network_stamp(ground);
  const PinnedAt pressed = [&](double x, double z) { return on_core(a.stamp, ground, x, z); };
  // Where each run is merged into, by arc: no feature is laid across a junction.
  std::vector<std::vector<double>> junctions(n);
  for (const WalkedRun& r : walked) {
    if (r.into) junctions[size_t(r.into->run)].push_back(r.into->s);
  }
  std::vector<bool> dropped(n, false);
  struct Laid {
    size_t i;
    std::vector<Kicker> kickers;
    std::vector<Cliff> drops;
    std::vector<Drift> drifts;
  };
  std::vector<Laid> laid;
  for (size_t i = 0; i < n; i++) {
    WalkedRun& w = walked[i];
    const bool road = w.spec.kind == RunKind::road;
    const GradeRow& row = road ? kRoadRow : *w.spec.row;
    WalkedRun* onto = w.into ? &walked[size_t(w.into->run)] : nullptr;
    if (onto) funnel_into(w, *onto);
    const auto grade = [&] {
      return grade_run(w, ground, row.track.max_grade, colour_cap(w), onto, &pressed, false,
                       grade_face(a));
    };
    std::optional<std::string> why;
    if (a.access.is_lost(i)) {
      why = "no lift or lane brings a skier back to its top without a harder run (R29)";
    } else if (w.into && dropped[size_t(w.into->run)]) {
      why = "the run it merges into was left out";
    } else if (w.spec.branch && dropped[size_t(w.spec.branch->run)]) {
      why = "the run it leaves was left out";
    } else {
      why = grade();
    }
    // A run walked as gentler than its colour that grades out its colour on the pressed
    // mountain starts as its colour again (R12).
    if (!why && w.start_slope && !road) {
      const PisteGrade measured = run_colour(w.track.points, onto ? &onto->track.points : nullptr);
      if (int(measured) >= int(w.spec.row->id.value_or(PisteGrade::black))) {
        w.start_slope.reset();
        why = grade();
      }
    }
    if (why) {
      ge::core::output::debug("resort " + num::to_string(a.seed) + "#" + std::to_string(a.attempt) +
                              ": run " + w.spec.id + " left out — " + *why);
      dropped[i] = true;
      continue;
    }
    // Every feature keeps off the junctions down it and the stretch where it merges.
    constexpr double J = RR::network::junction;
    std::vector<ArcSpan> keep_off;
    keep_off.reserve(junctions[i].size() + 1);
    for (const double s : junctions[i]) keep_off.push_back({s - J, s + J});
    if (w.into) keep_off.push_back({w.merge_from - J, w.track.length + J});
    const uint32_t run_sub = a.sub ^ static_cast<uint32_t>(num::imul(uint32_t(i + 1), kRunSalt));
    const GradeRow scaled = scaled_row(row, w.track.length);
    // Drops only across a run that grades out black (R24): a black-built run the mountain made
    // gentler is no place for one.
    const bool black = !road && run_colour(w.track.points, onto ? &onto->track.points : nullptr) ==
                                    PisteGrade::black;
    std::vector<TrackDrop> track_drops;
    if (black) track_drops = lay_drops(run_sub, w.track, scaled, keep_off);
    std::vector<TrackKicker> track_kickers;
    if (!road) track_kickers = lay_track_kickers(a.rng, w.track, scaled, track_drops, keep_off);
    Laid l{
        i, publish_track_kickers(w.track, track_kickers), publish_drops(w.track, track_drops), {}};
    if (!road) {
      l.drifts = deal_drifts(run_sub, w.track.length, l.kickers, row.drift, l.drops, keep_off);
    }
    stamp_run(w, ground, a.packed, a.stamp, l.drifts, onto);
    laid.push_back(std::move(l));
  }
  // The station pads at their levels again, over the runs pressed off them (R26).
  NetHit hit;
  relevel_pads(ground, a.pads, [&](double x, double z) {
    return a.walking.nearest(
                        x, z, kPadLine, [](int, double) { return false; }, hit)
               .distance < kPadLine;
  });
  // The runs that stand, numbered among themselves: a merge names the run it joins by its place.
  std::vector<int> place(n, -1);
  int placed = 0;
  for (size_t i = 0; i < n; i++) {
    if (!dropped[i]) place[i] = placed++;
  }
  a.dropped = static_cast<int>(n) - placed;
  a.net = NetIndex();
  a.built.clear();
  for (Laid& l : laid) {
    a.built.push_back({std::move(walked[l.i]), PisteGrade::green, std::move(l.kickers),
                       std::move(l.drops), std::move(l.drifts)});
  }
  a.walking = NetIndex();
  walked.clear();
  // Every run's line as the ground carries it once every run is pressed, and the colour it
  // MEASURES there (R23, R27); a lane's is green.
  int graded = 0;
  for (BuiltRun& b : a.built) {
    WalkedRun& w = b.walked;
    if (w.into) w.into->run = place[size_t(w.into->run)];
    if (w.spec.branch) w.spec.branch->run = place[size_t(w.spec.branch->run)];
    for (TrackPoint& p : w.track.points) p.y = sample_field(ground, p.x, p.z);
    for (Kicker& k : b.kickers) k.y = sample_field(ground, k.x, k.z);
    for (Cliff& d : b.drops) d.y = sample_field(ground, d.x, d.z);
  }
  for (BuiltRun& b : a.built) {
    const WalkedRun& w = b.walked;
    if (w.spec.kind != RunKind::piste) continue;
    const WalkedRun* onto = w.into ? &a.built[size_t(w.into->run)].walked : nullptr;
    b.grade = run_colour(w.track.points, onto ? &onto->track.points : nullptr);
    graded++;
  }
  if (graded < (a.face ? RR::massif::real::least::runs : RR::network::runs_min)) {
    return "only " + std::to_string(graded) + " piste(s) could be graded into the mountain";
  }
  for (const BuiltRun& b : a.built) {
    const WalkedRun& w = b.walked;
    a.net.add(w.track.points, rank_of(w.spec), w.into ? w.into->run : -1, w.merge_start,
              w.spec.kind == RunKind::road);
  }
  return std::nullopt;
}

}  // namespace ge::mountain::mapgen
