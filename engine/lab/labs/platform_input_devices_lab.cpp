// LAB `input-devices`: the platform's input backend (engine/platform/devices.hpp) against the
// simulated one (engine/input/sim.hpp). One script moves a gamepad and a wheel with pedals and
// paddles; it is played through SDL3's virtual devices (SDL's own mapping and events, the
// translation, the device table) and through the simulated backend, and a skier's and a racer's
// bindings read both. What the game is handed must be the same whichever backend fed it, to
// the 16-bit step a real axis reports in. Then the way back: a rumble, the light and the player
// lights sent to the virtual pad, and a pad unplugged and plugged in again mid-session.
//
// ge:summary Lab: the SDL backend against the simulated one on one script (pad, wheel), feedback
// back ge:keywords lab input-devices platform sdl virtual-joystick backend parity gamepad wheel
// ge:keywords hot-plug rumble pedals paddles role
// ge:skill input labs
// ge:test tests/platform_devices_test.cpp
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

#include "core/libm/libm.hpp"
#include "input/actions.hpp"
#include "input/bindings_text.hpp"
#include "input/device_db.hpp"
#include "input/feedback.hpp"
#include "input/sim.hpp"
#include "lab/chart.hpp"
#include "lab/lab.hpp"
#include "platform/devices.hpp"

namespace {

namespace gl = ge::lab;
namespace in = ge::input;
namespace pl = ge::platform;

constexpr const char* kSkier = R"(ge-bindings 1
context ride
bind steer pad:left_x deadzone=0.12,0.97
bind tuck pad:right_trigger
bind brake pad:left_trigger
bind jump pad:south
)";

constexpr const char* kRacer = R"(ge-bindings 1
context drive
bind steer role:steer
bind throttle role:throttle
bind brake role:brake
bind clutch role:clutch
bind shift_up role:shift_up
bind shift_down role:shift_down
)";

// What the script does to a device; each backend fills it in.
struct Hands {
  std::function<void(int dev, int axis, float v)> axis;
  std::function<void(int dev, int button, bool down)> button;
};
constexpr int kPad = 0, kWheel = 1;

// THE SCRIPT, at time t: the pad's stick swept round and its triggers squeezed; the wheel turned
// lock to lock, the throttle floored, the brake and clutch, the paddles pulled.
void script(double t, double dt, const Hands& h) {
  const auto at = [&](double when) { return t <= when && when < t + dt; };
  const double pi = 3.141592653589793;
  if (t >= 0.2 && t < 1.2) {
    const double a = (t - 0.2) * 2 * pi;
    h.axis(kPad, static_cast<int>(in::PadAxis::left_x),
           static_cast<float>(0.9 * ge::core::libm::sin(a)));
    h.axis(kPad, static_cast<int>(in::PadAxis::left_y),
           static_cast<float>(0.9 * ge::core::libm::cos(a)));
  }
  if (at(1.2)) {
    h.axis(kPad, static_cast<int>(in::PadAxis::left_x), 0);
    h.axis(kPad, static_cast<int>(in::PadAxis::left_y), 0);
  }
  if (t >= 0.4 && t < 1.0) {
    h.axis(kPad, static_cast<int>(in::PadAxis::right_trigger), static_cast<float>((t - 0.4) / 0.6));
  }
  if (at(1.0)) h.axis(kPad, static_cast<int>(in::PadAxis::right_trigger), 0);
  if (at(0.7)) h.button(kPad, static_cast<int>(in::PadButton::south), true);
  if (at(0.8)) h.button(kPad, static_cast<int>(in::PadButton::south), false);
  if (t >= 0.3 && t < 1.5) {
    // 0.95 of full lock: no frame lands on an axis-as-button's 0.5 threshold exactly, where the
    // 16-bit step would put one backend either side of it.
    h.axis(kWheel, 0, static_cast<float>(0.95 * ge::core::libm::sin((t - 0.3) / 1.2 * 2 * pi)));
  }
  if (at(1.5)) h.axis(kWheel, 0, 0);
  if (at(0.5)) h.axis(kWheel, 1, -1);  // the throttle floored (the pedals rest at +1)
  if (at(1.1)) h.axis(kWheel, 1, 1);
  if (at(1.1)) h.axis(kWheel, 2, -0.4f);
  if (at(1.4)) h.axis(kWheel, 2, 1);
  if (at(0.9)) h.axis(kWheel, 3, -1);
  if (at(1.0)) h.axis(kWheel, 3, 1);
  if (at(0.95)) h.button(kWheel, 4, true);
  if (at(1.05)) h.button(kWheel, 4, false);
}

