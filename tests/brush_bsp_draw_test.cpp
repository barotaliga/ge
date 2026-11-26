// plugins/brush/app/bsp_draw.hpp's budget for the shadow cubes a frame draws: every cube on the
// map's first frame that shadows anything (no cube held yet), however many frames a look that
// casts none drew before it; `fresh_cubes` a frame after that; `first_all` off keeps the budget
// from the first frame on (a window that warms the map up itself).
//
// ge:summary Tests for the shadow cubes' budget a frame (the load's first shadowed frame)
// ge:keywords test brush bsp shadow-cubes fresh_cubes first_all fresh_cube_budget warm-up
// ge:skill brush testing
#include <cstddef>

#include "brush/app/bsp_draw.hpp"
#include "check.hpp"

namespace {

namespace ba = ge::brush::app;

TEST(the_first_frame_that_shadows_draws_every_cube) {
  ba::Options o;
  o.fresh_cubes = 2;
  CHECK(ba::fresh_cube_budget(o, false, 48) == size_t(48));
}

TEST(a_frame_after_the_cubes_are_in_draws_the_budget) {
  ba::Options o;
  o.fresh_cubes = 2;
  CHECK(ba::fresh_cube_budget(o, true, 48) == size_t(2));
  o.fresh_cubes = -1;
  CHECK(ba::fresh_cube_budget(o, true, 48) == size_t(0));
}

TEST(a_window_that_warms_the_map_up_gets_the_budget_from_the_first_frame) {
  ba::Options o;
  o.fresh_cubes = 2;
  o.first_all = false;
  CHECK(ba::fresh_cube_budget(o, false, 48) == size_t(2));
}

TEST(a_device_with_fewer_cube_slots_draws_what_it_holds) {
  ba::Options o;
  CHECK(ba::fresh_cube_budget(o, false, 42) == size_t(42));
}

}  // namespace
