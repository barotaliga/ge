// THE PLAYER'S HANDS: one frame's keys and mouse as the view's turn and the command the listen
// server runs. The keys are the console's binds (`bind w +forward`): `add_cvars` registers the
// moves as buttons (+forward, +back, +moveleft, +moveright, +jump, +attack, +speed, +strafe,
// +left, +right) and `impulse <n>`, and `default_binds` gives every key the classic layout has
// unbound (W A S D and the arrows move, SPACE jumps, the left button fires, SHIFT walks or runs,
// ALT sidesteps with the mouse, 1..8 pick a weapon, the wheel the next one carried), before the
// player's config rebinds them. OPTIONS ▸ CUSTOMIZE CONTROLS rebinds them too.
//
// - `button_moves` reads the buttons (the window); `key_moves` reads the default keys straight off
//   the input state (a scripted session, a test), so a session's frames make the same commands on
//   any machine whatever the player bound.
// - `read_controls` makes the command: forward at the run's speeds with `cl_run` on (the default)
//   and +speed held to walk, or the other way round with it off; the left button fires.
// - `turn_view`: raw mouse counts turn the view (`sensitivity` degrees a count, `m_invert` flips
//   the pitch), the pitch held within `pitch_limit`; +left and +right turn at `cl_yawspeed`; with
//   +strafe held (or `lookstrafe` on) the mouse's sideways counts sidestep instead of turning.
// - `lookspring`: while it is on, a player who moves on the ground without pitching the mouse has
//   his pitch drift to the program's ideal pitch (`spring_view`: level ground centres it, a slope
//   tilts it down or up the way he goes, at `kSpringSpeed` degrees a second).
//
// ge:summary The player's keys and mouse as the view's turn and the server's command
// ge:keywords controls keys mouse-look raw-mouse sensitivity invert wasd jump fire impulse
// ge:keywords weapon-keys wheel read_controls turn_view move-cmd m_invert cvars
// ge:keywords lookspring spring_view ideal-pitch centre-view cl_run always-run lookstrafe
// ge:keywords buttons binds default_binds forward attack speed strafe impulse key_moves
// ge:skill input shooter
// ge:test tests/shooters_edge_listen_test.cpp
#pragma once

#include "console/console.hpp"
#include "input/state.hpp"
#include "shooter/pmove.hpp"

namespace shooters_edge::client {

// The console's buttons the moves are read from (null until `add_cvars` registers them).
struct ControlButtons {
  ge::console::Button *forward = nullptr, *back = nullptr, *moveleft = nullptr,
                      *moveright = nullptr, *jump = nullptr, *attack = nullptr, *speed = nullptr,
                      *strafe = nullptr, *left = nullptr, *right = nullptr;
  int impulse = 0;  // the last `impulse <n>` not yet sent (0 none)
};

struct ControlSettings {
  double sensitivity = 0.066;  // degrees of turn a raw count
  bool invert = false;
  bool lookspring = false;
  bool run = true;          // cl_run: run without +speed (+speed walks)
  bool lookstrafe = false;  // the mouse's sideways counts sidestep, not turn
  double yaw_speed = 140;   // cl_yawspeed: +left and +right, degrees a second
  float pitch_limit = 89;   // degrees up or down
  int forward_speed = 400;  // units/s, running
  int side_speed = 350;
  int up_speed = 200;
  double strafe_scale = 36;  // units/s of sidestep a degree the mouse's counts would have turned
  ControlButtons buttons;
};

// One frame's moves: what is held, and the impulse asked for.
struct Moves {
  bool forward = false, back = false, moveleft = false, moveright = false;
  bool jump = false, attack = false, speed = false, strafe = false;
  bool left = false, right = false;  // turning
  int impulse = 0;
};

inline constexpr double kSpringSpeed = 90;  // degrees a second the lookspring drifts the pitch

// The cvars `sensitivity`, `m_invert`, `lookspring`, `cl_run`, `lookstrafe` and `cl_yawspeed`
// over `c`, the moves' buttons into `c.buttons` and the command `impulse` (`c` must outlive
// `con`).
void add_cvars(ge::console::Console& con, ControlSettings& c);
// Every move's default key bound where the key runs nothing yet (call before the configs run).
void default_binds(ge::console::Console& con);

// The moves the buttons hold; the impulse waiting is taken.
Moves button_moves(ControlSettings& c);
// Every button let go (the menu or the console took the keys).
void release_buttons(ControlSettings& c);
// The moves the default keys make, read off the input state: W A S D, SPACE, the left button,
// 1..8 and the wheel.
Moves key_moves(const ge::input::InputState& in);

// The view (degrees: pitch down positive, yaw) turned by this frame's mouse and turn keys over
// `dt` s.
void turn_view(const ge::input::InputState& in, const Moves& m, const ControlSettings& c, double dt,
               ge::shooter::Vec3f& view);
// The same with the default keys (key_moves) and no turn keys.
void turn_view(const ge::input::InputState& in, const ControlSettings& c, ge::shooter::Vec3f& view);

// The pitch drifted toward `ideal` (degrees) for `dt` s when `lookspring` is on, the player on
// the ground and moving (`cmd`), and the mouse has not pitched the view this frame.
void spring_view(const ge::input::InputState& in, const ControlSettings& c, bool on_ground,
                 const ge::shooter::MoveCmd& cmd, float ideal, double dt, ge::shooter::Vec3f& view);

// The command `m` makes (no length, no angles: the server's frame and the view give those); the
// mouse's sideways counts sidestep when +strafe or `lookstrafe` says so. An impulse is in
// `cmd.impulse` (0 none).
ge::shooter::MoveCmd read_controls(const Moves& m, const ge::input::InputState& in,
                                   const ControlSettings& c);
// The same with the default keys (key_moves).
ge::shooter::MoveCmd read_controls(const ge::input::InputState& in, const ControlSettings& c);

}  // namespace shooters_edge::client
