// The player's keys and mouse (controls.hpp).
//
// ge:summary The player's keys and mouse as the view's turn and the server's command
// ge:keywords controls read_controls turn_view buttons default_binds key_moves
// ge:skill input shooter
// ge:test tests/shooters_edge_listen_test.cpp
#include "shooters_edge/client/controls.hpp"

#include <algorithm>
#include <string>

#include "console/cvar.hpp"

namespace shooters_edge::client {

namespace {

// Keys by their USB HID usage (the physical key, whatever the layout).
constexpr uint16_t kA = 4, kD = 7, kS = 22, kW = 26, kSpace = 44, kOne = 30;

bool held(const ge::input::InputState& in, uint16_t k) { return ge::input::key_held(in, k); }

bool down(const ge::console::Button* b) { return b != nullptr && b->down(); }

// The default layout: a key, the command it runs.
constexpr const char* kDefaults[][2] = {
    {"w", "+forward"},          {"s", "+back"},
    {"a", "+moveleft"},         {"d", "+moveright"},
    {"up", "+forward"},         {"down", "+back"},
    {"left", "+left"},          {"right", "+right"},
    {",", "+moveleft"},         {".", "+moveright"},
    {"space", "+jump"},         {"mouse1", "+attack"},
    {"ctrl", "+attack"},        {"mouse2", "+jump"},
    {"shift", "+speed"},        {"alt", "+strafe"},
    {"1", "impulse 1"},         {"2", "impulse 2"},
    {"3", "impulse 3"},         {"4", "impulse 4"},
    {"5", "impulse 5"},         {"6", "impulse 6"},
    {"7", "impulse 7"},         {"8", "impulse 8"},
    {"mwheelup", "impulse 10"}, {"mwheeldown", "impulse 10"},
};

}  // namespace

void add_cvars(ge::console::Console& con, ControlSettings& c) {
  con.cvars().bind({.name = "sensitivity",
                    .fallback = "0.066",
                    .flags = ge::console::kArchive,
                    .help = "the mouse's turn, degrees a count",
                    .min = 0.001,
                    .max = 2},
                   &c.sensitivity);
  con.cvars().bind({.name = "m_invert",
                    .fallback = "0",
                    .flags = ge::console::kArchive,
                    .help = "the mouse's pitch upside down (0 or 1)",
                    .kind = ge::console::CvarKind::boolean},
                   &c.invert);
  con.cvars().bind({.name = "lookspring",
                    .fallback = "0",
                    .flags = ge::console::kArchive,
                    .help = "1: the view's pitch centres (follows the slope) as you move",
                    .kind = ge::console::CvarKind::boolean},
                   &c.lookspring);
  con.cvars().bind({.name = "cl_run",
                    .fallback = "1",
                    .flags = ge::console::kArchive,
                    .help = "1: always run (+speed walks), 0: walk (+speed runs)",
                    .kind = ge::console::CvarKind::boolean},
                   &c.run);
  con.cvars().bind({.name = "lookstrafe",
                    .fallback = "0",
                    .flags = ge::console::kArchive,
                    .help = "1: the mouse's sideways moves sidestep instead of turning (as "
                            "+strafe held does)",
                    .kind = ge::console::CvarKind::boolean},
                   &c.lookstrafe);
  con.cvars().bind({.name = "cl_yawspeed",
                    .fallback = "140",
                    .flags = ge::console::kArchive,
                    .help = "how fast +left and +right turn, degrees a second",
                    .min = 0,
                    .max = 1000},
                   &c.yaw_speed);
  ControlButtons& b = c.buttons;
  b.forward = &con.add_button("forward", "+forward: move forward while held");
  b.back = &con.add_button("back", "+back: move back while held");
  b.moveleft = &con.add_button("moveleft", "+moveleft: step left while held");
  b.moveright = &con.add_button("moveright", "+moveright: step right while held");
  b.jump = &con.add_button("jump", "+jump: jump (and swim up) while held");
  b.attack = &con.add_button("attack", "+attack: fire while held");
  b.speed = &con.add_button("speed", "+speed: walk while held (run with cl_run 0)");
  b.strafe = &con.add_button("strafe", "+strafe: the mouse and the turn keys sidestep while held");
  b.left = &con.add_button("left", "+left: turn left while held (cl_yawspeed)");
  b.right = &con.add_button("right", "+right: turn right while held (cl_yawspeed)");
  con.add_command("impulse", "impulse <n>: send the mod impulse n (1..8 a weapon, 10 the next)",
                  [&c](ge::console::Console& k, const ge::console::Args& a) {
                    double n = 0;
                    if (a.size() < 2 || !ge::console::parse_number(a[1], n) || n < 1 || n > 255)
                      k.print("impulse: impulse <1..255> (1..8 a weapon, 10 the next one)");
                    else
                      c.buttons.impulse = static_cast<int>(n);
                  });
}

void default_binds(ge::console::Console& con) {
  for (const auto& [key, command] : kDefaults)
    if (con.binds().get(ge::console::BindMode::game, key).empty())
      con.binds().set(ge::console::BindMode::game, key, command);
}

Moves button_moves(ControlSettings& c) {
  const ControlButtons& b = c.buttons;
  Moves m;
  m.forward = down(b.forward);
  m.back = down(b.back);
  m.moveleft = down(b.moveleft);
  m.moveright = down(b.moveright);
  m.jump = down(b.jump);
  m.attack = down(b.attack);
  m.speed = down(b.speed);
  m.strafe = down(b.strafe);
  m.left = down(b.left);
  m.right = down(b.right);
  m.impulse = b.impulse;
  c.buttons.impulse = 0;
  return m;
}

void release_buttons(ControlSettings& c) {
  ControlButtons& b = c.buttons;
  for (ge::console::Button* p : {b.forward, b.back, b.moveleft, b.moveright, b.jump, b.attack,
                                 b.speed, b.strafe, b.left, b.right})
    if (p != nullptr) p->held = 0;
  b.impulse = 0;
}

Moves key_moves(const ge::input::InputState& in) {
  Moves m;
  m.forward = held(in, kW);
  m.back = held(in, kS);
  m.moveright = held(in, kD);
  m.moveleft = held(in, kA);
  m.jump = held(in, kSpace);
  m.attack = in.mouse.buttons.down(0);
  for (int k = 0; k < 8; k++)
    if (ge::input::key_pressed(in, static_cast<uint16_t>(kOne + k))) m.impulse = k + 1;
  if (in.mouse.wheel_y != 0) m.impulse = 10;
  return m;
}

void spring_view(const ge::input::InputState& in, const ControlSettings& c, bool on_ground,
                 const ge::shooter::MoveCmd& cmd, float ideal, double dt,
                 ge::shooter::Vec3f& view) {
  if (!c.lookspring || !on_ground || in.mouse.dy != 0) return;
  if (cmd.forward == 0 && cmd.side == 0) return;
  const double most = kSpringSpeed * dt;
  const double gap = static_cast<double>(ideal) - view.x;
  view.x = static_cast<float>(view.x + std::clamp(gap, -most, most));
}

void turn_view(const ge::input::InputState& in, const Moves& m, const ControlSettings& c, double dt,
               ge::shooter::Vec3f& view) {
  const bool strafe = m.strafe || c.lookstrafe;
  const double dy = in.mouse.dy * c.sensitivity * (c.invert ? -1 : 1);
  if (!strafe) view.y = static_cast<float>(view.y - in.mouse.dx * c.sensitivity);
  if (!m.strafe) {  // +strafe makes the turn keys sidestep (read_controls)
    const double turn = c.yaw_speed * dt * ((m.left ? 1 : 0) - (m.right ? 1 : 0));
    view.y = static_cast<float>(view.y + turn);
  }
  view.x = std::clamp(static_cast<float>(view.x + dy), -c.pitch_limit, c.pitch_limit);
  while (view.y >= 360) view.y -= 360;
  while (view.y < 0) view.y += 360;
}

void turn_view(const ge::input::InputState& in, const ControlSettings& c,
               ge::shooter::Vec3f& view) {
  turn_view(in, key_moves(in), c, 0, view);
}

ge::shooter::MoveCmd read_controls(const Moves& m, const ge::input::InputState& in,
                                   const ControlSettings& c) {
  ge::shooter::MoveCmd cmd;
  // cl_run on: run, +speed walks; off: walk, +speed runs. Walking is half the run.
  const bool run = c.run != m.speed;
  const int fs = run ? c.forward_speed : c.forward_speed / 2;
  const int ss = run ? c.side_speed : c.side_speed / 2;
  int f = 0, s = 0;
  if (m.forward) f += fs;
  if (m.back) f -= fs;
  if (m.moveright) s += ss;
  if (m.moveleft) s -= ss;
  if (m.strafe) {  // the turn keys sidestep
    if (m.right) s += ss;
    if (m.left) s -= ss;
  }
  if (m.strafe || c.lookstrafe) {
    const double side = in.mouse.dx * c.sensitivity * c.strafe_scale;
    s = std::clamp(s + static_cast<int>(side), -c.side_speed, c.side_speed);
  }
  cmd.forward = static_cast<int16_t>(f);
  cmd.side = static_cast<int16_t>(s);
  if (m.attack) cmd.buttons |= ge::shooter::kButtonAttack;
  if (m.jump) cmd.buttons |= ge::shooter::kButtonJump;
  cmd.impulse = static_cast<uint8_t>(m.impulse);
  return cmd;
}

ge::shooter::MoveCmd read_controls(const ge::input::InputState& in, const ControlSettings& c) {
  return read_controls(key_moves(in), in, c);
}

}  // namespace shooters_edge::client
