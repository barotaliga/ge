// The actions (engine/input/actions.hpp, bindings_text.hpp, rebind.hpp): keys and a pad on one
// axis, a lever, a stick from four keys, the mouse's look, a chord, a blocking context, a
// player's devices only; the bindings file both ways and its mistakes; rebinding by pressing.
//
// ge:summary Tests: evaluate's combining, contexts, chords, per player; the bindings file; capture
// ge:keywords test input actions evaluate bindings text parse rebind capture conflicts chord
// ge:skill input testing
#include <cmath>

#include "check.hpp"
#include "input/actions.hpp"
#include "input/bindings_text.hpp"
#include "input/names.hpp"
#include "input/rebind.hpp"
#include "input/sim.hpp"

namespace in = ge::input;

namespace {

// A source word the test knows reads (a word that does not is the empty source).
in::Source src_of(std::string_view w) {
  const auto s = in::parse_source(w);
  return s ? *s : in::Source{};
}

constexpr double kDt = 1.0 / 60;

in::ActionMap ride_map() {
  in::ActionMap m;
  m.defs = {{"steer", in::ActionKind::axis, false, "turn"},
            {"tuck", in::ActionKind::axis, true, "fold into the tuck"},
            {"jump", in::ActionKind::button, false, "jump"},
            {"move", in::ActionKind::stick, false, "walk"},
            {"look", in::ActionKind::delta, false, "look around"},
            {"sprint", in::ActionKind::button, false, "run"},
            {"type", in::ActionKind::button, false, "the console's key"}};
  std::string error;
  const bool ok = in::read_bindings(R"(ge-bindings 1
context console blocking off
bind type key:a
context ride
bind steer key:a value=-1
bind steer key:d
bind steer pad:left_x
bind tuck key:w ramp=4,12
bind tuck pad:right_trigger
bind jump key:space
bind jump pad:south
bind move key:w component=y value=-1
bind move key:s component=y
bind move key:a value=-1
bind move key:d
bind move pad:left_stick
bind look mouse:x value=0.5
bind look mouse:y component=y value=-0.5
bind look pad:right_stick rate=2
bind sprint key:w with=key:left_shift
)",
                                    m, error);
  if (!ok) std::fprintf(stderr, "%s\n", error.c_str());
  return m;
}

struct Rig {
  in::SimInput sim;
  in::InputState s;
  in::ActionMap map = ride_map();
  in::ActionValues v = in::make_values(map);
  in::DeviceId kb = 0, mouse = 0, pad = 0;
  Rig() {
    kb = in::sim_plug(sim, in::sim_keyboard());
    mouse = in::sim_plug(sim, in::sim_mouse());
    pad = in::sim_plug(sim, in::sim_pad());
    frame();
  }
  void frame(const in::Sources& from = {}) {
    in::sim_frame(sim, s);
    in::evaluate(map, s, kDt, v, from);
    in::sim_advance(sim, kDt);
  }
  const in::ActionValue& operator[](const char* name) const {
    return v[static_cast<size_t>(map.find_action(name))];
  }
};

bool near(double a, double b) { return std::fabs(a - b) < 1e-5; }

}  // namespace

TEST(two_keys_held_together_cancel_and_the_pad_wins_when_it_pushes_harder) {
  Rig r;
  in::sim_key(r.sim, r.kb, in::key::a, true);
  in::sim_key(r.sim, r.kb, in::key::d, true);
  r.frame();
  CHECK(r["steer"].x == 0);
  in::sim_key(r.sim, r.kb, in::key::d, false);
  r.frame();
  CHECK(r["steer"].x == -1);  // no ramp on this binding: at once
  in::sim_axis(r.sim, r.pad, static_cast<int>(in::PadAxis::left_x), 0.5f);
  r.frame();
  CHECK(r["steer"].x == -1);  // the key still pushes harder
  in::sim_key(r.sim, r.kb, in::key::a, false);
  r.frame();
  CHECK(r["steer"].x > 0.4f && r["steer"].x < 0.5f);  // the pad, through its dead zone
  CHECK(r["steer"].device == r.pad);
}

