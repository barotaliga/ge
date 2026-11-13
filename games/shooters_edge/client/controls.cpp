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
#include "shooter/rjump_assist.hpp"
#include "shooter/usercmd.hpp"

namespace shooters_edge::client {

namespace {

// Keys by their USB HID usage (the physical key, whatever the layout).
constexpr uint16_t kA = 4, kD = 7, kS = 22, kW = 26, kSpace = 44, kOne = 30;

bool held(const ge::input::InputState& in, uint16_t k) { return ge::input::key_held(in, k); }

bool down(const ge::console::Button* b) { return b != nullptr && b->down(); }

// The default layout: a key, the command it runs (+voice talks, console/talk.hpp binding the
// other chat and voice keys: net/classic_remote.hpp; screenshot: engine/capture/screenshot.hpp).
constexpr const char* kDefaults[][2] = {
    {"w", "+forward"},
    {"s", "+back"},
    {"a", "+moveleft"},
    {"d", "+moveright"},
    {"up", "+forward"},
    {"down", "+back"},
    {"left", "+left"},
    {"right", "+right"},
    {",", "+moveleft"},
    {".", "+moveright"},
    {"space", "+jump"},
    {"mouse1", "+attack"},
    {"ctrl", "+attack"},
    {"mouse2", "+jump"},
    {"alt", "+strafe"},
    {"1", "impulse 1"},
    {"2", "impulse 2"},
    {"3", "impulse 3"},
    {"4", "impulse 4"},
    {"5", "impulse 5"},
    {"6", "impulse 6"},
    {"7", "impulse 7"},
    {"8", "impulse 8"},
    {"mwheelup", "impulse 10"},
    {"mwheeldown", "impulse 10"},
    {"v", "+voice"},
    {"f12", "screenshot"},
};

}  // namespace

void add_cvars(ge::console::Console& con, ControlSettings& c) {
  con.cvars().bind({.name = "sensitivity",
                    .fallback = "0.066",
                    .flags = ge::console::kArchive,
                    .help = "The mouse's turn, in degrees a count.",
                    .min = 0.001,
                    .max = 2,
                    .values = "0.001..2: that many degrees (0.066 the default)"},
                   &c.sensitivity);
  con.cvars().bind({.name = "m_invert",
                    .fallback = "0",
                    .flags = ge::console::kArchive,
                    .help = "Whether the mouse's pitch is upside down.",
                    .kind = ge::console::CvarKind::boolean,
                    .values = "0: forward looks down (the default); 1: forward looks up"},
                   &c.invert);
  con.cvars().bind({.name = "lookspring",
                    .fallback = "0",
                    .flags = ge::console::kArchive,
                    .help = "Whether the view's pitch centres, following the slope, as you move.",
                    .kind = ge::console::CvarKind::boolean,
                    .values = "0: it stays (the default); 1: it centres"},
                   &c.lookspring);
  con.cvars().bind({.name = "cl_run",
                    .fallback = "1",
                    .flags = ge::console::kArchive,
                    .help = "Whether you always run.",
                    .kind = ge::console::CvarKind::boolean,
                    .values = "0: walk, and +speed runs; 1: run, and +speed walks (the default)"},
                   &c.run);
  con.cvars().bind(
      {.name = "lookstrafe",
       .fallback = "0",
       .flags = ge::console::kArchive,
       .help =
           "Whether the mouse's sideways moves sidestep instead of turning, as +strafe held does.",
       .kind = ge::console::CvarKind::boolean,
       .values = "0: they turn (the default); 1: they sidestep"},
      &c.lookstrafe);
  con.cvars().bind(
      {.name = "in_mouse_release",
       .fallback = "5",
       .flags = ge::console::kArchive,
       .help = "What frees the mouse while the window has the focus, as bits added together.",
       .kind = ge::console::CvarKind::integer,
       .min = 0,
       .max = 7,
       .values = "0: never; 1: the console; 2: a menu, its pointer then hovering and clicking; 4: "
                 "a demo's play bar; 0..7: the sum of those (5 the default)"},
      &c.mouse_release);
  con.cvars().bind({.name = "cl_yawspeed",
                    .fallback = "140",
                    .flags = ge::console::kArchive,
                    .help = "How fast +left and +right turn, in degrees a second.",
                    .min = 0,
                    .max = 1000,
                    .values = "0..1000: that many degrees (140 the default)"},
                   &c.yaw_speed);
  ControlButtons& b = c.buttons;
  b.forward = &con.add_button("forward", "+forward: move forward while held");
  b.back = &con.add_button("back", "+back: move back while held");
  b.moveleft = &con.add_button("moveleft", "+moveleft: step left while held");
  b.moveright = &con.add_button("moveright", "+moveright: step right while held");
  b.jump = &con.add_button("jump", "+jump: jump (and swim up) while held");
  b.attack = &con.add_button(
      "attack", "+attack: fire while held (cl_quickweapon 1: the best of cl_quickweapon_prio)");
  b.attack2 = &con.add_button(
      "attack2", "+attack2: fire the best of cl_quickweapon_prio2 while held, then switch back");
  add_quick_cvars(con, c.quick);
  add_group_cvars(con, c.groups);
  b.moveup = &con.add_button("moveup", "+moveup: swim up while held");
  b.movedown = &con.add_button("movedown",
                               "+movedown: swim down while held (crouches in a move that ducks)");
  b.speed = &con.add_button("speed", "+speed: walk while held (run with cl_run 0); unbound");
  b.strafe = &con.add_button("strafe", "+strafe: the mouse and the turn keys sidestep while held");
  b.rjump = &con.add_button("rjump",
                            "+rjump: a rocket jump for you: the rocket fired straight down as you "
                            "jump, where the server allows it (sv_rjump)");
  b.rrjump = &con.add_button("rrjump",
                             "+rrjump: a reverse rocket jump: the rocket fired behind you as you "
                             "jump, where the server allows it (sv_rrjump)");
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
  m.moveup = down(b.moveup);
  m.movedown = down(b.movedown);
  m.jump = down(b.jump);
  m.attack = down(b.attack);
  m.attack2 = down(b.attack2);
  m.speed = down(b.speed);
  m.strafe = down(b.strafe);
  m.left = down(b.left);
  m.right = down(b.right);
  m.impulse = b.impulse;
  c.buttons.impulse = 0;
  // A rocket jump helper's press: its request and the jump in this one command.
  for (ge::console::Button* h : {b.rjump, b.rrjump}) {
    if (h == nullptr || !h->pressed) continue;
    h->pressed = false;
    m.impulse = h == b.rjump ? ge::shooter::kRjumpImpulse : ge::shooter::kBackRjumpImpulse;
    m.jump = true;
  }
  return m;
}

void release_buttons(ControlSettings& c) {
  ControlButtons& b = c.buttons;
  for (ge::console::Button* p :
       {b.forward, b.back, b.moveleft, b.moveright, b.moveup, b.movedown, b.jump, b.attack, b.speed,
        b.strafe, b.left, b.right, b.attack2, b.rjump, b.rrjump})
    if (p != nullptr) p->held = 0, p->pressed = false;
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
  // cl_run on (the default): run, +speed (bound to no key) walks; off: walk, +speed runs. Walking
  // is half the run. Nobody walks: no key is bound to +speed.
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
  // Up and down: swimming in the classic move (bound to no key; SPACE swims up); down crouches in
  // a move that ducks (the sequel's, plugins/shooter/sequel_move.hpp: up below 0 on the ground).
  const int us = run ? c.up_speed : c.up_speed / 2;
  const int u = (m.moveup ? us : 0) - (m.movedown ? us : 0);
  cmd.forward = static_cast<int16_t>(f);
  cmd.side = static_cast<int16_t>(s);
  cmd.up = static_cast<int16_t>(u);
  if (m.attack || m.attack2) cmd.buttons |= ge::shooter::kButtonAttack;
  if (m.jump) cmd.buttons |= ge::shooter::kButtonJump;
  cmd.impulse = static_cast<uint8_t>(m.impulse);
  return cmd;
}

ge::shooter::MoveCmd read_controls(const ge::input::InputState& in, const ControlSettings& c) {
  return read_controls(key_moves(in), in, c);
}

}  // namespace shooters_edge::client
