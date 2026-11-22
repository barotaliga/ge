// Any shooter's keys and mouse (controls.hpp).
//
// ge:summary Any shooter's keys and mouse: the move buttons, mouse look, cl_run, lookspring
// ge:keywords controls read_controls turn_view buttons key_moves add_control_cvars
// ge:skill input shooter
// ge:test tests/shooters_edge_listen_test.cpp
#include "shooter/controls.hpp"

#include <algorithm>
#include <cstdlib>
#include <string>

#include "console/cvar.hpp"
#include "core/libm/libm.hpp"
#include "core/num_text.hpp"
#include "shooter/rjump_assist.hpp"
#include "shooter/usercmd.hpp"

namespace ge::shooter {

namespace {

// Keys by their USB HID usage (the physical key, whatever the layout).
constexpr uint16_t kA = 4, kD = 7, kS = 22, kW = 26, kSpace = 44, kOne = 30;

bool held(const input::InputState& in, uint16_t k) { return input::key_held(in, k); }

bool down(const console::Button* b) { return b != nullptr && b->down(); }

}  // namespace

void add_control_cvars(console::Console& con, Controls& c) {
  add_mouse_cvars(con.cvars(), c.mouse);
  // +zoom: the lens narrowed while held, the mouse slowed by as much, both put back on release.
  con.add_command("+zoom", "+zoom: narrow the view while held, the mouse slowed to match",
                  [&c](console::Console& k, const console::Args&) {
                    console::Cvars& v = k.cvars();
                    const console::Cvar* fov = v.find("cl_fov");
                    const console::Cvar* sens = v.find("sensitivity");
                    if (c.zoom.on || fov == nullptr || sens == nullptr) return;
                    c.zoom = {.on = true, .fov = fov->value, .sensitivity = sens->value};
                    double was = std::atof(c.zoom.fov.c_str());
                    if (was <= 0) was = 90;  // cl_fov 0: the rig's own 90
                    const double scaled = std::atof(c.zoom.sensitivity.c_str()) * kZoomFov / was;
                    v.set("cl_fov", core::num::to_string(kZoomFov), console::Source::local);
                    // m_autosens on slows the mouse by the lens itself: not twice.
                    if (c.autosens != 0) return;
                    v.set("sensitivity", core::num::to_string(scaled), console::Source::local);
                  });
  con.add_command("-zoom", "-zoom: the view and the mouse as they were before +zoom",
                  [&c](console::Console& k, const console::Args&) {
                    if (!c.zoom.on) return;
                    c.zoom.on = false;
                    k.cvars().set("cl_fov", c.zoom.fov, console::Source::local);
                    k.cvars().set("sensitivity", c.zoom.sensitivity, console::Source::local);
                  });
  con.cvars().bind({.name = "sensitivity",
                    .fallback = "3",
                    .flags = console::kArchive,
                    .help = "The mouse's speed: its turn is this times m_yaw degrees a count.",
                    .min = 0.001,
                    .max = 100,
                    .values = "0.5..100: the speed (3 the default); under 0.5: degrees a count"},
                   &c.sensitivity);
  con.cvars().bind(
      {.name = "m_yaw",
       .fallback = "0.022",
       .flags = console::kArchive,
       .help = "Degrees the mouse turns you a count sideways at sensitivity 1.",
       .min = -1,
       .max = 1,
       .values =
           "0..1: degrees a count (0.022 the default); -1..0: the same, turning the other way"},
      &c.m_yaw);
  con.cvars().bind(
      {.name = "m_pitch",
       .fallback = "0.022",
       .flags = console::kArchive,
       .help = "Degrees the mouse pitches you a count at sensitivity 1; below 0 "
               "flips it.",
       .min = -1,
       .max = 1,
       .values =
           "0..1: degrees a count (0.022 the default); -1..0: the same, pitching the other way"},
      &c.m_pitch);
  con.cvars().bind({.name = "m_autosens",
                    .fallback = "0",
                    .flags = console::kArchive,
                    .help = "Slows the mouse as the view narrows, so a zoomed aim moves as far "
                            "on screen.",
                    .min = 0,
                    .max = 179,
                    .values = "0: off (the default); 91..179: tuned at that field of view; any "
                              "other value: tuned at 90"},
                   &c.autosens);
  con.cvars().bind({.name = "m_invert",
                    .fallback = "0",
                    .flags = console::kArchive,
                    .help = "Whether the mouse's pitch is upside down.",
                    .kind = console::CvarKind::boolean,
                    .values = "0: forward looks down (the default); 1: forward looks up"},
                   &c.invert);
  con.cvars().bind({.name = "lookspring",
                    .fallback = "0",
                    .flags = console::kArchive,
                    .help = "Whether the view's pitch centres when you let mouse look go, and "
                            "follows the slope as you move while looking by keys.",
                    .kind = console::CvarKind::boolean,
                    .values = "0: it stays (the default); 1: it centres"},
                   &c.lookspring);
  con.cvars().bind({.name = "v_centermove",
                    .fallback = "0.15",
                    .flags = console::kArchive,
                    .help = "How long you run forward before a keyboard view drifts level again.",
                    .min = 0,
                    .max = 10,
                    .values = "0..10: that many seconds (0.15 the default)"},
                   &c.center_move);
  con.cvars().bind({.name = "v_centerspeed",
                    .fallback = "500",
                    .flags = console::kArchive,
                    .help = "How fast a keyboard view's drift toward the slope speeds up.",
                    .min = 0,
                    .max = 10000,
                    .values = "0..10000: degrees a second each second (500 the default)"},
                   &c.center_speed);
  con.cvars().bind({.name = "cl_aimhelp_turn",
                    .fallback = "1",
                    .flags = console::kArchive,
                    .help = "Whether your view turns toward the player the aim help helps you hit.",
                    .kind = console::CvarKind::boolean,
                    .values = "0: the view stays where you aim; 1: it turns while you fire and "
                              "comes back after (the default)"},
                   &c.aim_turn);
  con.cvars().bind({.name = "cl_run",
                    .fallback = "1",
                    .flags = console::kArchive,
                    .help = "Whether you always run.",
                    .kind = console::CvarKind::boolean,
                    .values = "0: walk, and +speed runs; 1: run, and +speed walks (the default)"},
                   &c.run);
  con.cvars().bind(
      {.name = "lookstrafe",
       .fallback = "0",
       .flags = console::kArchive,
       .help =
           "Whether the mouse's sideways moves sidestep instead of turning, as +strafe held does.",
       .kind = console::CvarKind::boolean,
       .values = "0: they turn (the default); 1: they sidestep"},
      &c.lookstrafe);
  con.cvars().bind(
      {.name = "in_mouse_release",
       .fallback = "5",
       .flags = console::kArchive,
       .help = "What frees the mouse while the window has the focus, as bits added together.",
       .kind = console::CvarKind::integer,
       .min = 0,
       .max = 7,
       .values = "0: never; 1: the console; 2: a menu, its pointer then hovering and clicking; 4: "
                 "a demo's play bar; 0..7: the sum of those (5 the default)"},
      &c.mouse_release);
  con.cvars().bind({.name = "cl_yawspeed",
                    .fallback = "140",
                    .flags = console::kArchive,
                    .help = "How fast +left and +right turn, in degrees a second.",
                    .min = 0,
                    .max = 1000,
                    .values = "0..1000: that many degrees (140 the default)"},
                   &c.yaw_speed);
  con.cvars().bind(
      {.name = "cl_pitchspeed",
       .fallback = "150",
       .flags = console::kArchive,
       .help = "How fast +lookup, +lookdown and +klook's keys look, in degrees a second.",
       .min = 0,
       .max = 1000,
       .values = "0..1000: that many degrees (150 the default)"},
      &c.pitch_speed);
  con.cvars().bind({.name = "cl_anglespeedkey",
                    .fallback = "1.5",
                    .flags = console::kArchive,
                    .help = "How much faster the turn and look keys turn while +speed is held.",
                    .min = 0,
                    .max = 10,
                    .values = "0..10: that many times (1.5 the default)"},
                   &c.angle_speed_key);
  con.cvars().bind({.name = "freelook",
                    .fallback = "1",
                    .flags = console::kArchive,
                    .help = "Whether the mouse always looks up and down.",
                    .kind = console::CvarKind::boolean,
                    .values = "0: only while +mlook is held, else it moves you; 1: always (the "
                              "default)"},
                   &c.freelook);
  con.cvars().bind({.name = "m_forward",
                    .fallback = "1",
                    .flags = console::kArchive,
                    .help = "How far the mouse's up and down moves you when it does not look "
                            "(freelook 0, +mlook not held).",
                    .min = -10,
                    .max = 10,
                    .values = "-10..10: units a count at sensitivity 1 (1 the default)"},
                   &c.m_forward);
  con.cvars().bind({.name = "m_side",
                    .fallback = "0.8",
                    .flags = console::kArchive,
                    .help = "How far the mouse's sideways moves sidestep you when they do (+strafe "
                            "held, or lookstrafe with +mlook).",
                    .min = -10,
                    .max = 10,
                    .values = "-10..10: units a count at sensitivity 1 (0.8 the default)"},
                   &c.m_side);
  // The keys' speeds: what each asks the server for, walking; running times cl_movespeedkey.
  struct Speed {
    const char* name;
    const char* help;
    int* field;
  };
  const Speed speeds[] = {
      {"cl_forwardspeed", "The speed the forward key asks for, walking.", &c.forward_speed},
      {"cl_backspeed", "The speed the back key asks for, walking.", &c.back_speed},
      {"cl_sidespeed", "The speed the step keys ask for, walking.", &c.side_speed},
      {"cl_upspeed", "The speed swimming up or down asks for, walking.", &c.up_speed}};
  for (const Speed& sp : speeds)
    con.cvars().bind({.name = sp.name,
                      .fallback = "400",
                      .flags = console::kArchive,
                      .help = sp.help,
                      .kind = console::CvarKind::integer,
                      .min = 0,
                      .max = 2000,
                      .values = "0..2000: units a second, the server capping it (400 the "
                                "default)"},
                     sp.field);
  con.cvars().bind({.name = "cl_movespeedkey",
                    .fallback = "2",
                    .flags = console::kArchive,
                    .help = "How much faster the move keys ask to go running than walking.",
                    .min = 0,
                    .max = 10,
                    .values = "0..10: that many times (2 the default)"},
                   &c.move_speed_key);
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
  b.lookup = &con.add_button("lookup", "+lookup: look up while held (cl_pitchspeed)");
  b.lookdown = &con.add_button("lookdown", "+lookdown: look down while held (cl_pitchspeed)");
  b.klook = &con.add_button("klook", "+klook: the forward and back keys look up and down instead");
  b.mlook = &con.add_button("mlook", "+mlook: the mouse looks up and down while held (freelook 0)");
  // The classic games' use key: held, as a config binds it, but never sent; no classic game reads
  // it from a move.
  con.add_button("use", "+use: the classic use key; the classic games do nothing with it");
  for (const char* name : {"centerview", "force_centerview"})
    con.add_command(name, std::string(name) + ": level the view",
                    [&b](console::Console&, const console::Args&) { b.center = true; });
}

Moves button_moves(Controls& c) {
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
  m.lookup = down(b.lookup);
  m.lookdown = down(b.lookdown);
  m.klook = down(b.klook);
  m.mlook = down(b.mlook);
  m.center = b.center || (b.mlook_held && !m.mlook && c.lookspring && !c.freelook);
  c.buttons.mlook_held = m.mlook;
  c.buttons.center = false;
  m.impulse = b.impulse;
  c.buttons.impulse = 0;
  // A rocket jump helper's press: its request and the jump in this one command.
  for (console::Button* h : {b.rjump, b.rrjump}) {
    if (h == nullptr || !h->pressed) continue;
    h->pressed = false;
    m.impulse = h == b.rjump ? kRjumpImpulse : kBackRjumpImpulse;
    m.jump = true;
  }
  return m;
}

void release_buttons(Controls& c) {
  ControlButtons& b = c.buttons;
  for (console::Button* p : {b.forward, b.back, b.moveleft, b.moveright, b.moveup, b.movedown,
                             b.jump, b.attack, b.speed, b.strafe, b.left, b.right, b.attack2,
                             b.rjump, b.rrjump, b.lookup, b.lookdown, b.klook, b.mlook})
    if (p != nullptr) p->held = 0, p->pressed = false;
  b.impulse = 0;
}

Moves key_moves(const input::InputState& in) {
  Moves m;
  m.forward = held(in, kW);
  m.back = held(in, kS);
  m.moveright = held(in, kD);
  m.moveleft = held(in, kA);
  m.jump = held(in, kSpace);
  m.attack = in.mouse.buttons.down(0);
  for (int k = 0; k < 8; k++)
    if (input::key_pressed(in, static_cast<uint16_t>(kOne + k))) m.impulse = k + 1;
  if (in.mouse.wheel_y != 0) m.impulse = 10;
  return m;
}

void spring_view(const input::InputState& in, const Controls& c, bool on_ground, const MoveCmd& cmd,
                 float ideal, double dt, Vec3f& view) {
  if (!c.lookspring || !on_ground || in.mouse.dy != 0) return;
  // A player looking with the mouse (freelook, or +mlook held) aims where he points it: the
  // drift is for those who look by keys alone, as in every classic game.
  if (c.freelook || (c.buttons.mlook != nullptr && c.buttons.mlook->down())) return;
  if (cmd.forward == 0 && cmd.side == 0) return;
  const double most = kSpringSpeed * dt;
  const double gap = static_cast<double>(ideal) - view.x;
  view.x = static_cast<float>(view.x + std::clamp(gap, -most, most));
}

void drift_pitch(const Moves& m, const Controls& c, PitchDrift& d, bool on_ground, bool noclip,
                 const MoveCmd& cmd, float ideal, double dt, Vec3f& view) {
  if (c.freelook || m.mlook || m.lookup || m.lookdown) d.still = true;  // a hand on the pitch
  if (d.mlook_held && !m.mlook && c.lookspring) d.still = false;
  d.mlook_held = m.mlook;
  if (m.center) d.still = false;
  if (!on_ground || noclip) {
    d.run_s = 0;
    d.speed = 0;
    return;
  }
  if (d.still) {
    d.speed = 0;
    const int ahead = cmd.forward < 0 ? -cmd.forward : cmd.forward;
    d.run_s = ahead < c.forward_speed ? 0 : d.run_s + dt;
    if (d.run_s > c.center_move) d.still = false;
    return;
  }
  const double gap = static_cast<double>(ideal) - view.x;
  if (gap == 0) {
    d.speed = 0;
    return;
  }
  double step = d.speed * dt;
  d.speed += c.center_speed * dt;
  const double left = gap < 0 ? -gap : gap;
  if (step >= left) {
    step = left;
    d.speed = 0;
  }
  view.x = static_cast<float>(view.x + (gap > 0 ? step : -step));
}

AutoSens auto_sens(const Controls& c) {
  if (c.autosens == 0) return {};
  const double tuned = c.autosens > 90 && c.autosens <= 179 ? c.autosens : 90;
  const double lens = c.lens_fov != nullptr && *c.lens_fov > 0 ? *c.lens_fov : 90;
  // The height a lens across sees at 4:3, degrees.
  const auto high = [](double across) {
    constexpr double kDeg = 3.14159265358979323846 / 180;
    return 2 * core::libm::atan(core::libm::tan(across * kDeg / 2) * 0.75) / kDeg;
  };
  return {.x = lens / tuned, .y = high(lens) / high(tuned)};
}

double yaw_per_count(const Controls& c) {
  return c.sensitivity < 0.5 ? c.sensitivity : c.sensitivity * c.m_yaw;
}

double pitch_per_count(const Controls& c) {
  return c.sensitivity < 0.5 ? c.sensitivity : c.sensitivity * c.m_pitch;
}

void turn_view(const input::InputState& in, const Moves& m, const Controls& c, double dt,
               Vec3f& view) {
  const bool strafe = m.strafe || c.lookstrafe;
  // The mouse's up and down look with freelook or +mlook; else they move you (read_controls).
  const bool looks = c.freelook || m.mlook;
  const MouseTurn feel = feel_mouse(c.mouse, in.mouse.dx, in.mouse.dy, c.sensitivity, dt);
  // The old saves' degrees a count (a sensitivity under 0.5) are never shaped (feel_mouse).
  const double s = feel.sensitivity;
  const AutoSens lens = auto_sens(c);  // m_autosens
  const double yaw = (s < 0.5 ? s : s * c.m_yaw) * lens.x;
  const double pitch = (s < 0.5 ? s : s * c.m_pitch) * lens.y;
  const double dy = looks ? feel.dy * pitch * (c.invert ? -1 : 1) : 0;
  if (!strafe) view.y = static_cast<float>(view.y - feel.dx * yaw);
  const double keys = m.speed ? c.angle_speed_key : 1;  // +speed turns the keys faster
  if (!m.strafe) {  // +strafe makes the turn keys sidestep (read_controls)
    const double turn = c.yaw_speed * keys * dt * ((m.left ? 1 : 0) - (m.right ? 1 : 0));
    view.y = static_cast<float>(view.y + turn);
  }
  // +lookup and +lookdown, and the move keys under +klook (pitch down is positive).
  const int down_keys = (m.lookdown ? 1 : 0) - (m.lookup ? 1 : 0) +
                        (m.klook ? (m.back ? 1 : 0) - (m.forward ? 1 : 0) : 0);
  if (m.center) view.x = 0;
  view.x = static_cast<float>(view.x + c.pitch_speed * keys * dt * down_keys);
  view.x = std::clamp(static_cast<float>(view.x + dy), -c.pitch_limit, c.pitch_limit);
  while (view.y >= 360) view.y -= 360;
  while (view.y < 0) view.y += 360;
}

void turn_view(const input::InputState& in, const Controls& c, Vec3f& view) {
  turn_view(in, key_moves(in), c, 0, view);
}

MoveCmd read_controls(const Moves& m, const input::InputState& in, const Controls& c) {
  MoveCmd cmd;
  // cl_run on (the default): run, +speed (bound to no key) walks; off: walk, +speed runs. Running
  // asks for the walk's speeds times cl_movespeedkey; the server caps them. Nobody walks: no key
  // is bound to +speed.
  const bool run = c.run != m.speed;
  const auto speed = [&](int walk) {
    const double v = run ? walk * c.move_speed_key : walk;
    return static_cast<int>(std::clamp(v, -32767.0, 32767.0));
  };
  const int fs = speed(c.forward_speed), bs = speed(c.back_speed), ss = speed(c.side_speed);
  int f = 0, s = 0;
  if (m.forward && !m.klook) f += fs;  // +klook: the forward and back keys look (turn_view)
  if (m.back && !m.klook) f -= bs;
  if (m.moveright) s += ss;
  if (m.moveleft) s -= ss;
  if (m.strafe) {  // the turn keys sidestep
    if (m.right) s += ss;
    if (m.left) s -= ss;
  }
  if (m.strafe || c.lookstrafe) {
    const double side = in.mouse.dx * c.sensitivity * c.m_side;
    s = std::clamp(s + static_cast<int>(side), -ss, ss);
  }
  // freelook off and +mlook not held: the mouse's up and down move you.
  if (!c.freelook && !m.mlook && in.mouse.dy != 0) {
    const double fwd = -in.mouse.dy * c.sensitivity * c.m_forward;
    f = std::clamp(f + static_cast<int>(fwd), -32767, 32767);
  }
  // Up and down: swimming in the classic move (bound to no key; SPACE swims up); down crouches in
  // a move that ducks (Quake II's, plugins/classic/q2_move.hpp: up below 0 on the ground).
  const int us = speed(c.up_speed);
  const int u = (m.moveup ? us : 0) - (m.movedown ? us : 0);
  cmd.forward = static_cast<int16_t>(f);
  cmd.side = static_cast<int16_t>(s);
  cmd.up = static_cast<int16_t>(u);
  if (m.attack || m.attack2) cmd.buttons |= kButtonAttack;
  if (m.jump) cmd.buttons |= kButtonJump;
  cmd.impulse = static_cast<uint8_t>(m.impulse);
  return cmd;
}

MoveCmd read_controls(const input::InputState& in, const Controls& c) {
  return read_controls(key_moves(in), in, c);
}

void aim_turn_view(const Controls& c, AimTurn& turn, Vec3f& view, Vec3f eye, const Vec3f* target_at,
                   bool attack, double dt) {
  if (!c.aim_turn) {  // off: the turn held is given back now
    view.x -= turn.pitch;
    view.y -= turn.yaw;
    turn = {};
    return;
  }
  aim_turn_step(turn, view, eye, target_at, attack, static_cast<float>(dt));
}

}  // namespace ge::shooter