struct Read {
  in::ActionMap map;
  in::ActionValues v;
};

Read read_map(gl::Lab& lab, std::vector<in::ActionDef> defs, const char* text) {
  Read r;
  r.map.defs = std::move(defs);
  std::string error;
  if (!in::read_bindings(text, r.map, error)) {
    lab.fail("the lab's bindings do not read: " + error,
             "Fix them in engine/lab/labs/platform_input_devices_lab.cpp (skill: input).");
  }
  r.v = in::make_values(r.map);
  return r;
}

std::string fmt(double v, const char* f = "%.5f") {
  char b[32];
  std::snprintf(b, sizeof b, f, v);
  return b;
}

void run(gl::Lab& lab) {
  const double dt = 1.0 / lab.arg_num("fps");
  const int frames = static_cast<int>(lab.arg_num("seconds") / dt);
  const std::vector<in::ActionDef> skier_defs = {{"steer", in::ActionKind::axis, false, "turn"},
                                                 {"tuck", in::ActionKind::axis, true, "tuck"},
                                                 {"brake", in::ActionKind::axis, true, "skid"},
                                                 {"jump", in::ActionKind::button, false, "pop"}};
  const std::vector<in::ActionDef> racer_defs = {
      {"steer", in::ActionKind::axis, false, "turn"},
      {"throttle", in::ActionKind::axis, true, "accelerate"},
      {"brake", in::ActionKind::axis, true, "slow"},
      {"clutch", in::ActionKind::axis, true, "disengage"},
      {"shift_up", in::ActionKind::button, false, "next gear"},
      {"shift_down", in::ActionKind::button, false, "last gear"}};
  const in::DeviceInfo wheel_info = in::sim_wheel();
  in::DeviceDb db;
  db.put({wheel_info.key,
          in::DeviceKind::wheel,
          "the lab's wheel",
          900,
          {{in::Role::steer, 0, -1, 1},
           {in::Role::throttle, 1, 1, -1},
           {in::Role::brake, 2, 1, -1},
           {in::Role::clutch, 3, 1, -1}},
          {{in::Role::shift_up, 4}, {in::Role::shift_down, 5}}});
  in::Sources from;
  from.db = &db;

  // THE SIMULATED BACKEND.
  in::SimInput sim;
  in::InputState sim_s;
  const in::DeviceId sim_ids[2] = {in::sim_plug(sim, in::sim_pad(in::Family::xbox)),
                                   in::sim_plug(sim, wheel_info)};
  for (int a = 1; a <= 3; a++) in::sim_axis(sim, sim_ids[kWheel], a, 1.0f);
  const Hands sim_hands{
      [&](int dev, int axis, float v) { in::sim_axis(sim, sim_ids[dev], axis, v); },
      [&](int dev, int b, bool down) { in::sim_button(sim, sim_ids[dev], b, down); }};

  // THE PLATFORM BACKEND, over SDL's virtual devices.
  pl::DevicesPtr dev = pl::open_devices({.sensors = false, .haptics = false, .keyboard = false});
  if (!dev) {
    lab.fail("the platform's input devices would not open",
             "Read the error line above and engine/platform/devices.cpp (skill: input).");
    return;
  }
  const uint32_t handles[2] = {
      pl::attach_virtual(*dev, {.kind = in::DeviceKind::gamepad, .name = "lab pad"}),
      pl::attach_virtual(*dev, {.kind = in::DeviceKind::wheel,
                                .name = "lab wheel",
                                .vendor = wheel_info.vendor,
                                .product = wheel_info.product,
                                .axes = wheel_info.axes,
                                .buttons = wheel_info.buttons,
                                .hats = wheel_info.hats})};
  for (int a = 1; a <= 3; a++) pl::virtual_axis(*dev, handles[kWheel], a, 1.0f);
  in::Queue q;
  in::InputState sdl_s;
  const Hands sdl_hands{
      [&](int d, int axis, float v) { pl::virtual_axis(*dev, handles[d], axis, v); },
      [&](int d, int b, bool down) { pl::virtual_button(*dev, handles[d], b, down); }};

  Read sims[2] = {read_map(lab, skier_defs, kSkier), read_map(lab, racer_defs, kRacer)};
  Read sdls[2] = {read_map(lab, skier_defs, kSkier), read_map(lab, racer_defs, kRacer)};
  const char* game_names[2] = {"skier", "racer"};

  int differing = 0, plugged_at = -1;
  std::vector<double> worst[2] = {std::vector<double>(skier_defs.size(), 0),
                                  std::vector<double>(racer_defs.size(), 0)};
  int presses[2] = {0, 0};
  gl::Trace sim_steer{"racer steer, simulated", {}, {}}, sdl_steer{"racer steer, SDL", {}, {}},
      sim_pad{"skier steer, simulated", {}, {}}, sdl_pad{"skier steer, SDL", {}, {}};
  for (int f = 0; f < frames; f++) {
    const double t = f * dt;
    script(t, dt, sim_hands);
    script(t, dt, sdl_hands);
    in::sim_frame(sim, sim_s);
    q.clear();
    pl::pump_devices(*dev, q);
    in::update(sdl_s, q);
    if (plugged_at < 0 && pl::virtual_id(*dev, handles[kPad]) &&
        pl::virtual_id(*dev, handles[kWheel])) {
      plugged_at = f;
    }
    bool differs = false;
    for (int g = 0; g < 2; g++) {
      in::evaluate(sims[g].map, sim_s, dt, sims[g].v, from);
      in::evaluate(sdls[g].map, sdl_s, dt, sdls[g].v, from);
      for (size_t a = 0; a < sims[g].v.v.size(); a++) {
        const in::ActionValue& x = sims[g].v.v[a];
        const in::ActionValue& y = sdls[g].v.v[a];
        const double d = std::max(std::fabs(static_cast<double>(x.x) - y.x),
                                  std::fabs(static_cast<double>(x.y) - y.y));
        worst[g][a] = std::max(worst[g][a], d);
        if (d > 1e-3 || x.held != y.held || x.pressed != y.pressed) differs = true;
        if (y.pressed) presses[g]++;
      }
    }
    if (differs) differing++;
    sim_steer.x.push_back(t);
    sim_steer.y.push_back(sims[1].v.v[0].x);
    sdl_steer.x.push_back(t);
    sdl_steer.y.push_back(sdls[1].v.v[0].x);
    sim_pad.x.push_back(t);
    sim_pad.y.push_back(sims[0].v.v[0].x);
    sdl_pad.x.push_back(t);
    sdl_pad.y.push_back(sdls[0].v.v[0].x);
    in::sim_advance(sim, dt);
  }
  lab.metric("parity.frames", frames, "count");
  lab.metric("parity.frames-differing", differing, "count");
  lab.metric("parity.plugged-after-frames", plugged_at, "count");
  gl::LabTable table{"the largest gap between the backends, each action",
                     {"game", "action", "largest |sim - SDL|", "presses (SDL)"},
                     {}};
  for (int g = 0; g < 2; g++) {
    const auto& defs = g == 0 ? skier_defs : racer_defs;
    for (size_t a = 0; a < defs.size(); a++) {
      table.rows.push_back({game_names[g], defs[a].name, fmt(worst[g][a]), ""});
      lab.metric(std::string(game_names[g]) + "." + defs[a].name + ".gap", worst[g][a], "");
    }
    table.rows.back()[3] = std::to_string(presses[g]);
  }
  lab.table(table);
  if (differing > 0) {
    lab.fail(std::to_string(differing) +
                 " frames hand the game different actions from the SDL "
                 "backend than from the simulated one",
             "Read the table above for the action and engine/platform/devices.cpp's translation "
             "of that device (skill: input).");
  }

  gl::LineChart chart;
  chart.frame = {"ONE SCRIPT, TWO BACKENDS", "SDL's virtual devices against the simulated ones",
                 "t (s)", "action value"};
  chart.spikes = false;
  chart.traces = {sim_steer, sdl_steer, sim_pad, sdl_pad};
  const gl::Chart drawn = gl::line_chart(chart);
  lab.shot({"parity",
            drawn.title,
            "The racer's steer from the wheel's role and the skier's steer from the pad's stick, "
            "each read off the simulated backend and off SDL's virtual devices.",
            "each SDL line lies on its simulated line: one sine for the wheel lock to lock, one "
            "for the stick swept round, with the skier's flat through the dead zone",
            {{"fps", lab.arg("fps")}, {"seconds", lab.arg("seconds")}}},
           gl::to_picture(drawn.plot));

  // THE WAY BACK: feedback reaches the virtual pad through SDL, as it would a real one.
  const in::DeviceId pad = pl::virtual_id(*dev, handles[kPad]);
  in::Feedback fb;
  in::rumble(fb, pad, 0.8f, 0.3f, 0.25, 0);
  in::set_light(fb, pad, 255, 64, 0);
  fb.get(pad)->player = 1;
  pl::play_feedback(*dev, fb, sdl_s, 0);
  const pl::VirtualFeedback on = pl::virtual_feedback(*dev, handles[kPad]);
  pl::play_feedback(*dev, fb, sdl_s, 0.3);
  const pl::VirtualFeedback off = pl::virtual_feedback(*dev, handles[kPad]);
  gl::LabTable back{"what the pad was told", {"when", "low", "high", "light", "player"}, {}};
  const auto row = [](const char* when, const pl::VirtualFeedback& v) {
    return std::vector<std::string>{
        when, std::to_string(v.low), std::to_string(v.high),
        v.light ? std::to_string(v.r) + "," + std::to_string(v.g) + "," + std::to_string(v.b) : "-",
        std::to_string(v.player)};
  };
  back.rows = {row("rumble 0.8 / 0.3 for 0.25 s", on), row("0.3 s later", off)};
  lab.table(back);
  lab.metric("feedback.low", on.low, "");
  lab.metric("feedback.high", on.high, "");
  lab.metric("feedback.after", off.low + off.high, "");
  if (on.low != 52428 || on.high != 19661 || off.low != 0 || off.high != 0 || on.player != 1) {
    lab.fail("the pad was not told the rumble, its end or its player light as asked",
             "Read play_feedback in engine/platform/devices.cpp (skill: input).");
  }

  // HOT-PLUG: the pad unplugged and plugged in again is a new device with a new id.
  pl::detach_virtual(*dev, handles[kPad]);
  q.clear();
  pl::pump_devices(*dev, q);
  in::update(sdl_s, q);
  const int removed = sdl_s.removed_n;
  const uint32_t again =
      pl::attach_virtual(*dev, {.kind = in::DeviceKind::gamepad, .name = "lab pad"});
  q.clear();
  pl::pump_devices(*dev, q);
  in::update(sdl_s, q);
  const in::DeviceId new_id = pl::virtual_id(*dev, again);
  lab.metric("hotplug.removed", removed, "count");
  lab.metric("hotplug.added", sdl_s.added_n, "count");
  lab.metric("hotplug.new-id", new_id != 0 && new_id != pad, "");
  lab.note("hot-plug: the pad's removal was " + std::to_string(removed) +
           " device_removed; plugged in again it came back as a new device (" +
           (new_id != 0 && new_id != pad ? "a new id" : "NOT a new id") + ")");
}

const gl::LabSpec kSpec{
    "input-devices",
    gl::LabKind::scenario,
    "engine",
    "engine/platform/devices.hpp",
    "input",
    "Does the platform's backend hand a game the same actions as the simulated one, and does "
    "feedback reach the device?",
    {
        {"fps", "120", "frames a second"},
        {"seconds", "1.6", "how long the script runs"},
    },
    "--seconds=1.6",
    run,
};
GE_LAB(kSpec);

}  // namespace
