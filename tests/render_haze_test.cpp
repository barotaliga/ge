// engine/render/haze.hpp: the presets are named and found, a rocket's wake wider, stronger and
// longer-lived than a nail's thread; a stretch's heat is 0 before it is laid, rises in its first
// moments, falls to nothing at its life's end and is a pure function of the moment; the staging is
// camera-relative, widens and lifts a stretch as it ages, drops what has cooled and keeps the
// newest when there are too many.
//
// ge:summary Tests for heat haze: presets, a stretch's heat over its life, the staging
// ge:keywords test render haze heat-haze trail nail rocket level staging
// ge:skill render testing
#include <vector>

#include "check.hpp"
#include "render/haze.hpp"

namespace {

namespace r = ge::render;

r::HazeTrail stretch(const char* look, double born) {
  r::HazeTrail t;
  t.look = r::haze_look(look);
  t.a[0] = 10, t.a[1] = 2, t.a[2] = 5;
  t.b[0] = 10, t.b[1] = 2, t.b[2] = 5.4;
  t.born_s = born;
  return t;
}

}  // namespace

TEST(the_presets_are_named_and_a_rockets_wake_outdoes_a_nails_thread) {
  const r::HazeLook* nail = r::haze_look("haze.nail");
  const r::HazeLook* rocket = r::haze_look("haze.rocket");
  CHECK(nail != nullptr && rocket != nullptr && r::haze_look("haze.none") == nullptr);
  if (nail == nullptr || rocket == nullptr) return;
  CHECK(r::haze_looks().size() == 2);
  CHECK(rocket->width_m > nail->width_m && rocket->bend_mrad > nail->bend_mrad);
  CHECK(rocket->life_s > nail->life_s);
  for (const r::HazeLook& l : r::haze_looks()) CHECK(l.bend_mrad < 5);  // a few milliradians
}

TEST(a_stretch_warms_at_once_and_cools_to_nothing) {
  const r::HazeTrail t = stretch("haze.nail", 1.0);
  const double life = t.look->life_s;
  CHECK(r::haze_level(t, 0.99) == 0);
  CHECK(r::haze_level(t, 1.0) == 0);  // laid this moment: warming
  const double early = r::haze_level(t, 1.02), late = r::haze_level(t, 1.0 + life * 0.8);
  CHECK(early > 0.75 && early <= 1);  // (1 - 0.02 / 0.22)^2
  CHECK(late > 0 && late < 0.05);
  CHECK(r::haze_level(t, 1.0 + life) == 0 && r::haze_cold(t, 1.0 + life));
  CHECK(!r::haze_cold(t, 1.0 + life * 0.5));
  CHECK_BITS(r::haze_level(t, 1.05), r::haze_level(t, 1.05));
  r::HazeTrail none = t;
  none.look = nullptr;
  CHECK(r::haze_level(none, 1.05) == 0 && r::haze_cold(none, 1.05));
}

TEST(the_staging_is_camera_relative_widens_and_lifts_and_drops_the_cold) {
  r::Haze h;
  r::FrameView v;
  v.eye[0] = 10, v.eye[1] = 1, v.eye[2] = 0;
  v.time_s = 2.0;
  v.near_m = 0.1;
  const std::vector<r::HazeTrail> trails = {stretch("haze.rocket", 1.0),  // cooled by now
                                            stretch("haze.rocket", 1.9),
                                            stretch("haze.rocket", 1.99)};
  r::stage_haze(h, trails, v);
  CHECK(h.count == 2 && h.staged.info[0] == 2);
  const r::GpuHazeSeg& older = h.staging[0];
  const r::GpuHazeSeg& newer = h.staging[1];
  CHECK(older.a[0] == 0 && older.a[1] == 1 && older.a[2] == 5);  // camera-relative
  CHECK(older.b[3] > newer.b[3]);                                // wider as it mixes
  CHECK(older.c[2] > newer.c[2]);                                // risen further
  CHECK(older.a[3] < newer.a[3]);                                // cooler
  CHECK(h.staged.info[1] == 0.1f && h.staged.info[2] == 2.0f);
}

TEST(too_many_keeps_the_newest) {
  r::Haze h;
  r::FrameView v;
  v.time_s = 1.0;
  std::vector<r::HazeTrail> trails;
  for (uint32_t i = 0; i < r::kMaxHaze + 10; ++i) {
    r::HazeTrail t = stretch("haze.rocket", 0.9);
    t.a[0] = i;
    trails.push_back(t);
  }
  r::stage_haze(h, trails, v);
  CHECK(h.count == r::kMaxHaze);
  CHECK(h.staging[0].a[0] == 10.0f);  // the first ten, the oldest, left out
  CHECK(h.staging[r::kMaxHaze - 1].a[0] == static_cast<float>(r::kMaxHaze + 9));
}