TEST(a_lever_ramps_from_its_key_and_reads_the_trigger) {
  Rig r;
  in::sim_key(r.sim, r.kb, in::key::w, true);
  r.frame();
  CHECK(r["tuck"].x > 0 && r["tuck"].x < 0.1f);
  for (int i = 0; i < 120; i++) r.frame();
  CHECK(r["tuck"].x > 0.99f);
  in::sim_key(r.sim, r.kb, in::key::w, false);
  for (int i = 0; i < 30; i++) r.frame();
  CHECK(r["tuck"].x == 0);
  in::sim_axis(r.sim, r.pad, static_cast<int>(in::PadAxis::right_trigger), 1.0f);
  r.frame();
  CHECK(r["tuck"].x == 1);
}

TEST(four_keys_make_a_stick_no_longer_than_one_and_a_tap_is_a_press) {
  Rig r;
  in::sim_key(r.sim, r.kb, in::key::w, true);
  in::sim_key(r.sim, r.kb, in::key::d, true);
  r.frame();
  CHECK(near(r["move"].x, std::sqrt(0.5)) && near(r["move"].y, -std::sqrt(0.5)));
  in::sim_key(r.sim, r.kb, in::key::space, true);
  in::sim_key(r.sim, r.kb, in::key::space, false);
  r.frame();
  CHECK(r["jump"].pressed && r["jump"].released && !r["jump"].held);
  in::sim_button(r.sim, r.pad, static_cast<int>(in::PadButton::south), true);
  r.frame();
  CHECK(r["jump"].pressed && r["jump"].held && r["jump"].device == r.pad);
  r.frame();
  CHECK(!r["jump"].pressed && r["jump"].held);
}

TEST(the_mouse_look_is_its_motion_scaled_and_the_stick_turns_at_its_rate) {
  Rig r;
  in::sim_mouse_move(r.sim, r.mouse, 10, 4);
  in::sim_mouse_move(r.sim, r.mouse, 2, 0);
  r.frame();
  CHECK(r["look"].x == 6 && r["look"].y == -2);
  in::sim_axis(r.sim, r.pad, static_cast<int>(in::PadAxis::right_x), 1.0f);
  r.frame();
  CHECK(near(r["look"].x, 2 * kDt));
}

TEST(a_chord_reads_only_while_its_modifier_is_held) {
  Rig r;
  in::sim_key(r.sim, r.kb, in::key::w, true);
  r.frame();
  CHECK(!r["sprint"].held);
  in::sim_key(r.sim, r.kb, in::key::left_shift, true);
  r.frame();
  CHECK(r["sprint"].pressed);
}

TEST(a_blocking_context_takes_its_keys_from_the_contexts_after_it) {
  Rig r;
  in::sim_key(r.sim, r.kb, in::key::a, true);
  r.frame();
  CHECK(r["steer"].x == -1 && !r["type"].held);
  CHECK(r.map.set_active("console", true));
  r.frame();
  CHECK(r["type"].held && r["steer"].x == 0);
  CHECK(!r.map.set_active("nothing", true));
}

TEST(a_player_reads_only_his_own_devices) {
  Rig r;
  const auto pad2 = in::sim_plug(r.sim, in::sim_pad(in::Family::playstation));
  r.frame();
  in::sim_button(r.sim, pad2, static_cast<int>(in::PadButton::south), true);
  const in::DeviceId mine[] = {r.pad};
  in::Sources from;
  from.only = mine;
  from.keyboard = from.mouse = false;
  r.frame(from);
  CHECK(!r["jump"].held);
  const in::DeviceId theirs[] = {pad2};
  from.only = theirs;
  r.frame(from);
  CHECK(r["jump"].held);
}

TEST(sources_read_back_from_their_words) {
  const char* words[] = {"key:a",         "key:left_shift",  "mouse:left",   "mouse:x",
                         "mouse:y-",      "mouse:wheel_y+",  "pad:south",    "pad:left_x",
                         "pad:left_x-",   "pad:right_stick", "joy:button12", "joy:axis3+",
                         "joy:hat0_left", "gyro:y",          "touch:thumb",  "role:clutch",
                         "role:steer-"};
  for (const char* w : words) {
    const auto s = in::parse_source(w);
    CHECK(s.has_value());
    if (s) CHECK(in::source_text(*s) == w);
  }
  CHECK(!in::parse_source("key:a+") && !in::parse_source("pad:north+") &&
        !in::parse_source("joy:hat0") && !in::parse_source("wheel:steer"));
}

