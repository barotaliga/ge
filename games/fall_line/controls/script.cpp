// A scripted session: the log's axes held on a simulated pad, or on the thumbs.
//
// ge:summary pad_session, touch_session: an input log's axes as a pad's or the thumbs' event log
// ge:keywords fall-line script pad session touch thumbs
// ge:skill input
// ge:test tests/fall_line_controls_test.cpp
#include "fall_line/controls/script.hpp"

#include "fall_line/controls/thumbs.hpp"
#include "fall_line/sim/play.hpp"
#include "input/sim.hpp"

namespace fall_line::controls {

namespace in = ge::input;

const char* const kExactPadBindings = R"(ge-bindings 1
context ride
bind steer pad:left_x deadzone=0,1
bind tuck pad:right_trigger deadzone=0,1
bind brake pad:left_trigger deadzone=0,1
bind jump pad:south
bind reset pad:north
)";

const char* const kExactTouchBindings = R"(ge-bindings 1
context ride
bind steer touch:edge deadzone=0,1
bind tuck touch:lever+ deadzone=0,1
bind brake touch:lever- deadzone=0,1
bind jump touch:load
bind reset touch:reset
)";

in::EventLog pad_session(const ge::play::InputLog& log) {
  in::SimInput sim;
  in::InputState state;
  in::EventLog out;
  const in::DeviceId pad = in::sim_plug(sim, in::sim_pad());
  bool jump = false, reset = false;
  float last[3] = {0, 0, 0};
  const in::PadAxis axes[3] = {in::PadAxis::left_x, in::PadAxis::right_trigger,
                               in::PadAxis::left_trigger};
  for (int64_t i = 0; i < log.steps(); ++i) {
    if (i > 0) in::sim_advance(sim, static_cast<double>(frame_time(i) - sim.t_ns) * 1e-9);
    const auto a = log.at(i);
    for (int k = 0; k < 3; ++k) {
      const auto v = static_cast<float>(a[static_cast<size_t>(k)]);
      if (v != last[k]) in::sim_axis(sim, pad, static_cast<int>(axes[k]), v);
      last[k] = v;
    }
    const bool j = a[3] != 0;
    if (j != jump) in::sim_button(sim, pad, static_cast<int>(in::PadButton::south), j);
    jump = j;
    const bool r = a[4] != 0;
    if (r && reset) in::sim_button(sim, pad, static_cast<int>(in::PadButton::north), false);
    if (r || reset) in::sim_button(sim, pad, static_cast<int>(in::PadButton::north), r);
    in::Queue kept;
    in::sim_frame(sim, state, &kept);
    out.add(frame_time(i), kept);
  }
  return out;
}

namespace {

// Where the thumbs land (of the screen) and their fingers.
constexpr float kEdgeX = 0.25f, kEdgeY = 0.6f;
constexpr float kLeverX = 0.75f, kLeverY = 0.45f;
constexpr float kResetX = 0.06f, kResetY = 0.07f;
constexpr uint16_t kEdgeFinger = 1, kLeverFinger = 2, kResetFinger = 3;
// The tap before a jump, in steps before it: off the glass, down, up (a 0.05 s tap 0.15 s ahead).
constexpr int64_t kLift = 30, kTapDown = 24, kTapUp = 18;

enum class Lever : uint8_t { off, lands, held };

}  // namespace

