// Tests: SHIFT held over a played demo turns the view by the mouse, and letting it go drops the
// turn (demo_free_look.hpp).
//
// ge:summary Tests: a demo's free look turns by the mouse while SHIFT is held, and lets go
// ge:keywords test demo free-look shift mouse offset pitch-limit
// ge:skill camera shooter testing
// ge:test tests/shooters_edge_demo_free_look_test.cpp
#include <cmath>

#include "check.hpp"
#include "shooters_edge/client/demo_free_look.hpp"

namespace cl = shooters_edge::client;

namespace {
constexpr double kPi = 3.14159265358979323846;
bool near(double a, double b) { return std::fabs(a - b) < 1e-9; }
}  // namespace

TEST(shift_held_turns_the_view_by_the_players_mouse_and_letting_go_drops_it) {
  ge::shooter::Controls c;  // sensitivity 3, 0.022 degrees a count
  cl::DemoFreeLook f;
  ge::render::FrameView view;
  view.heading = 1.0;
  view.pitch = 0.1;
  cl::free_look_step(f, false, 100, 100, c);  // not held: nothing
  cl::free_look_apply(f, view);
  CHECK(near(view.heading, 1.0) && near(view.pitch, 0.1));
  cl::free_look_step(f, true, 100, 0, c);
  cl::free_look_step(f, true, 100, -50, c);
  CHECK(near(f.yaw, 200 * 3 * 0.022) && near(f.pitch, -50 * 3 * 0.022));
  ge::render::FrameView turned = view;
  cl::free_look_apply(f, turned);
  CHECK(near(turned.heading, 1.0 + 13.2 * kPi / 180));  // right turns the heading clockwise
  CHECK(near(turned.pitch, 0.1 + 3.3 * kPi / 180));     // the mouse up looks up
  c.invert = true;                                      // m_invert: the other way up and down
  cl::free_look_step(f, true, 0, -50, c);
  CHECK(near(f.pitch, 0));
  cl::free_look_step(f, true, 0, 1e6, c);  // far: held within 89 degrees of level
  turned = view;
  cl::free_look_apply(f, turned);
  CHECK(near(turned.pitch, 89 * kPi / 180));
  cl::free_look_step(f, false, 10, 10, c);  // let go: the recorded view again
  CHECK(!f.held && f.yaw == 0 && f.pitch == 0);
}
