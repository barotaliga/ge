// engine/lab/teleport.hpp: a track that moves at its speed has no teleport; a step that jumps is
// found at its sample with the distance and what the speed allowed; the faster end of a step
// counts (a launch is not a jump); report_teleports' metric, notes and the failure on request.
//
// ge:summary Test: find_teleports flags only steps that outrun their speed; report_teleports
// ge:keywords test lab teleport find_teleports report_teleports track
// ge:skill testing labs
#include <vector>

#include "check.hpp"
#include "lab/teleport.hpp"

using namespace ge::lab;

namespace {

void nothing(Lab&) {}
const LabSpec kSpec{"teleport-test", LabKind::sim, "engine", "tests", "labs", "?", {}, "", nothing};

std::vector<TrackSample> steady() {
  std::vector<TrackSample> t;
  for (int i = 0; i <= 120; i++) t.push_back({i / 120.0, i * 10 / 120.0, 0, 0, 10});
  return t;
}

}  // namespace

TEST(a_track_at_its_speed_has_no_teleport) { CHECK(find_teleports(steady()).empty()); }

TEST(a_jump_is_found_at_its_step) {
  auto t = steady();
  for (size_t i = 60; i < t.size(); i++) t[i].z += 4;  // set 4 m across at step 60
  const auto j = find_teleports(t);
  CHECK(j.size() == 1);
  CHECK(j[0].step == 60);
  CHECK(j[0].moved > 4 && j[0].allowed < 0.2);
}

TEST(the_faster_end_counts_so_a_launch_is_not_a_jump) {
  // From rest to 30 m/s in one step: the speed after allows the distance.
  const std::vector<TrackSample> t{{0, 0, 0, 0, 0}, {0.1, 2.5, 0, 0, 30}};
  CHECK(find_teleports(t).empty());
}

TEST(the_report_is_a_metric_notes_and_a_failure_when_asked) {
  auto t = steady();
  t[30].x += 3;  // out and back: two jumps
  const auto j = find_teleports(t);
  CHECK(j.size() == 2);
  Lab lab(kSpec, {});
  report_teleports(lab, "k", j, true, 1);
  CHECK(lab.metrics().size() == 1 && lab.metrics()[0].key == "k.teleports" &&
        lab.metrics()[0].value == 2);
  CHECK(lab.notes().size() == 2);  // one jump, then "1 more"
  CHECK(lab.failures().size() == 1);
}