TouchSession touch_session(const ge::play::InputLog& log, float aspect) {
  const int64_t n = log.steps();
  TouchSession out;
  // The jumps, kept where a tap fits before them: the lever's thumb's state at each step.
  std::vector<Lever> lever(static_cast<size_t>(n), Lever::held);
  std::vector<bool> jump(static_cast<size_t>(n), false);
  if (n > 0) lever[0] = Lever::lands;
  int64_t free_from = 0;  // the first step the lever's thumb has been on the glass since
  for (int64_t i = 0; i < n;) {
    if (log.at(i)[3] == 0) {
      ++i;
      continue;
    }
    int64_t end = i;
    while (end < n && log.at(end)[3] != 0) ++end;
    ++out.jumps;
    // The touch before the tap is a long one (never a tap itself): kLift steps or more.
    if (i - kLift - free_from <= kLift) {
      ++out.dropped;
      i = end;
      continue;
    }
    for (int64_t k = i - kLift; k < i - kTapDown; ++k) lever[static_cast<size_t>(k)] = Lever::off;
    lever[static_cast<size_t>(i - kTapDown)] = Lever::lands;
    for (int64_t k = i - kTapUp; k < i; ++k) lever[static_cast<size_t>(k)] = Lever::off;
    lever[static_cast<size_t>(i)] = Lever::lands;
    for (int64_t k = i; k < end; ++k) jump[static_cast<size_t>(k)] = true;
    if (end < n) lever[static_cast<size_t>(end)] = Lever::off;
    if (end + 1 < n) lever[static_cast<size_t>(end + 1)] = Lever::lands;
    free_from = end + 1;
    i = end;
  }

  const std::span<const ge::play::Axis> kinds = sim::kGame.axes;
  in::SimInput sim;
  in::InputState state;
  const in::DeviceId screen = in::sim_plug(sim, in::sim_touch());
  Lever was = Lever::off;
  float edge_x = -1, lever_y = -1;
  for (int64_t i = 0; i < n; ++i) {
    if (i > 0) in::sim_advance(sim, static_cast<double>(frame_time(i) - sim.t_ns) * 1e-9);
    const auto a = log.at(i);
    std::array<double, 5> want{};
    // The edge thumb.
    if (i == 0) {
      in::sim_touch(sim, screen, kEdgeFinger, in::TouchPhase::down, kEdgeX, kEdgeY);
      edge_x = kEdgeX;
    } else {
      const float x = kEdgeX + static_cast<float>(a[0]) * kEdgeReach / aspect;
      if (x != edge_x) in::sim_touch(sim, screen, kEdgeFinger, in::TouchPhase::move, x, kEdgeY);
      edge_x = x;
      want[0] = a[0];
    }
    // The lever's thumb.
    const Lever now = lever[static_cast<size_t>(i)];
    if (now == Lever::off) {
      if (was != Lever::off) {
        in::sim_touch(sim, screen, kLeverFinger, in::TouchPhase::up, kLeverX, lever_y);
      }
    } else if (now == Lever::lands) {
      if (was != Lever::off) {
        in::sim_touch(sim, screen, kLeverFinger, in::TouchPhase::up, kLeverX, lever_y);
      }
      in::sim_touch(sim, screen, kLeverFinger, in::TouchPhase::down, kLeverX, kLeverY);
      lever_y = kLeverY;
      want[1] = 1;
    } else {
      // The tuck down from full over one reach; the brake past the dead band below it.
      const double brake = a[2], tuck = brake > 0 ? 0 : a[1];
      const float y = brake > 0
                          ? kLeverY + (1 + kLeverDead + static_cast<float>(brake)) * kLeverReach
                          : kLeverY + (1 - static_cast<float>(tuck)) * kLeverReach;
      if (y != lever_y) {
        in::sim_touch(sim, screen, kLeverFinger, in::TouchPhase::move, kLeverX, y);
      }
      lever_y = y;
      want[1] = tuck;
      want[2] = brake;
    }
    was = now;
    want[3] = jump[static_cast<size_t>(i)] ? 1 : 0;
    // The reset: a tap of its button on its step.
    const bool r = a[4] != 0;
    if (r) {
      in::sim_touch(sim, screen, kResetFinger, in::TouchPhase::down, kResetX, kResetY);
      in::sim_touch(sim, screen, kResetFinger, in::TouchPhase::up, kResetX, kResetY);
      want[4] = 1;
    }
    for (size_t k = 0; k < want.size(); ++k) want[k] = ge::play::snap_axis(want[k], kinds[k].kind);
    in::Queue kept;
    in::sim_frame(sim, state, &kept);
    out.events.add(frame_time(i), kept);
    out.want.push_back(want);
  }
  return out;
}

}  // namespace fall_line::controls
