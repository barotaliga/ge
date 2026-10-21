// engine/render/sky.hpp's decision half: the first frame builds every table; the sky-view table
// is rebuilt when the sun has turned past the threshold or the eye has climbed past it, and not
// for a small turn or a small move.
//
// ge:summary Tests for the sky's rebuild decisions
// ge:keywords test render sky prepare_sky rebuild threshold sky-view tables
// ge:skill render testing
#include "check.hpp"
#include "render/sky.hpp"

TEST(first_frame_builds_everything) {
  ge::render::Sky sky;
  ge::render::FrameView v;
  ge::render::prepare_sky(sky, v);
  CHECK(sky.build_tables);
  CHECK(sky.build_view);
}

TEST(small_changes_reuse_the_view_and_large_ones_rebuild_it) {
  ge::render::Sky sky;
  ge::render::FrameView v;
  ge::render::prepare_sky(sky, v);
  sky.build_tables = sky.build_view = false;  // as encode leaves it

  v.sun_elevation += 0.001;  // well under half a degree
  v.eye[1] += 10;            // under the 50 m threshold
  ge::render::prepare_sky(sky, v);
  CHECK(!sky.build_tables);
  CHECK(!sky.build_view);

  v.sun_elevation += 0.02;  // over a degree
  ge::render::prepare_sky(sky, v);
  CHECK(!sky.build_tables);
  CHECK(sky.build_view);
  sky.build_view = false;

  v.eye[1] += 80;
  ge::render::prepare_sky(sky, v);
  CHECK(sky.build_view);
}
