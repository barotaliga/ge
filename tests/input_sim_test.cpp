// The simulated backend and the event log (engine/input/sim.hpp, event_log.hpp): a session of
// every kind of device written as text reads back to the same text and replays to the same
// state; a hand-written log reads; and the simulated wheel comes back to centre under a spring.
//
// ge:summary Tests: the event log round trip and replay; the force-fed wheel returns to centre
// ge:keywords test input sim event-log replay round-trip wheel spring force-feedback
// ge:skill input testing
#include <cmath>
#include <cstring>

#include "check.hpp"
#include "input/event_log.hpp"
#include "input/feedback.hpp"
#include "input/names.hpp"
#include "input/sim.hpp"

namespace in = ge::input;

namespace {

// A digest of what the state holds after a frame.
uint64_t digest(const in::InputState& s) {
  uint64_t h = 0xcbf29ce484222325ull;
  const auto mix = [&h](const void* p, size_t n) {
    const auto* b = static_cast<const unsigned char*>(p);
    for (size_t i = 0; i < n; i++) h = (h ^ b[i]) * 0x100000001b3ull;
  };
  for (int k = 0; k < in::kKeys; k++) {
    const bool v[3] = {s.keys_held[k], s.keys_pressed[k], s.keys_released[k]};
    mix(v, sizeof v);
  }
  mix(&s.mouse.dx, sizeof s.mouse.dx);
  mix(&s.mouse.dy, sizeof s.mouse.dy);
  mix(&s.mouse.buttons, sizeof s.mouse.buttons);
  for (const in::Slot& sl : s.slots) {
    mix(&sl.active, 1);
    mix(&sl.buttons, sizeof sl.buttons);
    mix(sl.axes.data(), sizeof sl.axes);
    mix(sl.hats.data(), sizeof sl.hats);
    mix(sl.gyro.data(), sizeof sl.gyro);
  }
  for (const in::Finger& f : s.fingers) {
    mix(&f.x, sizeof f.x);
    mix(&f.active, 1);
  }
  return h;
}

// Every kind of device, every kind of event, over a few frames.
in::EventLog session(std::vector<uint64_t>& digests) {
  in::SimInput sim;
  in::InputState s;
  in::EventLog log;
  in::Queue kept;
  const auto frame = [&] {
    const uint64_t t = sim.t_ns;
    in::sim_frame(sim, s, &kept);
    log.add(t, kept);
    digests.push_back(digest(s));
    in::sim_advance(sim, 1.0 / 60);
  };
  const auto kb = in::sim_plug(sim, in::sim_keyboard());
  const auto mouse = in::sim_plug(sim, in::sim_mouse());
  const auto pad = in::sim_plug(sim, in::sim_pad(in::Family::playstation));
  const auto wheel = in::sim_plug(sim, in::sim_wheel());
  const auto screen = in::sim_plug(sim, in::sim_touch());
  frame();
  in::sim_key(sim, kb, in::key::w, true);
  in::sim_text(sim, kb, 'w');
  in::sim_mouse_move(sim, mouse, 1.25f, -3);
  in::sim_mouse_button(sim, mouse, in::MouseButton::right, true);
  in::sim_wheel_scroll(sim, mouse, 0, -1);
  in::sim_button(sim, pad, static_cast<int>(in::PadButton::touchpad), true);
  in::sim_axis(sim, pad, static_cast<int>(in::PadAxis::left_trigger), 0.333f);
  in::sim_gyro(sim, pad, 0.1f, -0.2f, 0.3f);
  in::sim_battery(sim, pad, 0.75f);
  in::sim_axis(sim, wheel, 0, -0.123456f);
  in::sim_button(sim, wheel, 17, true);
  in::sim_hat(sim, wheel, 0, in::kHatDown | in::kHatLeft);
  in::sim_touch(sim, screen, 3, in::TouchPhase::down, 0.5f, 0.25f);
  frame();
  in::sim_key(sim, kb, in::key::w, false);
  in::sim_touch(sim, screen, 3, in::TouchPhase::move, 0.6f, 0.2f);
  in::sim_unplug(sim, wheel);
  frame();
  return log;
}

}  // namespace

