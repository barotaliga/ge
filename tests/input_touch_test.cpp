// The touch controls (engine/input/touch.hpp): a floating stick anchored where it was touched,
// kept by its finger outside its zone; a lever anchored full; a tap on a button; a drag; and a
// binding reading them; a trailing stick, a lever's brake range under its throttle, a flick and a
// tap-then-hold.
//
// ge:summary Tests: floating stick, lever, button tap, drag, and bindings reading touch controls
// ge:keywords test input touch stick lever drag button tap zone
// ge:skill input testing
#include <cmath>

#include "check.hpp"
#include "input/actions.hpp"
#include "input/bindings_text.hpp"
#include "input/sim.hpp"
#include "input/touch.hpp"

namespace in = ge::input;

namespace {

in::TouchLayout layout() {
  in::TouchLayout l;
  l.aspect = 2;
  l.controls = {{"stick", in::TouchKind::stick, 0, 0.3f, 0.4f, 1, 0.1f, 0, false},
                {"tuck", in::TouchKind::lever, 0.6f, 0.3f, 1, 1, 0.2f, 1, false},
                {"jump", in::TouchKind::button, 0.6f, 0, 1, 0.3f, 0.1f, 0, false},
                {"look", in::TouchKind::drag, 0.4f, 0, 0.6f, 1, 0.1f, 0, false}};
  return l;
}

bool near(double a, double b) { return std::fabs(a - b) < 1e-5; }

}  // namespace

TEST(a_floating_stick_is_anchored_where_it_was_touched_and_follows_outside_its_zone) {
  in::SimInput sim;
  in::InputState s;
  const auto l = layout();
  in::TouchFrame f = in::make_touch_frame(l);
  const auto screen = in::sim_plug(sim, in::sim_touch());
  in::sim_touch(sim, screen, 1, in::TouchPhase::down, 0.2f, 0.6f);
  in::sim_frame(sim, s);
  in::touch_update(l, s, f);
  CHECK(f.c[0].held && f.c[0].pressed && f.c[0].x == 0 && f.c[0].ax == 0.2f);
  in::sim_touch(sim, screen, 1, in::TouchPhase::move, 0.225f, 0.6f);  // 0.05 of the height
  in::sim_frame(sim, s);
  in::touch_update(l, s, f);
  CHECK(near(f.c[0].x, 0.5) && f.c[0].y == 0);
  in::sim_touch(sim, screen, 1, in::TouchPhase::move, 0.9f, 0.6f);  // far out of its zone
  in::sim_frame(sim, s);
  in::touch_update(l, s, f);
  CHECK(f.c[0].held && near(f.c[0].x, 1) && !f.c[1].held);
  in::sim_touch(sim, screen, 1, in::TouchPhase::up, 0.9f, 0.6f);
  in::sim_frame(sim, s);
  in::touch_update(l, s, f);
  CHECK(!f.c[0].held && f.c[0].released && f.c[0].x == 0);
}

TEST(a_lever_anchored_full_eases_off_as_the_thumb_slides_down) {
  in::SimInput sim;
  in::InputState s;
  const auto l = layout();
  in::TouchFrame f = in::make_touch_frame(l);
  const auto screen = in::sim_plug(sim, in::sim_touch());
  in::sim_touch(sim, screen, 4, in::TouchPhase::down, 0.8f, 0.6f);
  in::sim_frame(sim, s);
  in::touch_update(l, s, f);
  CHECK(f.c[1].x == 1);
  in::sim_touch(sim, screen, 4, in::TouchPhase::move, 0.8f, 0.7f);
  in::sim_frame(sim, s);
  in::touch_update(l, s, f);
  CHECK(near(f.c[1].x, 0.5));
  in::sim_touch(sim, screen, 4, in::TouchPhase::move, 0.8f, 0.95f);
  in::sim_frame(sim, s);
  in::touch_update(l, s, f);
  CHECK(f.c[1].x == 0);
}

