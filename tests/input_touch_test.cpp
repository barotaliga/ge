// The touch controls (engine/input/touch.hpp): a floating stick anchored where it was touched,
// kept by its finger outside its zone; a lever anchored full; a tap on a button; a drag; and a
// binding reading them.
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
