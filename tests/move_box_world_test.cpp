// The box world (engine/move/box_world.hpp): a box dropped onto the floor stops a skin above it
// with an upward normal; one swept into a wall stops short of it facing back; a ramp's slanted
// face answers its own normal at the height its slope says; a box already inside is start-solid
// (all-solid when it never gets out); a sweep past everything touches nothing; stairs are a box a
// step.
//
// ge:summary Tests: the box world's swept-box trace (floor, wall, ramp, solid, miss, stairs)
// ge:keywords test move box-world trace sweep floor wall ramp start-solid all-solid stairs
// ge:skill move testing
#include <cmath>
#include <string>

#include "check.hpp"
#include "move/box_world.hpp"

namespace mv = ge::move;

namespace {

const mv::Box kHull{{-16, 0, -16}, {16, 56, 16}};

bool near(double a, double b, double tol) { return std::fabs(a - b) <= tol; }

mv::BoxWorld floor_world() {
  mv::BoxWorld w;
  mv::add_box(w, {-1000, -10, -1000}, {1000, 0, 1000});
  return w;
}

}  // namespace

TEST(a_box_dropped_on_the_floor_stops_a_skin_above_it_facing_up) {
  const mv::BoxWorld w = floor_world();
  const mv::Hit h = mv::trace_box_world(w, kHull, {0, 100, 0}, {0, -100, 0});
  CHECK(h.touched() && !h.start_solid);
  CHECK_MSG(near(h.end.y, w.skin, 1e-9), "stopped at y " + std::to_string(h.end.y));
  CHECK(h.normal.y == 1 && h.normal.x == 0 && h.normal.z == 0);
  CHECK(near(h.fraction, (100 - w.skin) / 200, 1e-12));
}

TEST(a_box_swept_into_a_wall_stops_short_facing_back) {
  mv::BoxWorld w = floor_world();
  mv::add_box(w, {-100, 0, 200}, {100, 100, 210});
  const mv::Hit h = mv::trace_box_world(w, kHull, {0, 1, 0}, {0, 1, 400});
  CHECK(h.touched());
  CHECK_MSG(near(h.end.z, 200 - 16 - w.skin, 1e-9), "stopped at z " + std::to_string(h.end.z));
  CHECK(h.normal.z == -1);
}

TEST(a_ramp_answers_its_slanted_normal_at_its_slope_height) {
  mv::BoxWorld w;
  mv::add_ramp(w, {-100, 0, 0}, {100, 100, 100}, mv::Rise::pz);  // 45 degrees up +z
  // A point dropped at z = 50 meets the face at y = 50.
  const mv::Box point{{0, 0, 0}, {0, 0, 0}};
  const mv::Hit h = mv::trace_box_world(w, point, {0, 200, 50}, {0, -10, 50});
  CHECK(h.touched());
  CHECK_MSG(near(h.end.y, 50, 0.1), "met the ramp at y " + std::to_string(h.end.y));
  CHECK(near(h.normal.y, std::sqrt(0.5), 1e-12) && near(h.normal.z, -std::sqrt(0.5), 1e-12));
  // A hull dropped there meets the face where its bottom edge up the slope does: 16 farther up,
  // so at y 66.
  const mv::Hit hb = mv::trace_box_world(w, kHull, {0, 200, 50}, {0, -10, 50});
  CHECK_MSG(near(hb.end.y, 66, 0.1), "the hull met the ramp at y " + std::to_string(hb.end.y));
  // Down the other way round (rise -z), the face leans the other way.
  mv::BoxWorld back;
  mv::add_ramp(back, {-100, 0, 0}, {100, 100, 100}, mv::Rise::nz);
  const mv::Hit hn = mv::trace_box_world(back, point, {0, 200, 25}, {0, -10, 25});
  CHECK_MSG(near(hn.end.y, 75, 0.1), "met the reversed ramp at y " + std::to_string(hn.end.y));
  CHECK(hn.normal.z > 0);
}

TEST(a_box_inside_is_start_solid_and_all_solid_when_it_never_gets_out) {
  const mv::BoxWorld w = floor_world();
  const mv::Hit stuck = mv::trace_box_world(w, kHull, {0, -5, 0}, {0, -6, 0});
  CHECK(stuck.start_solid && stuck.all_solid && stuck.fraction == 0);
  const mv::Hit out = mv::trace_box_world(w, kHull, {0, -5, 0}, {0, 50, 0});
  CHECK(out.start_solid && !out.all_solid);
  const mv::Hit still = mv::trace_box_world(w, kHull, {0, 10, 0}, {0, 10, 0});
  CHECK(!still.start_solid && !still.touched());
}

TEST(a_sweep_past_everything_touches_nothing) {
  const mv::BoxWorld w = floor_world();
  const mv::Hit h = mv::trace_box_world(w, kHull, {0, 10, 0}, {500, 10, 300});
  CHECK(!h.touched() && h.end.x == 500 && h.end.z == 300);
}

TEST(stairs_are_a_box_a_step_each_a_rise_higher) {
  mv::BoxWorld w;
  mv::add_stairs(w, {0, 0, 0}, 5, 8, 16, 64);
  CHECK(w.brushes.size() == 5);
  CHECK(w.brushes[4].hi.y == 40 && w.brushes[4].lo.z == 64 && w.brushes[4].hi.z == 80);
  const mv::Box point{{0, 0, 0}, {0, 0, 0}};
  const mv::Hit h = mv::trace_box_world(w, point, {32, 100, 70}, {32, -10, 70});
  CHECK(near(h.end.y, 40, 0.1));
}
