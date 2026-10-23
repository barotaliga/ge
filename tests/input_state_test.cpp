// The input state (engine/input/state.hpp): a frame's events folded into held, pressed and
// released; a tap inside one frame kept; repeats ignored; a device unplugged releasing what it
// held; the mouse summed; fingers; hats; text; the last used device.
//
// ge:summary Tests: the input state's edges, taps, repeats, unplug, mouse sums, fingers, hats
// ge:keywords test input state edges tap repeat unplug mouse finger hat text last-used
// ge:skill input testing
#include "check.hpp"
#include "input/names.hpp"
#include "input/sim.hpp"
#include "input/state.hpp"

namespace in = ge::input;

TEST(a_key_tapped_inside_one_frame_is_pressed_and_released_and_not_held) {
  in::SimInput sim;
  in::InputState s;
  const auto kb = in::sim_plug(sim, in::sim_keyboard());
  in::sim_frame(sim, s);
  in::sim_key(sim, kb, in::key::space, true);
  in::sim_key(sim, kb, in::key::space, false);
  in::sim_frame(sim, s);
  CHECK(s.keys_pressed[in::key::space] && s.keys_released[in::key::space]);
  CHECK(!s.keys_held[in::key::space]);
  in::sim_frame(sim, s);
  CHECK(!s.keys_pressed[in::key::space] && !s.keys_released[in::key::space]);
}

TEST(a_held_key_presses_once_and_its_repeats_are_not_presses) {
  in::Queue q;
  in::InputState s;
  q.push(in::key_event(0, 1, in::key::a, true));
  in::update(s, q);
  CHECK(s.keys_pressed[in::key::a] && s.keys_held[in::key::a]);
  q.clear();
  q.push(in::key_event(1, 1, in::key::a, true, true));
  in::update(s, q);
  CHECK(!s.keys_pressed[in::key::a] && s.keys_held[in::key::a]);
}

TEST(a_pad_unplugged_releases_every_button_it_held_and_frees_its_slot) {
  in::SimInput sim;
  in::InputState s;
  const auto pad = in::sim_plug(sim, in::sim_pad());
  in::sim_frame(sim, s);
  CHECK(s.added_n == 1 && s.added[0] == pad);
  in::sim_button(sim, pad, static_cast<int>(in::PadButton::south), true);
  in::sim_axis(sim, pad, static_cast<int>(in::PadAxis::left_x), 0.8f);
  in::sim_frame(sim, s);
  CHECK(in::find_slot(s, pad)->buttons.down(0));
  CHECK(s.last_used == pad && s.last_kind == in::DeviceKind::gamepad);
  in::sim_unplug(sim, pad);
  in::sim_frame(sim, s);
  CHECK(in::find_slot(s, pad) == nullptr);
  CHECK(s.removed_n == 1 && s.removed[0] == pad);
  CHECK(s.last_used == 0);
  // A new device takes the freed slot, and an event for the old id reaches nothing.
  const auto other = in::sim_plug(sim, in::sim_pad(in::Family::playstation));
  in::sim_button(sim, pad, 1, true);
  in::sim_frame(sim, s);
  CHECK(other != pad);
  CHECK(in::find_slot(s, other)->buttons.held == 0);
  CHECK(in::count_kind(s, in::DeviceKind::gamepad) == 1);
}

TEST(the_mouse_sums_its_raw_motion_and_wheel_over_the_frame) {
  in::SimInput sim;
  in::InputState s;
  const auto m = in::sim_plug(sim, in::sim_mouse());
  for (int i = 0; i < 1000; i++) in::sim_mouse_move(sim, m, 0.25f, -0.125f);
  in::sim_wheel_scroll(sim, m, 0, 1);
  in::sim_wheel_scroll(sim, m, 0, 1);
  in::sim_frame(sim, s);
  CHECK(s.mouse.dx == 250 && s.mouse.dy == -125);
  CHECK(s.mouse.raw);
  CHECK(s.mouse.wheel_y == 2);
  in::sim_frame(sim, s);
  CHECK(s.mouse.dx == 0 && s.mouse.wheel_y == 0);
}

TEST(fingers_touch_down_move_and_lift_and_a_tap_in_one_frame_begins_and_ends) {
  in::SimInput sim;
  in::InputState s;
  const auto screen = in::sim_plug(sim, in::sim_touch());
  in::sim_touch(sim, screen, 7, in::TouchPhase::down, 0.2f, 0.7f);
  in::sim_frame(sim, s);
  CHECK(s.fingers[0].active && s.fingers[0].began && s.fingers[0].id == 7);
  in::sim_touch(sim, screen, 7, in::TouchPhase::move, 0.3f, 0.6f);
  in::sim_frame(sim, s);
  CHECK(s.fingers[0].x == 0.3f && s.fingers[0].x0 == 0.2f && !s.fingers[0].began);
  in::sim_touch(sim, screen, 7, in::TouchPhase::up, 0.3f, 0.6f);
  in::sim_touch(sim, screen, 9, in::TouchPhase::down, 0.9f, 0.1f);
  in::sim_touch(sim, screen, 9, in::TouchPhase::up, 0.9f, 0.1f);
  in::sim_frame(sim, s);
  CHECK(!s.fingers[0].active && s.fingers[0].ended);
  CHECK(s.fingers[1].began && s.fingers[1].ended && !s.fingers[1].active);
}

TEST(a_hat_s_new_directions_are_its_presses_and_text_is_kept_in_order) {
  in::SimInput sim;
  in::InputState s;
  const auto stick = in::sim_plug(sim, in::sim_flight_stick());
  const auto kb = in::sim_plug(sim, in::sim_keyboard());
  in::sim_hat(sim, stick, 0, in::kHatUp);
  in::sim_text(sim, kb, 'h');
  in::sim_text(sim, kb, 0x1F3BF);  // beyond 16 bits
  in::sim_frame(sim, s);
  CHECK(in::find_slot(s, stick)->hats_pressed[0] == in::kHatUp);
  CHECK(s.text_n == 2 && s.text[0] == 'h' && s.text[1] == 0x1F3BF);
  in::sim_hat(sim, stick, 0, in::kHatUp | in::kHatRight);
  in::sim_frame(sim, s);
  CHECK(in::find_slot(s, stick)->hats_pressed[0] == in::kHatRight);
  CHECK(in::find_slot(s, stick)->hats[0] == (in::kHatUp | in::kHatRight));
}