TEST(a_session_written_as_an_event_log_reads_back_to_the_same_text) {
  std::vector<uint64_t> live;
  const in::EventLog log = session(live);
  const std::string text = in::write_events(log);
  in::EventLog back;
  std::string error;
  CHECK(in::read_events(text, back, error));
  if (!error.empty()) std::fprintf(stderr, "%s\n", error.c_str());
  CHECK(in::write_events(back) == text);
  CHECK(back.frames.size() == 3);
}

TEST(an_event_log_replays_to_the_same_state_every_frame) {
  std::vector<uint64_t> live;
  const in::EventLog log = session(live);
  in::EventLog back;
  std::string error;
  CHECK(in::read_events(in::write_events(log), back, error));
  in::InputState s;
  for (size_t i = 0; i < back.frames.size(); i++) {
    in::update(s, back.frames[i].q);
    CHECK(digest(s) == live[i]);
  }
}

TEST(a_log_written_by_hand_reads_and_a_wrong_line_is_named) {
  const char* text = R"(ge-events 1
# a pad and a key, by hand
device 1 gamepad xbox fff1:0002 features=rumble,gyro "a pad"
frame 0
0 1 added
frame 0.5
0.5 1 pad_button south down
0.5 1 pad_axis left_x -0.5
0.51 2 key space down
)";
  in::EventLog log;
  std::string error;
  CHECK(in::read_events(text, log, error));
  in::InputState s;
  for (const auto& f : log.frames) in::update(s, f.q);
  const in::Slot* pad = in::find_slot(s, 1);
  CHECK(pad && pad->info.name == "a pad" && in::has(pad->info, in::kGyro));
  CHECK(pad && pad->buttons.went_down(0) && pad->axes[0] == -0.5f);
  CHECK(s.keys_pressed[in::key::space]);
  CHECK(!in::read_events("ge-events 1\nframe 0\n0 1 pad_button sideways down\n", log, error));
  CHECK(error.find("line 3") != std::string::npos);
  CHECK(!in::read_events("ge-events 1\nframe 0\n0 4 added\n", log, error));
}

TEST(the_simulated_wheel_turned_by_hand_comes_back_to_centre_under_a_spring) {
  in::SimInput sim;
  in::InputState s;
  in::Feedback fb;
  in::SimWheel w;
  w.id = in::sim_plug(sim, in::sim_wheel());
  in::Effect spring;
  spring.kind = in::EffectKind::spring;
  spring.coeff = 0.8f;
  in::play_effect(fb, w.id, spring, 0);
  in::Effect damper;
  damper.kind = in::EffectKind::damper;
  damper.coeff = 0.05f;
  in::play_effect(fb, w.id, damper, 0);
  w.hold = 0.8;  // the hand turns it right and holds it there
  for (int i = 0; i < 240; i++) {
    in::sim_wheel_step(sim, w, fb, 1.0 / 120);
    in::sim_frame(sim, s);
    in::sim_advance(sim, 1.0 / 120);
  }
  CHECK(w.pos > 0.5 && w.pos < 0.8);  // the spring pulls against the hand
  CHECK(std::fabs(in::find_slot(s, w.id)->axes[0] - static_cast<float>(w.pos)) < 1e-6);
  w.hold = -2;  // let go
  for (int i = 0; i < 600; i++) {
    in::sim_wheel_step(sim, w, fb, 1.0 / 120);
    in::sim_advance(sim, 1.0 / 120);
  }
  CHECK(std::fabs(w.pos) < 0.05);
  // With no spring, a wheel let go stays where it was left (its friction holds it).
  in::stop_effect(fb, w.id, 0);
  in::stop_effect(fb, w.id, 1);
  w.pos = 0.4;
  w.vel = 0;
  for (int i = 0; i < 120; i++) in::sim_wheel_step(sim, w, fb, 1.0 / 120);
  CHECK(w.pos == 0.4);
}