TEST(the_bindings_file_writes_back_to_itself) {
  in::ActionMap m = ride_map();
  m.contexts[1].bindings[2].shape.inner = 0.1f;
  m.contexts[1].bindings[2].shape.curve.exponent = 1.5f;
  m.contexts[1].bindings[2].device = "fff1:0002";
  const std::string text = in::write_bindings(m);
  in::ActionMap back = ride_map();
  std::string error;
  CHECK(in::read_bindings(text, back, error));
  CHECK(in::write_bindings(back) == text);
  CHECK(text.find("bind steer pad:left_x deadzone=0.1,0.98 curve=1.5 device=fff1:0002") !=
        std::string::npos);
  CHECK(text.find("context console blocking off") != std::string::npos);
}

TEST(a_mistake_in_the_bindings_file_names_its_line_and_leaves_the_map_alone) {
  in::ActionMap m = ride_map();
  const size_t before = m.contexts.size();
  std::string error;
  CHECK(!in::read_bindings("ge-bindings 1\ncontext x\nbind fly key:a\n", m, error));
  CHECK(error.find("line 3") != std::string::npos && error.find("steer") != std::string::npos);
  CHECK(!in::read_bindings("ge-bindings 1\ncontext x\nbind steer key:aa\n", m, error));
  CHECK(!in::read_bindings("ge-bindings 1\ncontext x\nbind steer key:a wobble=1\n", m, error));
  CHECK(!in::read_bindings("bind steer key:a\n", m, error));
  CHECK(m.contexts.size() == before);
}

TEST(capturing_takes_the_first_deliberate_input_and_escape_cancels) {
  Rig r;
  in::Capture c;
  in::Source src;
  std::string device;
  in::begin_capture(c, r.s, in::ActionKind::button);
  r.frame();
  CHECK(in::poll_capture(c, r.s, src, device) == in::CaptureResult::waiting);
  in::sim_axis(r.sim, r.pad, static_cast<int>(in::PadAxis::left_x), -0.8f);
  r.frame();
  CHECK(in::poll_capture(c, r.s, src, device) == in::CaptureResult::captured);
  CHECK(in::source_text(src) == "pad:left_x-" && device.empty());

  in::begin_capture(c, r.s, in::ActionKind::button);
  in::sim_key(r.sim, r.kb, in::key::escape, true);
  r.frame();
  CHECK(in::poll_capture(c, r.s, src, device) == in::CaptureResult::cancelled);
}

TEST(a_pedal_resting_at_one_end_is_captured_whole_with_its_device) {
  Rig r;
  const auto wheel = in::sim_plug(r.sim, in::sim_wheel());
  in::sim_axis(r.sim, wheel, 2, 1.0f);  // the pedal at rest reads +1
  r.frame();
  in::Capture c;
  in::Source src;
  std::string device;
  in::begin_capture(c, r.s, in::ActionKind::axis);
  in::sim_axis(r.sim, wheel, 2, -0.4f);
  r.frame();
  CHECK(in::poll_capture(c, r.s, src, device) == in::CaptureResult::captured);
  CHECK(in::source_text(src) == "joy:axis2" && device == "fff2:0001");
}

TEST(conflicts_name_the_actions_a_source_is_on_and_rebind_replaces_a_slot) {
  in::ActionMap m = ride_map();
  const auto w = src_of("key:w");
  const auto names = in::conflicts(m, "ride", w);
  CHECK(names.size() == 3);  // tuck, move, sprint
  CHECK(in::rebind(m, "ride", "jump", 0, src_of("key:e"), ""));
  CHECK(in::conflicts(m, "ride", src_of("key:space")).empty());
  CHECK(in::conflicts(m, "ride", src_of("key:e")).size() == 1);
  CHECK(in::rebind(m, "ride", "jump", 5, src_of("key:j"), ""));  // added
  CHECK(!in::rebind(m, "ride", "fly", 0, w, ""));
}