TEST(a_tap_presses_the_button_and_a_drag_moves_the_look_through_the_bindings) {
  in::SimInput sim;
  in::InputState s;
  const auto l = layout();
  in::TouchFrame f = in::make_touch_frame(l);
  in::ActionMap m;
  m.defs = {{"jump", in::ActionKind::button, false, ""},
            {"look", in::ActionKind::delta, false, ""},
            {"move", in::ActionKind::stick, false, ""}};
  std::string error;
  CHECK(
      in::read_bindings("ge-bindings 1\ncontext t\nbind jump touch:jump\nbind look "
                        "touch:look value=10\nbind move touch:stick zone=none\n",
                        m, error));
  in::ActionValues v = in::make_values(m);
  in::Sources from;
  from.layout = &l;
  from.touch = &f;
  const auto screen = in::sim_plug(sim, in::sim_touch());
  in::sim_touch(sim, screen, 2, in::TouchPhase::down, 0.8f, 0.1f);
  in::sim_touch(sim, screen, 2, in::TouchPhase::up, 0.8f, 0.1f);
  in::sim_touch(sim, screen, 3, in::TouchPhase::down, 0.5f, 0.5f);
  in::sim_frame(sim, s);
  in::touch_update(l, s, f);
  in::evaluate(m, s, 1.0 / 60, v, from);
  CHECK(v[0].pressed && v[0].released);
  in::sim_touch(sim, screen, 3, in::TouchPhase::move, 0.55f, 0.45f);
  in::sim_frame(sim, s);
  in::touch_update(l, s, f);
  in::evaluate(m, s, 1.0 / 60, v, from);
  CHECK(near(v[1].x, 1.0) && near(v[1].y, -0.5));
}

namespace {

// One frame of a finger: moved (or put down or lifted), the frame folded and read at `t`.
void touch_at(in::SimInput& sim, in::InputState& s, const in::TouchLayout& l, in::TouchFrame& f,
              in::DeviceId screen, in::TouchPhase phase, float x, float y, double t) {
  in::sim_touch(sim, screen, 7, phase, x, y);
  in::sim_frame(sim, s);
  in::touch_update(l, s, f, t);
}

in::TouchLayout thumbs() {
  in::TouchLayout l;
  l.aspect = 2;
  in::TouchControl edge{"edge", in::TouchKind::stick, 0, 0, 0.5f, 1, 0.1f, 0, false};
  edge.trail = true;
  in::TouchControl lever{"lever", in::TouchKind::lever, 0.5f, 0, 1, 1, 0.1f, 1, false};
  lever.floor = -1;
  lever.dead = 0.2f;
  in::TouchControl flick{"flick", in::TouchKind::flick};
  flick.of = "lever";
  in::TouchControl load{"load", in::TouchKind::tap_hold};
  load.of = "lever";
  l.controls = {edge, lever, flick, load};
  return l;
}

}  // namespace

TEST(a_trailing_stick_drags_its_centre_so_the_way_back_is_no_longer_than_the_push) {
  in::SimInput sim;
  in::InputState s;
  const auto l = thumbs();
  in::TouchFrame f = in::make_touch_frame(l);
  const auto screen = in::sim_plug(sim, in::sim_touch());
  touch_at(sim, s, l, f, screen, in::TouchPhase::down, 0.2f, 0.5f, 0);
  touch_at(sim, s, l, f, screen, in::TouchPhase::move, 0.4f, 0.5f, 0.1);  // 0.4 of the height
  CHECK(near(f.c[0].x, 1) && near(f.c[0].ax, 0.35f));
  touch_at(sim, s, l, f, screen, in::TouchPhase::move, 0.375f, 0.5f, 0.2);  // back half a radius
  CHECK(near(f.c[0].x, 0.5));
}

