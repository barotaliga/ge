// The client's command rate, lengths, key shares and grids (usercmd.hpp states the rules).
//
// ge:summary The client's command clock, key shares, axis and angle grids, the command built
// ge:keywords create_cmd_clock cmd_due next_msec key_share quantize_axis quantize_angle build_cmd
// ge:skill shooter input
// ge:test tests/shooter_usercmd_test.cpp
#include "shooter/usercmd.hpp"

#include <cmath>
#include <initializer_list>

#include "core/num.hpp"

namespace ge::shooter {

namespace {

constexpr double kMaxMsec = 250;
constexpr uint8_t kLongMsec = 100;  // what a command over kMaxMsec becomes
constexpr double kGridPerDeg = 65536.0 / 360;

void clear_edges(KeyState& k) {
  k.pressed = false;
  k.released = false;
}

bool any_down(const KeyState& k) { return k.down || k.pressed || k.released; }

}  // namespace

void key_press(KeyState& k) {
  if (k.down) return;
  k.down = true;
  k.pressed = true;
}

void key_release(KeyState& k) {
  if (!k.down) return;
  k.down = false;
  k.released = true;
}

float key_share(const KeyState& k, bool look_key) {
  if (k.down) {
    if (k.pressed && k.released) return 0.75f;  // released and pressed again
    if (k.pressed) return look_key ? 0.5f : 1.0f;
    return 1.0f;  // held throughout
  }
  if (k.pressed && k.released) return 0.25f;  // pressed and released within the period
  return 0.0f;                                // released during it, or never down
}

CmdClock create_cmd_clock(int server_maxfps, int player_rate) {
  int rate = server_maxfps;
  if (player_rate > 0 && player_rate < rate) rate = player_rate;
  if (rate < kMinCmdRate) rate = kMinCmdRate;
  CmdClock c;
  c.period = 1.0 / rate;
  return c;
}

bool cmd_due(CmdClock& c, double frame_s) {
  c.acc += frame_s;
  if (c.acc < c.period) return false;
  if (c.acc > 2 * c.period)
    c.acc = 0;
  else
    c.acc -= c.period;
  return true;
}

uint8_t next_msec(CmdClock& c) {
  c.ms_total += c.period * 1000;
  const double whole = std::floor(c.ms_total);
  c.ms_total -= whole;
  if (whole > kMaxMsec) return kLongMsec;
  return static_cast<uint8_t>(whole);
}

float quantize_angle(float deg) {
  const auto q = static_cast<int32_t>(core::num::round(static_cast<double>(deg) * kGridPerDeg));
  const auto s = static_cast<int16_t>(static_cast<uint16_t>(q & 0xffff));
  return static_cast<float>(s / kGridPerDeg);
}

int16_t quantize_axis(float v) {
  double q = std::floor(static_cast<double>(v) / kAxisGrid) * kAxisGrid;
  if (q > kAxisMax) q = kAxisMax;
  if (q < -kAxisMax) q = -kAxisMax;
  return static_cast<int16_t>(q);
}

MoveCmd build_cmd(MoveKeys& k, Vec3f view, uint8_t msec, uint8_t impulse,
                  const MoveSpeeds& speeds) {
  float forward = speeds.forward * key_share(k.forward) - speeds.back * key_share(k.back);
  float side = speeds.side * key_share(k.moveright) - speeds.side * key_share(k.moveleft);
  float up = speeds.up * key_share(k.moveup) - speeds.up * key_share(k.movedown);
  if (k.speed.down) {
    forward = forward * speeds.speedkey;
    side = side * speeds.speedkey;
    up = up * speeds.speedkey;
  }
  MoveCmd cmd;
  cmd.msec = msec;
  cmd.angles = {quantize_angle(view.x), quantize_angle(view.y), quantize_angle(view.z)};
  cmd.forward = quantize_axis(forward);
  cmd.side = quantize_axis(side);
  cmd.up = quantize_axis(up);
  cmd.buttons = static_cast<uint8_t>(
      (any_down(k.attack) ? kButtonAttack : 0) | (any_down(k.jump) ? kButtonJump : 0) |
      (any_down(k.use) ? kButtonUse : 0) | (any_down(k.attack2) ? kButtonAttack2 : 0));
  cmd.impulse = impulse;
  for (KeyState* key : {&k.forward, &k.back, &k.moveleft, &k.moveright, &k.moveup, &k.movedown,
                        &k.speed, &k.attack, &k.jump, &k.use, &k.attack2})
    clear_edges(*key);
  return cmd;
}

}  // namespace ge::shooter
