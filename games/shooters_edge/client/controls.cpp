// The player's keys and mouse (controls.hpp).
//
// ge:summary The player's keys and mouse as the view's turn and the server's command
// ge:keywords controls read_controls turn_view
// ge:skill input shooter
// ge:test tests/shooters_edge_listen_test.cpp
#include "shooters_edge/client/controls.hpp"

#include <algorithm>

namespace shooters_edge::client {

namespace {

// Keys by their USB HID usage (the physical key, whatever the layout).
constexpr uint16_t kA = 4, kD = 7, kS = 22, kW = 26, kSpace = 44, kOne = 30;

bool held(const ge::input::InputState& in, uint16_t k) { return ge::input::key_held(in, k); }

}  // namespace

void turn_view(const ge::input::InputState& in, const ControlSettings& c,
               ge::shooter::Vec3f& view) {
  const double dy = in.mouse.dy * c.sensitivity * (c.invert ? -1 : 1);
  view.y = static_cast<float>(view.y - in.mouse.dx * c.sensitivity);
  view.x = std::clamp(static_cast<float>(view.x + dy), -c.pitch_limit, c.pitch_limit);
  while (view.y >= 360) view.y -= 360;
  while (view.y < 0) view.y += 360;
}

ge::shooter::MoveCmd read_controls(const ge::input::InputState& in, const ControlSettings& c) {
  ge::shooter::MoveCmd cmd;
  int f = 0, s = 0;
  if (held(in, kW)) f += c.forward_speed;
  if (held(in, kS)) f -= c.forward_speed;
  if (held(in, kD)) s += c.side_speed;
  if (held(in, kA)) s -= c.side_speed;
  cmd.forward = static_cast<int16_t>(f);
  cmd.side = static_cast<int16_t>(s);
  if (in.mouse.buttons.down(0)) cmd.buttons |= ge::shooter::kButtonAttack;
  if (held(in, kSpace)) cmd.buttons |= ge::shooter::kButtonJump;
  for (int k = 0; k < 8; k++)
    if (ge::input::key_pressed(in, static_cast<uint16_t>(kOne + k)))
      cmd.impulse = static_cast<uint8_t>(k + 1);
  if (in.mouse.wheel_y != 0) cmd.impulse = 10;
  return cmd;
}

}  // namespace shooters_edge::client