TEST(a_lever_with_a_floor_is_a_brake_under_its_throttle_past_a_dead_band) {
  in::SimInput sim;
  in::InputState s;
  const auto l = thumbs();
  in::TouchFrame f = in::make_touch_frame(l);
  const auto screen = in::sim_plug(sim, in::sim_touch());
  touch_at(sim, s, l, f, screen, in::TouchPhase::down, 0.8f, 0.4f, 0);
  CHECK(f.c[1].x == 1);
  touch_at(sim, s, l, f, screen, in::TouchPhase::move, 0.8f, 0.51f, 0.1);  // inside the dead band
  CHECK(f.c[1].x == 0);
  touch_at(sim, s, l, f, screen, in::TouchPhase::move, 0.8f, 0.57f, 0.2);  // half past it
  CHECK(near(f.c[1].x, -0.5));
  touch_at(sim, s, l, f, screen, in::TouchPhase::move, 0.8f, 0.9f, 0.3);
  CHECK(f.c[1].x == -1);
  in::ActionMap m;
  m.defs = {{"tuck", in::ActionKind::axis, false, ""}, {"brake", in::ActionKind::axis, false, ""}};
  std::string error;
  CHECK(in::read_bindings(
      "ge-bindings 1\ncontext t\nbind tuck touch:lever+\nbind brake touch:lever-\n", m, error));
  in::ActionValues v = in::make_values(m);
  in::Sources from;
  from.layout = &l;
  from.touch = &f;
  in::evaluate(m, s, 1.0 / 60, v, from);
  CHECK(v[0].x == 0 && near(v[1].x, 1));
  CHECK(in::write_bindings(m).find("bind brake touch:lever-\n") != std::string::npos);
}

TEST(a_flick_up_presses_its_button_for_longer_the_faster_it_was) {
  in::SimInput sim;
  in::InputState s;
  const auto l = thumbs();
  in::TouchFrame f = in::make_touch_frame(l);
  const auto screen = in::sim_plug(sim, in::sim_touch());
  touch_at(sim, s, l, f, screen, in::TouchPhase::down, 0.8f, 0.6f, 0);
  touch_at(sim, s, l, f, screen, in::TouchPhase::move, 0.8f, 0.59f, 1.0 / 60);  // a slow slide
  CHECK(!f.c[2].held);
  // A rise of 0.13 of the height in three frames (2.6 heights a second): a flick.
  touch_at(sim, s, l, f, screen, in::TouchPhase::move, 0.8f, 0.53f, 2.0 / 60);
  touch_at(sim, s, l, f, screen, in::TouchPhase::move, 0.8f, 0.47f, 3.0 / 60);
  CHECK(f.c[2].held && f.c[2].pressed && f.c[2].strength > 0.15f && f.c[2].strength < 0.45f);
  const double until = f.c[2].until_s;
  CHECK(until > 3.0 / 60 + 0.05 && until < 3.0 / 60 + 0.15);
  touch_at(sim, s, l, f, screen, in::TouchPhase::move, 0.8f, 0.40f, 4.0 / 60);  // still rising
  CHECK(f.c[2].held && !f.c[2].pressed);  // fired once, not again until it comes back down
  touch_at(sim, s, l, f, screen, in::TouchPhase::move, 0.8f, 0.40f, until + 0.01);
  CHECK(!f.c[2].held && f.c[2].released);
}

TEST(a_tap_then_a_hold_loads_the_gesture_until_the_thumb_lifts) {
  in::SimInput sim;
  in::InputState s;
  const auto l = thumbs();
  in::TouchFrame f = in::make_touch_frame(l);
  const auto screen = in::sim_plug(sim, in::sim_touch());
  touch_at(sim, s, l, f, screen, in::TouchPhase::down, 0.8f, 0.5f, 0);  // a long touch: no tap
  touch_at(sim, s, l, f, screen, in::TouchPhase::up, 0.8f, 0.5f, 0.5);
  touch_at(sim, s, l, f, screen, in::TouchPhase::down, 0.8f, 0.5f, 0.6);
  CHECK(!f.c[3].held);
  touch_at(sim, s, l, f, screen, in::TouchPhase::up, 0.8f, 0.5f, 0.7);  // a tap
  touch_at(sim, s, l, f, screen, in::TouchPhase::down, 0.8f, 0.5f, 0.9);
  CHECK(f.c[3].held && f.c[3].pressed && f.c[1].held);
  touch_at(sim, s, l, f, screen, in::TouchPhase::move, 0.8f, 0.5f, 1.4);
  CHECK(f.c[3].held);
  touch_at(sim, s, l, f, screen, in::TouchPhase::up, 0.8f, 0.5f, 1.5);
  CHECK(!f.c[3].held && f.c[3].released);
  touch_at(sim, s, l, f, screen, in::TouchPhase::down, 0.8f, 0.5f, 1.6);  // a load is no tap
  CHECK(!f.c[3].held);
}
